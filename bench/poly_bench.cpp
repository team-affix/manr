// Polynomial reduction benchmark: every problem under four solvers (full rules or low-level only, best-first or
// MCTS) with comparable budgets. A problem is solved when the result is no larger than a known compact form.
// Every result is checked against the input's normal form. A problem holding free variables `?n` gives the bindings
// that reach its target after a `|` (`1 | ?0 = 0, ?1 = x`): a result is sound when it equals the input under them.
//
//   poly_bench [--fast] [--serial] [--write FILE] [--check FILE]
//
// --fast skips MCTS on the wide problems (minutes each); --write saves the results, --check gates them against a
// baseline (bench/bench_gate.hpp).
// Problems are solved in parallel, one per thread; --serial solves them one at a time, for true timings.
#include <array>
#include <chrono>
#include <cstdio>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include "bench_gate.hpp"
#include "composition/poly_baseline_manifest.hpp"
#include "composition/poly_baseline_mcts_manifest.hpp"
#include "composition/poly_reduce_manifest.hpp"
#include "composition/poly_reduce_mcts_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"

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

// The expression as a fraction of polynomials: a quotient `div(n, d)` is (n, d), a polynomial p is (p, 1).
std::optional<std::pair<const expr*, const expr*>> find_fraction(poly_kernel_manifest& kernel, const expr* e) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    const bool divided = f != nullptr && f->args.size() == 2 && f->id == kernel.symbols_.intern_symbol("div");
    if (divided)
        return std::make_pair(f->args[0], f->args[1]);
    if (!kernel.expander_.find_polynomial(e).has_value())
        return std::nullopt;
    return std::make_pair(e, kernel.literals_.make_literal(1));
}

// Equal as fractions of polynomials, n1 / d1 = n2 / d2 when n1 * d2 = n2 * d1 (so wherever d1 and d2 are not 0).
bool means_the_same(poly_kernel_manifest& kernel, const expr* best, const expr* initial) {
    const auto left = find_fraction(kernel, best);
    const auto right = find_fraction(kernel, initial);
    if (!left.has_value() || !right.has_value())
        return false;
    const uint32_t mul = kernel.poly_symbols_.mul;
    const auto cross = kernel.expander_.find_polynomial(
        kernel.exprs_.make_functor(mul, {left.value().first, right.value().second}));
    return cross.has_value()
           && cross == kernel.expander_.find_polynomial(
                           kernel.exprs_.make_functor(mul, {right.value().first, left.value().second}));
}

