// Sequence benchmark: integer sequences given by a recurrence (closed forms included, as recurrences of order one),
// some with wrong labels, under the best-first and MCTS sequence solvers. A problem is solved when the result is no
// larger than a known compact form (its cost is given, since recurrences do not parse). Every result is checked by
// evaluating it at every sample.
//
//   seq_bench [--fast] [--serial] [--write FILE] [--check FILE]
//
// --fast skips MCTS on the large data sets; --write saves the results, --check gates them against a baseline
// (bench/bench_gate.hpp).
// Problems are solved in parallel, one per thread; --serial solves them one at a time, for true timings.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "infrastructure/num_prime_table.hpp"
#include "infrastructure/poly_interpolator.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "infrastructure/seq_fit_manifest.hpp"
#include "infrastructure/seq_fit_mcts_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"
#include "bench_gate.hpp"

namespace {

// a(j) = bases[j] for j < bases.size(), else `step` in i = j and p1, p2, p3 = a(j-1), a(j-2), a(j-3) (0 before the
// start), or a(j) = prime(j) or is_prime(j) when the step is "prime(i)" or "is_prime(i)"; sampled at every index in
// [low, high]
struct seq_problem {
    std::string name;
    std::string step;
    std::vector<int> bases;
    int low;
    int high;
    std::vector<std::pair<size_t, int>> outliers;
    std::vector<std::pair<size_t, int>> holes;
    size_t target;
    std::string target_text;
};

struct outcome {
    size_t cost;
    std::string best;
    double seconds;
    bool sound;
};

constexpr size_t k_rewrites = 3000;
constexpr size_t k_frontier = 20000;
constexpr size_t k_probing_depth = 12;
constexpr size_t k_simulations = 150;
constexpr double k_exploration = 0.4;
constexpr size_t k_large = 40;  // samples; --fast skips MCTS above this

std::vector<poly_sample> sample(const seq_problem& p) {
    poly_kernel_manifest kernel{k_poly_default_limits};
    const std::vector<uint32_t> ids{kernel.symbols_.intern_symbol("i"), kernel.symbols_.intern_symbol("p1"),
                                    kernel.symbols_.intern_symbol("p2"), kernel.symbols_.intern_symbol("p3")};
    const poly_interpolator evaluator{ids, k_poly_fit_default_max_unknowns};
    // a number-theory primitive as the step names the sequence itself, read off the prime table
    num_prime_table table{k_num_default_max_value};
    const bool primes = p.step == "prime(i)";
    const bool primality = p.step == "is_prime(i)";
    const poly_polynomial step = primes || primality
                                     ? poly_polynomial{}
                                     : kernel.expander_.find_polynomial(kernel.parser_.parse(p.step)).value();
    std::vector<integer> terms;
    for (int index = 0; index <= p.high; ++index) {
        if (primes || primality) {
            terms.push_back(primes ? table.find_nth_prime(index).value()
                                   : integer(table.find_primality(index).value() ? 1 : 0));
            continue;
        }
        if (static_cast<size_t>(index) < p.bases.size()) {
            terms.push_back(p.bases[index]);
            continue;
        }
        std::vector<integer> inputs{index};
        for (int back = 1; back <= 3; ++back)
            inputs.push_back(back <= index ? terms[index - back] : integer(0));
        terms.push_back(evaluator.evaluate(step, inputs));
    }
    std::vector<poly_sample> samples;
    for (int index = p.low; index <= p.high; ++index)
        samples.push_back(poly_sample{.inputs = {index}, .output = terms[index]});
    for (const auto& [index, offset] : p.outliers)
        samples[index].output += offset;
    return samples;
}

// The samples as data points, each hole's label replaced by `?k` (k from 1, in order), or `coefficient * ?k`.
template<typename Manifest>
std::vector<pw_data_point> find_points(Manifest& m, const seq_problem& p, const std::vector<poly_sample>& samples) {
    std::vector<pw_data_point> points = m.rule_set_.fit_.converter_.convert(samples);
    uint32_t next = 1;
    for (const auto& [index, coefficient] : p.holes) {
        const expr* hole = m.kernel_.exprs_.make_var(next++);
        const expr* scale = m.kernel_.literals_.make_literal(coefficient);
        points[index].output =
            coefficient == 1 ? hole : m.kernel_.exprs_.make_functor(m.kernel_.poly_symbols_.mul, {scale, hole});
    }
    return points;
}

// Sound when every point's label matches the result's value there, under one binding of the holes, or the result
// keeps the point's own case (a label holding a hole has no value).
template<typename Manifest, typename Search>
outcome finish(Manifest& m, Search& search, const std::vector<pw_data_point>& points,
               std::chrono::steady_clock::time_point start) {
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const expr* best = search.best();
    const expr::functor& top = std::get<expr::functor>(best->content);
    bool sound = true;
    std::map<uint32_t, const expr*> bindings;
    for (const pw_data_point& point : points) {
        const expr* own = m.kernel_.exprs_.make_functor(m.rule_set_.pw_symbols_.branch, {point.input, point.output});
        const bool kept = top.id == m.rule_set_.pw_symbols_.piecewise
                          && std::find(top.args.begin(), top.args.end(), own) != top.args.end();
        if (kept)
            continue;
        const std::optional<const expr*> value = m.rule_set_.pw_evaluator_.evaluate_at(best, point.input);
        sound = sound && value.has_value() && m.rule_set_.matcher_.match(point.output, value.value(), bindings);
    }
    std::ostringstream os;
    m.kernel_.printer_.print(os, best);
    return outcome{.cost = search.best_cost(), .best = os.str(), .seconds = seconds, .sound = sound};
}

// A problem with holes starts from its own piecewise, with a search of its own.
outcome run_best_first(const seq_problem& p, const std::vector<poly_sample>& samples) {
    const auto start = std::chrono::steady_clock::now();
    seq_fit_manifest m{samples, k_poly_default_limits, k_poly_fit_default_limits, k_frontier};
    const std::vector<pw_data_point> points = find_points(m, p, samples);
    if (p.holes.empty()) {
        while (m.search_.rewrites() < k_rewrites && m.search_.next()) {}
        return finish(m, m.search_, points, start);
    }
    seq_fit_manifest::search_t search{m.rule_set_.rules_, m.kernel_.sizes_, m.rule_set_.builder_.build(points, 0), k_frontier};
    while (search.rewrites() < k_rewrites && search.next()) {}
    return finish(m, search, points, start);
}

outcome run_mcts(const std::vector<poly_sample>& samples) {
    const auto start = std::chrono::steady_clock::now();
    seq_fit_mcts_manifest m{samples, k_poly_default_limits, k_poly_fit_default_limits, k_probing_depth,
                            k_simulations, k_exploration, 1};
    while (m.search_.next()) {}
    return finish(m, m.search_, m.rule_set_.fit_.converter_.convert(samples), start);
}

std::string cell(const outcome& o, size_t target) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%5zu%s %6.2fs", o.cost, !o.sound ? "!" : o.cost <= target ? "*" : " ",
                  o.seconds);
    return buffer;
}

