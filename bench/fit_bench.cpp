// Data-fitting benchmark: integer samples of polynomials and piecewise polynomials, some with outliers, under the
// best-first and MCTS fit solvers. A problem is solved when the result is no larger than a known compact form (its
// cost is given, since piecewise forms do not parse). Every result is checked by evaluating it at every sample.
//
//   fit_bench [--fast] [--serial] [--write FILE] [--check FILE]
//
// --fast skips MCTS on the large data sets; --write saves the results, --check gates them against a baseline
// (bench/bench_gate.hpp).
// Problems are solved in parallel, one per thread; --serial solves them one at a time, for true timings.
#include <chrono>
#include <climits>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>
#include "infrastructure/poly_fit_manifest.hpp"
#include "infrastructure/poly_fit_mcts_manifest.hpp"
#include "infrastructure/poly_interpolator.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"
#include "bench_gate.hpp"

namespace {

// `text` gives the output while the split variable is below `below`
struct piece {
    std::string text;
    int below;
};

struct fit_problem {
    std::string name;
    std::vector<std::string> variables;
    std::vector<piece> pieces;
    size_t split;
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

// Every point of the grid [low, high]^k, in order, valued by the piece its split coordinate falls in.
std::vector<poly_sample> sample(const fit_problem& p) {
    poly_kernel_manifest kernel{k_poly_default_limits};
    std::vector<uint32_t> ids;
    for (const std::string& variable : p.variables)
        ids.push_back(kernel.symbols_.intern_symbol(variable));
    const poly_interpolator evaluator{ids, k_poly_fit_default_max_unknowns};
    std::vector<poly_polynomial> polynomials;
    for (const piece& part : p.pieces)
        polynomials.push_back(kernel.expander_.find_polynomial(kernel.parser_.parse(part.text)).value());
    std::vector<poly_sample> samples;
    std::vector<int> coordinates(p.variables.size(), p.low);
    while (true) {
        poly_sample s{.inputs = {}, .output = 0};
        for (const int coordinate : coordinates)
            s.inputs.push_back(coordinate);
        size_t part = 0;
        while (coordinates[p.split] >= p.pieces[part].below)
            ++part;
        s.output = evaluator.evaluate(polynomials[part], s.inputs);
        samples.push_back(s);
        size_t position = 0;
        while (position < coordinates.size() && ++coordinates[position] > p.high) {
            coordinates[position] = p.low;
            ++position;
        }
        if (position == coordinates.size())
            break;
    }
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

outcome run_best_first(const fit_problem& p, const std::vector<poly_sample>& samples) {
    const auto start = std::chrono::steady_clock::now();
    poly_fit_manifest m{p.variables.size(), samples, k_poly_default_limits, k_poly_fit_default_limits, k_frontier};
    while (m.search_.rewrites() < k_rewrites && m.search_.next()) {}
    return finish(m, samples, start);
}

outcome run_mcts(const fit_problem& p, const std::vector<poly_sample>& samples) {
    const auto start = std::chrono::steady_clock::now();
    poly_fit_mcts_manifest m{p.variables.size(), samples, k_poly_default_limits, k_poly_fit_default_limits,
                             k_probing_depth, k_simulations, k_exploration, 1};
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

solved solve(const fit_problem& p, bool fast) {
    const std::vector<poly_sample> samples = sample(p);
    const bool skip_mcts = fast && samples.size() > k_large;
    const outcome skipped{.cost = 0, .best = "(skipped)", .seconds = 0, .sound = true};
    return solved{.points = samples.size(), .skip_mcts = skip_mcts, .best_first = run_best_first(p, samples),
                  .mcts = skip_mcts ? skipped : run_mcts(p, samples)};
}

}

int main(int argc, char** argv) {
    const bench_options options = parse_bench_options(argc, argv);
    const std::vector<fit_problem> problems{
        {"cubic", {"x"}, {{"x^3 - x", INT_MAX}}, 0, -3, 4, {}, 6, "lam(#0^3 - #0)"},
        {"power of linear", {"x"}, {{"(2*x - 1)^3", INT_MAX}}, 0, -3, 4, {}, 8, "lam((2*#0 - 1)^3)"},
        {"horner quintic", {"x"}, {{"x^5 + 2*x^4 + 3*x^3 + 4*x^2 + 5*x + 6", INT_MAX}}, 0, -3, 5, {}, 20,
         "lam(((((#0+2)*#0+3)*#0+4)*#0+5)*#0+6)"},
        {"one outlier", {"x"}, {{"x^2 + 1", INT_MAX}}, 0, 0, 8, {{3, 7}}, 11, "pw(case(point(3), 17), lam(#0^2 + 1))"},
        {"outlier run", {"x"}, {{"x^2 - 4*x", INT_MAX}}, 0, 0, 39,
         {{0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1}, {8, 1}, {9, 1}, {10, 1}, {11, 1}}, 102,
         "12 cases + #0^2 - 4*#0"},
        {"scattered outliers", {"x"}, {{"x^2 + 1", INT_MAX}}, 0, 0, 39, {{3, 5}, {10, 5}, {17, 5}, {24, 5}, {31, 5}, {38, 5}},
         31, "6 cases + #0^2 + 1"},
        {"cubic 1000", {"x"}, {{"x^3 - 2*x + 5", INT_MAX}}, 0, -500, 499, {}, 10, "lam(#0^3 - 2*#0 + 5)"},
        {"two-variable square", {"x", "y"}, {{"(x + y)^2", INT_MAX}}, 0, -2, 2, {}, 7, "lams(2, (#1 + #0)^2)"},
        {"two-variable product", {"x", "y"}, {{"(x - y + 2)*(x + 2*y)", INT_MAX}}, 0, -2, 2, {{4, 3}}, 19,
         "pw(case(point(2, -2), -9), lams(2, (#1 - #0 + 2)*(#1 + 2*#0)))"},
        {"three-variable cube", {"x", "y", "z"}, {{"(x + y + z)^3", INT_MAX}}, 0, -1, 2, {}, 9, "lams(3, (#2 + #1 + #0)^3)"},
        {"three-variable mixed", {"x", "y", "z"}, {{"x*y*z + x^2 - z", INT_MAX}}, 0, -1, 2, {}, 13, "lams(3, #2*#1*#0 + #2^2 - #0)"},
        {"two pieces", {"x"}, {{"x^2", 0}, {"3*x + 1", INT_MAX}}, 0, -10, 9, {}, 14,
         "lam(pw(when(lt(#0, 0), #0^2), 3*#0 + 1))"},
        {"two pieces 400", {"x"}, {{"x^2", 0}, {"3*x + 1", INT_MAX}}, 0, -200, 199, {}, 14,
         "lam(pw(when(lt(#0, 0), #0^2), 3*#0 + 1))"},
        {"absolute value", {"x"}, {{"-1*x", 0}, {"x", INT_MAX}}, 0, -10, 10, {}, 10, "lam(pw(when(lt(#0, 1), -1*#0), #0))"},
        {"three pieces", {"x"}, {{"5", 10}, {"x^2 - 7", 20}, {"(x - 1)^3", INT_MAX}}, 0, 0, 29, {}, 21,
         "lam(pw(when(lt(#0, 10), 5), when(lt(#0, 20), #0^2 - 7), (#0 - 1)^3))"},
        {"pieces with outlier", {"x"}, {{"x^2", 0}, {"3*x + 1", INT_MAX}}, 0, -10, 9, {{4, 3}}, 19,
         "pw(case(point(-6), 39), lam(pw(when(lt(#0, 0), #0^2), 3*#0 + 1)))"},
        {"split on x", {"x", "y"}, {{"x*y", 1}, {"x + y^2", INT_MAX}}, 0, -3, 3, {}, 15, "lams(2, pw(when(lt(#1, 1), #1*#0), #0^2 + #1))"},
        {"split on y", {"x", "y"}, {{"x - y", 0}, {"(x + y)^2", INT_MAX}}, 1, -3, 3, {}, 15,
         "lams(2, pw(when(lt(#0, 0), #1 - #0), (#1 + #0)^2))"},
        {"sign flip at zero", {"x", "y"}, {{"x - y", 0}, {"x + y", INT_MAX}}, 0, -3, 3, {}, 13,
         "pw(when(lt(x, 0), x - y), x + y)"},
        // reduce request #8, and similar iterated polynomials, read with a helper by the iteration detector
        {"#8 g(g(g(x))), g = y^2 + 1", {"x"}, {{"x^8 + 4*x^6 + 8*x^4 + 8*x^2 + 5", INT_MAX}}, 0, -6, 6, {}, 15,
         "lam(let(lam(#0^2 + 1), app(#0, app(#0, app(#0, #1)))))"},
        {"g(g(x)), g = y^3 - y", {"x"}, {{"(x^3 - x)^3 - (x^3 - x)", INT_MAX}}, 0, -6, 6, {}, 13,
         "lam(let(lam(#0^3 - #0), app(#0, app(#0, #1))))"},
        {"g(g(g(x))), g = y^2 + y", {"x"}, {{"((x^2 + x)^2 + x^2 + x)^2 + (x^2 + x)^2 + x^2 + x", INT_MAX}}, 0, -6, 6, {},
         15, "lam(let(lam(#0^2 + #0), app(#0, app(#0, app(#0, #1)))))"},
        {"g(g(x)), g = y^2 - 2", {"x"}, {{"(x^2 - 2)^2 - 2", INT_MAX}}, 0, -6, 6, {}, 10,
         "(x^2 - 2)^2 - 2 (smaller than the helper form, 12)"},
        // every third point an outlier: once open, now read as a period-3 piecewise by the residue detector
        {"dense outliers", {"x"}, {{"x^2 + 1", INT_MAX}}, 0, 0, 29,
         {{1, 2}, {4, 2}, {7, 2}, {10, 2}, {13, 2}, {16, 2}, {19, 2}, {22, 2}, {25, 2}, {28, 2}}, 47,
         "10 cases + #0^2 + 1"},
    };

    std::printf("budgets: best-first %zu rewrites; mcts %zu simulations x depth %zu\n", k_rewrites, k_simulations,
                k_probing_depth);
    std::printf("cost  *: no larger than target   !: UNSOUND (misses a sample)\n\n");
    std::printf("%-24s %6s %6s | %-15s | %-15s\n", "problem", "points", "target", "best-1st", "mcts");
    size_t solved_counts[2] = {0, 0};
    std::vector<bench_result> gated;
    std::vector<std::string> notes;
    const std::vector<solved> runs = solve_in_parallel(problems, solve, options);
    for (size_t index = 0; index < problems.size(); ++index) {
        const fit_problem& p = problems[index];
        const bool skip_mcts = runs[index].skip_mcts;
        const outcome& best_first = runs[index].best_first;
        const outcome& mcts = runs[index].mcts;
        solved_counts[0] += best_first.cost <= p.target ? 1 : 0;
        solved_counts[1] += !skip_mcts && mcts.cost <= p.target ? 1 : 0;
        std::printf("%-24s %6zu %6zu | %s | %s\n", p.name.c_str(), runs[index].points, p.target,
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