template<typename Manifest>
outcome finish(Manifest& m, const expr* best, std::chrono::steady_clock::time_point start) {
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const bool sound = best == m.initial_ || means_the_same(m.kernel_, best, m.initial_);
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

// one problem under all four solvers
struct solved {
    size_t input;
    size_t target;
    bool skip_mcts;
    std::array<outcome, 4> results;
};

// e with each hole `?k` the bindings name replaced by its binding
const expr* bind_holes(poly_kernel_manifest& kernel, const expr* e, const std::map<uint32_t, const expr*>& bindings) {
    const expr::var* hole = std::get_if<expr::var>(&e->content);
    if (hole != nullptr) {
        const auto bound = bindings.find(hole->index);
        return bound == bindings.end() ? e : bound->second;
    }
    const expr::functor& f = std::get<expr::functor>(e->content);
    std::vector<const expr*> args;
    for (const expr* arg : f.args)
        args.push_back(bind_holes(kernel, arg, bindings));
    return kernel.exprs_.make_functor(f.id, args);
}

// "?0 = 0, ?1 = x" as bindings
std::map<uint32_t, const expr*> parse_bindings(poly_kernel_manifest& kernel, const std::string& text) {
    std::map<uint32_t, const expr*> bindings;
    std::stringstream entries(text);
    std::string entry;
    while (std::getline(entries, entry, ',')) {
        const size_t equals = entry.find('=');
        const expr* hole = kernel.parser_.parse(entry.substr(0, equals));
        bindings.emplace(std::get<expr::var>(hole->content).index, kernel.parser_.parse(entry.substr(equals + 1)));
    }
    return bindings;
}

solved solve(const problem& p, bool fast) {
    poly_kernel_manifest sizing{k_poly_default_limits};
    const size_t bar = p.target.find('|');
    const size_t input = sizing.sizes_.cost(sizing.parser_.parse(p.text));
    const size_t target = sizing.sizes_.cost(sizing.parser_.parse(p.target.substr(0, bar)));
    const bool skip_mcts = fast && p.name.rfind("wide", 0) == 0;
    const outcome skipped{.cost = input, .best = "(skipped)", .seconds = 0, .sound = true};
    solved run{.input = input, .target = target, .skip_mcts = skip_mcts, .results = {
        run_best_first<poly_reduce_manifest>(p.text),
        skip_mcts ? skipped : run_mcts<poly_reduce_mcts_manifest>(p.text),
        run_best_first<poly_baseline_manifest>(p.text),
        skip_mcts ? skipped : run_mcts<poly_baseline_mcts_manifest>(p.text),
    }};
    if (bar == std::string::npos)
        return run;
    const std::map<uint32_t, const expr*> bindings = parse_bindings(sizing, p.target.substr(bar + 1));
    const std::optional<poly_polynomial> meaning =
        sizing.expander_.find_polynomial(bind_holes(sizing, sizing.parser_.parse(p.text), bindings));
    for (outcome& result : run.results) {
        if (result.best == "(skipped)")
            continue;
        const expr* best = bind_holes(sizing, sizing.parser_.parse(result.best), bindings);
        result.sound = meaning.has_value() && sizing.expander_.find_polynomial(best) == meaning;
    }
    return run;
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
        // showcase (docs/showcase.md): identities, cancellations and factorings
        {"showcase: difference of cubes of neighbours", "(#0 + 1)^3 - (#0 - 1)^3", "6*#0^2 + 2"},
        {"showcase: polarization", "(#1 + #0)^2 - (#1 - #0)^2", "4*#1*#0"},
        {"showcase: a full cancellation", "#0^4 - 1 - (#0^2 + 1)*(#0^2 - 1)", "0"},
        {"showcase: a constant in disguise", "(#0 + 2)*(#0 + 3) - #0*(#0 + 5)", "6"},
        {"showcase: binomial cube", "#0^3 + 3*#0^2 + 3*#0 + 1", "(#0 + 1)^3"},
        {"showcase: common factor", "#1^2*#0 + #1*#0^2", "#1*#0*(#1 + #0)"},
        {"showcase: telescoping product", "(#0^2 + #0)*(#0 - 1) + #0", "#0^3"},
        {"showcase: square of a cube", "#0^6 - 2*#0^3 + 1", "(#0^3 - 1)^2"},
        {"showcase: two squares identity", "(#3*#0 - #2*#1)^2 + (#3*#1 + #2*#0)^2", "(#3^2 + #2^2)*(#1^2 + #0^2)"},
        {"showcase: tenth power",
         "#0^10 + 10*#0^9 + 45*#0^8 + 120*#0^7 + 210*#0^6 + 252*#0^5 + 210*#0^4 + 120*#0^3 + 45*#0^2 + 10*#0 + 1",
         "(#0 + 1)^10"},
        {"showcase: square of a trinomial", "#2^2 + #1^2 + #0^2 + 2*#2*#1 + 2*#2*#0 + 2*#1*#0", "(#2 + #1 + #0)^2"},
        {"showcase: product of conjugates", "(#0^2 + #0 + 1)*(#0^2 - #0 + 1)", "#0^4 + #0^2 + 1"},
        // free variables, bound to what makes the whole smallest: reduce requests #17, #25, #26, and similar
        {"#17 (?0 + 1)^2 - ?0^2", "(?0 + 1)^2 - ?0^2", "1 | ?0 = 0"},
        {"#25 a hole bound to an input", "?0^2 - x^2 + 1", "1 | ?0 = x"},
        {"#26 x * ?0 + x * y", "x * ?0 + x * y", "0 | ?0 = -1 * y"},
        {"a hole twice", "?0 + ?0", "0 | ?0 = 0"},
        {"two holes in a product", "?0 * ?1 + x^2 - 5", "x^2 - 5 | ?0 = 0, ?1 = 0"},
        {"a hole cancelling a product", "(x - 1) * ?0 - x^2 + 1", "0 | ?0 = x + 1"},
        {"a hole cubed against a cube", "2 * ?0^3 - 2 * (x + y)^3 + 4", "4 | ?0 = x + y"},
        {"a scaled hole, nearest quotient", "3 * ?0 + 6 * x^2 + 7", "1 | ?0 = -2 * x^2 - 2"},
        // reduce request #32: a hole bound to the shift that completes a square
        {"#32 (?0 - x)^2 + y", "(?0 - x)^2 + y", "y | ?0 = x"},
        {"a hole bound to a product", "?0^2 - 2 * ?0 * x * y + x^2 * y^2 + 1", "1 | ?0 = x * y"},
        {"a scaled square completed", "2 * ?0^2 - 4 * x * ?0 + 2 * x^2 + z", "z | ?0 = x"},
        // reduce request #33: holes bound jointly by the linear system of the coefficients
        {"#33 two holes as coefficients", "?0 * x^2 + ?1 * x + ?0 - (x + 1)^2", "0 | ?0 = 1, ?1 = 2"},
        {"two holes leaving a constant", "?0 * x + ?1 * y - x + 2 * y + 5", "5 | ?0 = 1, ?1 = -2"},
        {"three holes as coefficients", "?0 * x^2 + ?1 * x * y + ?2 * y^2 - (x - 2 * y)^2",
         "0 | ?0 = 1, ?1 = -4, ?2 = 4"},
        {"a product of two holes, not linear", "?0 * ?1 * x - x + 3", "-1 * x + 3 | ?0 = 0, ?1 = 0"},
        // reduce request #38: quotients of polynomials, their common factor cancelled
        {"#38 (x^2 - 1) / (x - 1)", "(x^2 - 1) / (x - 1)", "x + 1"},
        {"a quotient by a square's root", "(x^2 + 2 * x * y + y^2) / (x + y)", "x + y"},
        {"a common factor of degree one", "(x^2 - 1) / (x^2 + 2 * x + 1)", "(x - 1) / (x + 1)"},
        {"a quotient with nothing in common", "(x^2 + 1) / (x + 1)", "(x^2 + 1) / (x + 1)"},
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
    size_t solved_counts[4] = {0, 0, 0, 0};
    std::vector<std::string> notes;
    std::vector<bench_result> gated;
    const std::vector<solved> runs = solve_in_parallel(problems, solve, options);
    for (size_t index = 0; index < problems.size(); ++index) {
        const problem& p = problems[index];
        const size_t input = runs[index].input;
        const size_t target = runs[index].target;
        const bool skip_mcts = runs[index].skip_mcts;
        const std::array<outcome, 4>& results = runs[index].results;
        const char* const solvers[4] = {"full best-1st", "full mcts", "base best-1st", "base mcts"};
        for (int config = 0; config < 4; ++config) {
            solved_counts[config] += results[config].cost <= target ? 1 : 0;
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
                problems.size(), solved_counts[0], solved_counts[1], solved_counts[2], solved_counts[3]);
    for (const std::string& note : notes)
        std::printf("%s\n", note.c_str());
    return finish_bench(options, gated);
}