// one problem under both solvers
struct solved {
    size_t points;
    bool skip_mcts;
    outcome best_first;
    outcome mcts;
};

solved solve(const seq_problem& p, bool fast) {
    const std::vector<poly_sample> samples = sample(p);
    const bool skip_mcts = (fast && samples.size() > k_large) || !p.holes.empty();
    const outcome skipped{.cost = 0, .best = "(skipped)", .seconds = 0, .sound = true};
    return solved{.points = samples.size(), .skip_mcts = skip_mcts, .best_first = run_best_first(p, samples),
                  .mcts = skip_mcts ? skipped : run_mcts(samples)};
}

}

int main(int argc, char** argv) {
    const bench_options options = parse_bench_options(argc, argv);
    const std::vector<seq_problem> problems{
        {"#7 powers of two", "2*p1", {1}, 0, 25, {}, {}, 4, "lam(exp(2, #0))"},
        {"#6 Fibonacci", "p1 + p2", {0, 1}, 0, 25, {}, {}, 10, "lam(rec(lams(3, #1 + #2), 0, 1, #0))"},
        {"five times powers of 3", "3*p1", {5}, 0, 20, {}, {}, 6, "lam(5 * exp(3, #0))"},
        {"powers of -2", "-2*p1", {1}, 0, 20, {}, {}, 4, "lam(exp(-2, #0))"},
        {"Lucas numbers", "p1 + p2", {2, 1}, 0, 25, {}, {}, 10, "lam(rec(lams(3, #1 + #2), 2, 1, #0))"},
        {"Pell numbers", "2*p1 + p2", {0, 1}, 0, 20, {}, {}, 12, "lam(rec(lams(3, 2*#1 + #2), 0, 1, #0))"},
        {"tribonacci", "p1 + p2 + p3", {0, 0, 1}, 0, 25, {}, {}, 13, "lam(rec(lams(4, #1 + #2 + #3), 0, 0, 1, #0))"},
        {"factorial", "i*p1", {1}, 0, 15, {}, {}, 9, "lam(rec(lam(lam(#0*#1)), 1, #0))"},
        {"Fibonacci, wrong label", "p1 + p2", {0, 1}, 0, 25, {{10, 1}}, {}, 15,
         "pw(case(point(10), 56), lam(rec(lams(3, #1 + #2), 0, 1, #0)))"},
        {"squares", "i^2", {0}, 0, 20, {}, {}, 4, "lam(#0^2)"},
        {"powers of two, long", "2*p1", {1}, 0, 60, {}, {}, 4, "lam(exp(2, #0))"},
        {"Mersenne numbers", "2*p1 + 1", {0}, 0, 20, {}, {}, 6, "exp(2, n) - 1"},
        // reduce request #15, and similar sequences with unknown terms: a hole is bound to the model's value
        {"#15 Fibonacci, unknown term", "p1 + p2", {0, 1}, 0, 20, {}, {{7, 1}}, 10,
         "lam(rec(lams(3, #1 + #2), 0, 1, #0)), ?1 = 13"},
        {"Pell numbers, two unknown terms", "2*p1 + p2", {0, 1}, 0, 20, {}, {{5, 1}, {12, 1}}, 12,
         "lam(rec(lams(3, 2*#1 + #2), 0, 1, #0))"},
        {"powers of two, scaled unknown", "2*p1", {1}, 0, 25, {}, {{10, 4}}, 4, "lam(exp(2, #0)), ?1 = 256"},
        // reduce request #11, and similar sequences observed from the middle: the bases are solved by running the step
        // backwards from the first observed terms
        {"#11 Fibonacci from index 10", "p1 + p2", {0, 1}, 10, 35, {}, {}, 10, "lam(rec(lams(3, #1 + #2), 0, 1, #0))"},
        {"Fibonacci from index 1", "p1 + p2", {0, 1}, 1, 25, {}, {}, 10, "lam(rec(lams(3, #1 + #2), 0, 1, #0))"},
        {"Pell numbers from index 6", "2*p1 + p2", {0, 1}, 6, 26, {}, {}, 12,
         "lam(rec(lams(3, 2*#1 + #2), 0, 1, #0))"},
        {"tribonacci from index 5", "p1 + p2 + p3", {0, 0, 1}, 5, 30, {}, {}, 13,
         "lam(rec(lams(4, #1 + #2 + #3), 0, 0, 1, #0))"},
        {"factorial from index 4", "i*p1", {1}, 4, 18, {}, {}, 9, "lam(rec(lam(lam(#0*#1)), 1, #0))"},
        {"powers of 3 from index 8", "3*p1", {1}, 8, 25, {}, {}, 4, "lam(exp(3, #0))"},
        // reduce requests #12 and #13, and similar: the primes and primality as primitives
        {"#12 the n-th prime", "prime(i)", {}, 0, 30, {}, {}, 3, "lam(prime(#0))"},
        {"#13 is n prime", "is_prime(i)", {}, 0, 60, {}, {}, 3, "lam(is_prime(#0))"},
        {"the n-th prime from index 20", "prime(i)", {}, 20, 60, {}, {}, 3, "lam(prime(#0))"},
        {"is n prime, two wrong labels", "is_prime(i)", {}, 0, 60, {{9, 1}, {13, -1}}, {}, 12,
         "pw(case(point(9), 1), case(point(13), 0), lam(is_prime(#0)))"},
        {"p1^2 + 1 from index 2 (not invertible)", "p1^2 + 1", {1}, 2, 8, {}, {}, 27,
         "no recurrence: the step has no inverse, so the data stays as cases"},
        // showcase (docs/showcase.md): realistic sequences, and variants measuring how far each generalizes
        {"showcase: bacteria, triple then remove 2", "3*p1 - 2", {5}, 0, 20, {}, {}, 8, "lam(4 * exp(3, #0) + 1)"},
        {"showcase: bacteria, two miscounts", "3*p1 - 2", {5}, 0, 20, {{4, 1}, {11, -3}}, {}, 17,
         "two cases + the closed form"},
        {"showcase: savings, double plus 100", "2*p1 + 100", {0}, 0, 30, {}, {}, 8, "lam(100 * exp(2, #0) - 100)"},
        {"showcase: savings, from year 5", "2*p1 + 100", {0}, 5, 30, {}, {}, 8, "lam(100 * exp(2, #0) - 100)"},
        {"showcase: staircase, steps of 1 to 3", "p1 + p2 + p3", {1, 1, 2}, 0, 25, {}, {}, 13,
         "lam(rec(lams(4, #1 + #2 + #3), 1, 1, 2, #0))"},
        {"showcase: staircase, from step 6", "p1 + p2 + p3", {1, 1, 2}, 6, 30, {}, {}, 13,
         "lam(rec(lams(4, #1 + #2 + #3), 1, 1, 2, #0))"},
        {"showcase: rabbits from month 12, one unrecorded", "p1 + p2", {0, 1}, 12, 40, {}, {{5, 1}}, 10,
         "lam(rec(lams(3, #1 + #2), 0, 1, #0))"},
        {"showcase: prime-numbered lockers 50 to 120", "prime(i)", {}, 50, 120, {}, {}, 3, "lam(prime(#0))"},
        {"showcase: arrangements of n guests", "i*p1", {1}, 0, 20, {}, {}, 9, "lam(rec(lam(lam(#0 * #1)), 1, #0))"},
        {"showcase: rabbits from month 12, three unrecorded", "p1 + p2", {0, 1}, 12, 40, {}, {{3, 1}, {9, 1}, {20, 1}}, 10,
         "lam(rec(lams(3, #1 + #2), 0, 1, #0))"},
        {"showcase: savings at triple plus 100, from year 5", "3*p1 + 100", {0}, 5, 25, {}, {}, 8,
         "lam(50 * exp(3, #0) - 50)"},
        {"showcase: staircase from step 6, a miscount", "p1 + p2 + p3", {1, 1, 2}, 6, 30, {{8, 1}}, {}, 18,
         "one case + the staircase"},
    };

    std::printf("budgets: best-first %zu rewrites; mcts %zu simulations x depth %zu\n", k_rewrites, k_simulations,
                k_probing_depth);
    std::printf("cost  *: no larger than target   !: UNSOUND (misses a sample)\n\n");
    std::printf("%-30s %6s %6s | %-15s | %-15s\n", "problem", "points", "target", "best-1st", "mcts");
    size_t solved_counts[2] = {0, 0};
    std::vector<bench_result> gated;
    std::vector<std::string> notes;
    const std::vector<solved> runs = solve_in_parallel(problems, solve, options);
    for (size_t index = 0; index < problems.size(); ++index) {
        const seq_problem& p = problems[index];
        const bool skip_mcts = runs[index].skip_mcts;
        const outcome& best_first = runs[index].best_first;
        const outcome& mcts = runs[index].mcts;
        solved_counts[0] += best_first.cost <= p.target ? 1 : 0;
        solved_counts[1] += !skip_mcts && mcts.cost <= p.target ? 1 : 0;
        std::printf("%-30s %6zu %6zu | %s | %s\n", p.name.c_str(), runs[index].points, p.target,
                    cell(best_first, p.target).c_str(), skip_mcts ? "      (skipped)" : cell(mcts, p.target).c_str());
        std::fflush(stdout);
        gated.push_back(bench_result{.problem = p.name, .solver = "best-1st", .cost = best_first.cost,
                                     .target = p.target, .seconds = best_first.seconds, .sound = best_first.sound});
        if (!skip_mcts)
            gated.push_back(bench_result{.problem = p.name, .solver = "mcts", .cost = mcts.cost, .target = p.target,
                                         .seconds = mcts.seconds, .sound = mcts.sound});
        notes.push_back(p.name + "\n    target:   " + p.target_text + "\n    best-1st: " + best_first.best.substr(0, 160)
                        + "\n    mcts:     " + mcts.best.substr(0, 160));
    }
    std::printf("\nsolved (of %zu): best-1st %zu, mcts %zu\n\n", problems.size(), solved_counts[0], solved_counts[1]);
    for (const std::string& note : notes)
        std::printf("%s\n", note.c_str());
    return finish_bench(options, gated);
}
