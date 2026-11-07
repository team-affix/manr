// Polynomial reduction benchmark: every problem under four solvers (full rules or low-level only, best-first or
// MCTS) with comparable budgets. A problem is solved when the result is no larger than a known compact form.
// Every result is checked against the input's normal form.
//
//   poly_bench [--fast] [--write FILE] [--check FILE]
//
// --fast skips MCTS on the wide problems (minutes each); --write saves the results, --check gates them against a
// baseline (bench/bench_gate.hpp).
#include <chrono>
#include <cstdio>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include "infrastructure/poly_baseline_manifest.hpp"
#include "infrastructure/poly_baseline_mcts_manifest.hpp"
#include "infrastructure/poly_reduce_manifest.hpp"
#include "infrastructure/poly_reduce_mcts_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "bench_gate.hpp"

namespace {

struct problem {
    std::string name;
    std::string text;
    std::string target;
};

struct outcome {
    size_t cost;
    std::string best;
    double seconds;
    bool sound;
};

constexpr size_t k_rewrites = 5000;
constexpr size_t k_frontier = 20000;
constexpr size_t k_probing_depth = 12;
constexpr size_t k_simulations = 150;
constexpr double k_exploration = 0.4;

std::string print(poly_kernel_manifest& kernel, const expr* e) {
    std::ostringstream os;
    kernel.printer_.print(os, e);
    return os.str();
}

template<typename Manifest>
outcome finish(Manifest& m, const expr* best, std::chrono::steady_clock::time_point start) {
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const bool sound = m.kernel_.expander_.find_polynomial(best) == m.kernel_.expander_.find_polynomial(m.initial_);
    return outcome{.cost = m.search_.best_cost(), .best = print(m.kernel_, best), .seconds = seconds, .sound = sound};
}

template<typename Manifest>
outcome run_best_first(const std::string& text) {
    const auto start = std::chrono::steady_clock::now();
    Manifest m{text, k_poly_default_limits, k_frontier};
    while (m.search_.rewrites() < k_rewrites && m.search_.next()) {}
    return finish(m, m.search_.best(), start);
}

template<typename Manifest>
outcome run_mcts(const std::string& text) {
    const auto start = std::chrono::steady_clock::now();
    Manifest m{text, k_poly_default_limits, k_probing_depth, k_simulations, k_exploration, 1};
    while (m.search_.next()) {}
    return finish(m, m.search_.best(), start);
}

std::string random_linear(std::mt19937& rng, const std::vector<std::string>& variables) {
    std::string text;
    for (const std::string& v : variables) {
        if (rng() % 3 == 0 && !text.empty())
            continue;
        const int coefficient = static_cast<int>(rng() % 3) + 1;
        const bool negative = !text.empty() && rng() % 2 == 0;
        text += text.empty() ? "" : (negative ? " - " : " + ");
        text += coefficient == 1 ? v : std::to_string(coefficient) + "*" + v;
    }
    const int constant = static_cast<int>(rng() % 7) - 3;
    if (constant != 0)
        text += (constant < 0 ? " - " : " + ") + std::to_string(constant < 0 ? -constant : constant);
    return "(" + text + ")";
}

// A compact target built from linear forms, and the same polynomial fully expanded as the problem.
// Uses between `fewest` and all of `pool`'s variables.
problem random_problem(std::mt19937& rng, const std::string& name, const std::vector<std::string>& pool, size_t fewest,
                       poly_kernel_manifest& kernel) {
    const size_t count = fewest + rng() % (pool.size() - fewest + 1);
    std::vector<std::string> variables(pool.begin(), pool.begin() + count);
    std::string target;
    switch (rng() % 4) {
        case 0: target = random_linear(rng, variables) + "^" + std::to_string(2 + rng() % 3); break;
        case 1: target = random_linear(rng, variables) + " * " + random_linear(rng, variables) + " * "
                       + random_linear(rng, variables); break;
        case 2: target = random_linear(rng, variables) + "^2 * " + random_linear(rng, variables); break;
        default: target = random_linear(rng, {variables.front()}) + "^3 + " + random_linear(rng, {"w"}) + "^2"; break;
    }
    const expr* expanded = kernel.renderer_.render(kernel.expander_.find_polynomial(kernel.parser_.parse(target)).value());
    return problem{.name = name, .text = print(kernel, expanded), .target = target};
}

std::string cell(const outcome& o, size_t target) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%4zu%s %6.2fs", o.cost, !o.sound ? "!" : o.cost <= target ? "*" : " ",
                  o.seconds);
    return buffer;
}

}

