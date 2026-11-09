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
#include <chrono>
#include <cstdio>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "infrastructure/poly_interpolator.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "infrastructure/seq_fit_manifest.hpp"
#include "infrastructure/seq_fit_mcts_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"
#include "bench_gate.hpp"

namespace {

// a(j) = bases[j] for j < bases.size(), else `step` in i = j and p1, p2, p3 = a(j-1), a(j-2), a(j-3) (0 before the
// start); sampled at every index in [low, high]
struct seq_problem {
    std::string name;
    std::string step;
    std::vector<int> bases;
    int low;
    int high;
    std::vector<std::pair<size_t, int>> outliers;
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
    const poly_polynomial step = kernel.expander_.find_polynomial(kernel.parser_.parse(p.step)).value();
    std::vector<integer> terms;
    for (int index = 0; index <= p.high; ++index) {
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

template<typename Manifest>
outcome finish(Manifest& m, const std::vector<poly_sample>& samples, std::chrono::steady_clock::time_point start) {
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const expr* best = m.search_.best();
    bool sound = true;
    for (const pw_data_point& point : m.rule_set_.fit_.converter_.convert(samples)) {
        const std::optional<const expr*> value = m.rule_set_.pw_evaluator_.evaluate_at(best, point.input);
        sound = sound && value.has_value() && value.value() == point.output;
    }
    std::ostringstream os;
    m.kernel_.printer_.print(os, best);
    return outcome{.cost = m.search_.best_cost(), .best = os.str(), .seconds = seconds, .sound = sound};
}

outcome run_best_first(const std::vector<poly_sample>& samples) {
    const auto start = std::chrono::steady_clock::now();
    seq_fit_manifest m{"n", samples, k_poly_default_limits, k_poly_fit_default_limits, k_frontier};
    while (m.search_.rewrites() < k_rewrites && m.search_.next()) {}
    return finish(m, samples, start);
}

outcome run_mcts(const std::vector<poly_sample>& samples) {
    const auto start = std::chrono::steady_clock::now();
    seq_fit_mcts_manifest m{"n", samples, k_poly_default_limits, k_poly_fit_default_limits, k_probing_depth,
                            k_simulations, k_exploration, 1};
    while (m.search_.next()) {}
    return finish(m, samples, start);
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
    const bool skip_mcts = fast && samples.size() > k_large;
    const outcome skipped{.cost = 0, .best = "(skipped)", .seconds = 0, .sound = true};
    return solved{.points = samples.size(), .skip_mcts = skip_mcts, .best_first = run_best_first(samples),
                  .mcts = skip_mcts ? skipped : run_mcts(samples)};
}

}

int main(int argc, char** argv) {
    const bench_options options = parse_bench_options(argc, argv);
    const std::vector<seq_problem> problems{
        {"#7 powers of two", "2*p1", {1}, 0, 25, {}, 3, "exp(2, n)"},
        {"#6 Fibonacci", "p1 + p2", {0, 1}, 0, 25, {}, 7, "rec(p1 + p2, 0, 1, n)"},
        {"five times powers of 3", "3*p1", {5}, 0, 20, {}, 5, "5 * exp(3, n)"},
        {"powers of -2", "-2*p1", {1}, 0, 20, {}, 3, "exp(-2, n)"},
        {"Lucas numbers", "p1 + p2", {2, 1}, 0, 25, {}, 7, "rec(p1 + p2, 2, 1, n)"},
        {"Pell numbers", "2*p1 + p2", {0, 1}, 0, 20, {}, 9, "rec(2*p1 + p2, 0, 1, n)"},
        {"tribonacci", "p1 + p2 + p3", {0, 0, 1}, 0, 25, {}, 10, "rec(p1 + p2 + p3, 0, 0, 1, n)"},
        {"factorial", "i*p1", {1}, 0, 15, {}, 6, "rec(i*p1, 1, n)"},
        {"Fibonacci, wrong label", "p1 + p2", {0, 1}, 0, 25, {{10, 1}}, 12,
         "pw(case(point(10), 56), rec(p1 + p2, 0, 1, n))"},
        {"squares", "i^2", {0}, 0, 20, {}, 3, "n^2"},
        {"powers of two, long", "2*p1", {1}, 0, 60, {}, 3, "exp(2, n)"},
        {"Mersenne numbers", "2*p1 + 1", {0}, 0, 20, {}, 5, "exp(2, n) - 1"},
        // open: no solver reaches the target yet; kept so the gate reports when one does
        {"Fibonacci from index 1 (open)", "p1 + p2", {0, 1}, 1, 25, {}, 7, "rec(p1 + p2, 0, 1, n)"},
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