int main(int argc, char** argv) {
    const bench_options options = parse_bench_options(argc, argv);
    std::vector<problem> problems{
        {"identity", "x + 0", "x"},
        {"like terms", "2*x + 3*x", "5*x"},
        {"power fold", "x*x*x", "x^3"},
        {"hidden cancellation", "(x+1)^2 - (x-1)^2", "4*x"},
        {"distributivity", "a*b + a*c", "a*(b+c)"},
        {"square of sum", "x^2 + 2*x*y + y^2", "(x+y)^2"},
        {"cube of sum", "x^3 + 3*x^2*y + 3*x*y^2 + y^3", "(x+y)^3"},
        {"difference of squares", "(a+b)*(a-b) + b^2", "a^2"},
        {"cubic roots", "x^3 + 6*x^2 + 11*x + 6", "(x+1)*(x+2)*(x+3)"},
        {"two squares", "x^2 + 2*x + 1 + y^2 + 2*y + 1", "(x+1)^2 + (y+1)^2"},
        {"content", "6*x^2*y + 9*x*y^2", "3*x*y*(2*x+3*y)"},
        {"two-variable factor", "x^2 - y^2 + x + y", "(x+y)*(x-y+1)"},
        {"quartic difference", "(x+y)^4 - (x-y)^4", "8*x*y*(x^2+y^2)"},
        {"expanded cubic times linear", "x^4 + 5*x^3*y + 6*x^2*y^2 - 4*x*y^3 - 8*y^4", "(x+2*y)^3*(x-y)"},
        {"trinomial square", "a^2 + b^2 + c^2 + 2*a*b + 2*a*c + 2*b*c", "(a+b+c)^2"},
        {"cancelling quartic", "x^4 + 4*x^3 + 6*x^2 + 4*x + 1 - (x+1)^4 + z", "z"},
        {"cube of trinomial", "(x^2+x+1)^3 - 0", "(x^2+x+1)^3"},
        {"horner", "x^5 + 2*x^4 + 3*x^3 + 4*x^2 + 5*x + 6", "((((x+2)*x+3)*x+4)*x+5)*x+6"},
        {"square of product", "x^2*y^2 + 4*x^2*y + 4*x^2 - 2*x*y^2 - 8*x*y - 8*x + y^2 + 4*y + 4", "((x-1)*(y+2))^2"},
        {"hidden symmetric", "(x+y+z)^3 - (x+y)^3 - z^3", "3*z*(x+y)*(x+y+z)"},
        {"nested", "((x+1)^2 - x^2 - 1)^3", "8*x^3"},
        {"completed square", "x^2 + 6*x + 10", "(x+3)^2 + 1"},
    };
    poly_kernel_manifest generator{k_poly_default_limits};
    std::mt19937 rng{42};
    for (int index = 1; index <= 12; ++index)
        problems.push_back(random_problem(rng, "random " + std::to_string(index), {"x", "y", "z"}, 1, generator));
    for (int index = 1; index <= 8; ++index)
        problems.push_back(random_problem(
            rng, "wide " + std::to_string(index), {"a", "b", "c", "d", "e", "f", "g", "h"}, 5, generator));

    std::printf("budgets: best-first %zu rewrites; mcts %zu simulations x depth %zu\n", k_rewrites, k_simulations,
                k_probing_depth);
    std::printf("cost  *: no larger than target   !: UNSOUND\n\n");
    std::printf("%-28s %5s %6s | %-13s | %-13s | %-13s | %-13s\n", "problem", "input", "target", "full best-1st",
                "full mcts", "base best-1st", "base mcts");
    size_t solved[4] = {0, 0, 0, 0};
    std::vector<std::string> notes;
    std::vector<bench_result> gated;
    for (const problem& p : problems) {
        poly_kernel_manifest sizing{k_poly_default_limits};
        const size_t input = sizing.sizes_.cost(sizing.parser_.parse(p.text));
        const size_t target = sizing.sizes_.cost(sizing.parser_.parse(p.target));
        const bool skip_mcts = options.fast && p.name.rfind("wide", 0) == 0;
        const outcome skipped{.cost = input, .best = "(skipped)", .seconds = 0, .sound = true};
        const outcome results[4] = {
            run_best_first<poly_reduce_manifest>(p.text),
            skip_mcts ? skipped : run_mcts<poly_reduce_mcts_manifest>(p.text),
            run_best_first<poly_baseline_manifest>(p.text),
            skip_mcts ? skipped : run_mcts<poly_baseline_mcts_manifest>(p.text),
        };
        const char* const solvers[4] = {"full best-1st", "full mcts", "base best-1st", "base mcts"};
        for (int config = 0; config < 4; ++config) {
            solved[config] += results[config].cost <= target ? 1 : 0;
            const bool ran = !(skip_mcts && (config == 1 || config == 3));
            if (ran)
                gated.push_back(bench_result{.problem = p.name, .solver = solvers[config], .cost = results[config].cost,
                                             .target = target, .seconds = results[config].seconds,
                                             .sound = results[config].sound});
        }
        std::printf("%-28s %5zu %6zu | %s | %s | %s | %s\n", p.name.c_str(), input, target,
                    cell(results[0], target).c_str(), cell(results[1], target).c_str(),
                    cell(results[2], target).c_str(), cell(results[3], target).c_str());
        std::fflush(stdout);
        notes.push_back(p.name + "\n    input:     " + p.text + "\n    target:    " + p.target
                        + "\n    best-1st:  " + results[0].best + "\n    mcts:      " + results[1].best
                        + "\n    base mcts: " + results[3].best);
    }
    std::printf("\nsolved (of %zu): full best-1st %zu, full mcts %zu, base best-1st %zu, base mcts %zu\n\n",
                problems.size(), solved[0], solved[1], solved[2], solved[3]);
    for (const std::string& note : notes)
        std::printf("%s\n", note.c_str());
    return finish_bench(options, gated);
}
