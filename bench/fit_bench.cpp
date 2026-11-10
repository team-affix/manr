// Data-fitting benchmark: integer samples of polynomials and piecewise polynomials, some with outliers, under the
// best-first and MCTS fit solvers. A problem is solved when the result is no larger than a known compact form (its
// cost is given, since piecewise forms do not parse). Every result is checked by evaluating it at every sample.
//
//   fit_bench [--fast] [--serial] [--full-inputs] [--write FILE] [--check FILE]
//
// --fast skips MCTS on the large data sets; --write saves the results, --check gates them against a baseline
// (bench/bench_gate.hpp).
// Problems are solved in parallel, one per thread; --serial solves them one at a time, for true timings. Notes print
// each input shortened (abbreviate_piecewise); --full-inputs prints every case, and the whole result.
#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include "bench_gate.hpp"
#include "composition/poly_fit_manifest.hpp"
#include "composition/poly_fit_mcts_manifest.hpp"
#include "composition/poly_kernel_manifest.hpp"
#include "infrastructure/poly_interpolator.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"

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
    std::vector<std::pair<size_t, std::string>> holes;  // label text over holes h1, h2, ...; `@` puts it in the key
    size_t target;
    std::string target_text;
    std::string guard;  // when not empty, the first piece applies where the guard holds on the split variable
};

struct outcome {
    std::string input;
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

// The piece a named guard picks at a value: for "prime", "even", "odd above 10", "prime or below 5", the first where
// it holds and the second elsewhere; for "even, then multiple of 3" and "prime, then multiple of 4", the first where
// the first condition holds, the second where the other does, and the third elsewhere.
size_t find_guarded_piece(const std::string& guard, int value) {
    bool prime = value >= 2;
    for (int divisor = 2; divisor * divisor <= value; ++divisor)
        prime = prime && value % divisor != 0;
    if (guard == "even, then multiple of 3")
        return value % 2 == 0 ? 0 : value % 3 == 0 ? 1 : 2;
    if (guard == "prime, then multiple of 4")
        return prime ? 0 : value % 4 == 0 ? 1 : 2;
    bool holds = prime || value < 5;
    if (guard == "prime")
        holds = prime;
    if (guard == "even")
        holds = value % 2 == 0;
    if (guard == "odd above 10")
        holds = value % 2 != 0 && value > 10;
    return holds ? 0 : 1;
}

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
        if (!p.guard.empty())
            part = find_guarded_piece(p.guard, coordinates[p.split]);
        while (p.guard.empty() && coordinates[p.split] >= p.pieces[part].below)
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

// An expression with every symbol `h1` ... `h9` replaced by the hole `?1` ... `?9`.
template<typename Kernel>
const expr* fill_holes(Kernel& kernel, const expr* e) {
    for (uint32_t index = 1; index <= 9; ++index)
        if (e == kernel.parser_.parse("h" + std::to_string(index)))
            return kernel.exprs_.make_var(index);
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.empty())
        return e;
    std::vector<const expr*> args;
    for (const expr* arg : f->args)
        args.push_back(fill_holes(kernel, arg));
    return kernel.exprs_.make_functor(f->id, args);
}

// The samples as data points, each hole's label replaced by its text over holes (`2 * h1` is `2 * ?1`), or, for a
// text starting with `@`, the key's split coordinate replaced by it.
template<typename Manifest>
std::vector<pw_data_point> find_points(Manifest& m, const fit_problem& p, const std::vector<poly_sample>& samples) {
    std::vector<pw_data_point> points = m.rule_set_.fit_.converter_.convert(samples);
    for (const auto& [index, text] : p.holes) {
        const bool key = text.front() == '@';
        const expr* filled = fill_holes(m.kernel_, m.kernel_.parser_.parse(key ? text.substr(1) : text));
        if (!key) {
            points[index].output = filled;
            continue;
        }
        const expr::functor& point = std::get<expr::functor>(points[index].input->content);
        std::vector<const expr*> coordinates = point.args;
        coordinates[p.split] = filled;
        points[index].input = m.kernel_.exprs_.make_functor(point.id, coordinates);
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
        sound = sound && value.has_value() && m.rule_set_.piecewise_.matcher_.match(point.output, value.value(), bindings);
    }
    std::ostringstream os;
    m.kernel_.printer_.print(os, best);
    return outcome{.input = "", .cost = search.best_cost(), .best = os.str(), .seconds = seconds, .sound = sound};
}

// A problem with holes starts from its own piecewise, with a search of its own.
outcome run_best_first(const fit_problem& p, const std::vector<poly_sample>& samples) {
    const auto start = std::chrono::steady_clock::now();
    poly_fit_manifest m{p.variables.size(), samples, k_poly_default_limits, k_poly_fit_default_limits, k_frontier};
    const std::vector<pw_data_point> points = find_points(m, p, samples);
    std::ostringstream input;
    m.kernel_.printer_.print(input, m.rule_set_.piecewise_.builder_.build(points, 0));
    if (p.holes.empty()) {
        while (m.search_.rewrites() < k_rewrites && m.search_.next()) {}
        outcome reached = finish(m, m.search_, points, start);
        reached.input = input.str();
        return reached;
    }
    poly_fit_manifest::search_t search{m.rule_set_.rules_, m.kernel_.sizes_, m.rule_set_.piecewise_.builder_.build(points, 0), k_frontier};
    while (search.rewrites() < k_rewrites && search.next()) {}
    outcome reached = finish(m, search, points, start);
    reached.input = input.str();
    return reached;
}

outcome run_mcts(const fit_problem& p, const std::vector<poly_sample>& samples) {
    const auto start = std::chrono::steady_clock::now();
    poly_fit_mcts_manifest m{p.variables.size(), samples, k_poly_default_limits, k_poly_fit_default_limits,
                             k_probing_depth, k_simulations, k_exploration, 1};
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

solved solve(const fit_problem& p, bool fast) {
    const std::vector<poly_sample> samples = sample(p);
    const bool skip_mcts = (fast && samples.size() > k_large) || !p.holes.empty();
    const outcome skipped{.input = "", .cost = 0, .best = "(skipped)", .seconds = 0, .sound = true};
    return solved{.points = samples.size(), .skip_mcts = skip_mcts, .best_first = run_best_first(p, samples),
                  .mcts = skip_mcts ? skipped : run_mcts(p, samples)};
}

}

int main(int argc, char** argv) {
    const bench_options options = parse_bench_options(argc, argv);
    const std::vector<fit_problem> problems{
        {"cubic", {"x"}, {{"x^3 - x", INT_MAX}}, 0, -3, 4, {}, {}, 6, "lam(#0^3 - #0)", ""},
        {"power of linear", {"x"}, {{"(2*x - 1)^3", INT_MAX}}, 0, -3, 4, {}, {}, 8, "lam((2*#0 - 1)^3)", ""},
        {"horner quintic", {"x"}, {{"x^5 + 2*x^4 + 3*x^3 + 4*x^2 + 5*x + 6", INT_MAX}}, 0, -3, 5, {}, {}, 20,
         "lam(((((#0+2)*#0+3)*#0+4)*#0+5)*#0+6)", ""},
        {"one outlier", {"x"}, {{"x^2 + 1", INT_MAX}}, 0, 0, 8, {{3, 7}}, {}, 11, "pw(case(point(3), 17), lam(#0^2 + 1))", ""},
        {"outlier run", {"x"}, {{"x^2 - 4*x", INT_MAX}}, 0, 0, 39,
         {{0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1}, {8, 1}, {9, 1}, {10, 1}, {11, 1}}, {}, 102,
         "12 cases + #0^2 - 4*#0", ""},
        {"scattered outliers", {"x"}, {{"x^2 + 1", INT_MAX}}, 0, 0, 39, {{3, 5}, {10, 5}, {17, 5}, {24, 5}, {31, 5}, {38, 5}},
         {}, 31, "6 cases + #0^2 + 1", ""},
        {"cubic 1000", {"x"}, {{"x^3 - 2*x + 5", INT_MAX}}, 0, -500, 499, {}, {}, 10, "lam(#0^3 - 2*#0 + 5)", ""},
        {"two-variable square", {"x", "y"}, {{"(x + y)^2", INT_MAX}}, 0, -2, 2, {}, {}, 7, "lams(2, (#1 + #0)^2)", ""},
        {"two-variable product", {"x", "y"}, {{"(x - y + 2)*(x + 2*y)", INT_MAX}}, 0, -2, 2, {{4, 3}}, {}, 19,
         "pw(case(point(2, -2), -9), lams(2, (#1 - #0 + 2)*(#1 + 2*#0)))", ""},
        {"three-variable cube", {"x", "y", "z"}, {{"(x + y + z)^3", INT_MAX}}, 0, -1, 2, {}, {}, 9, "lams(3, (#2 + #1 + #0)^3)", ""},
        {"three-variable mixed", {"x", "y", "z"}, {{"x*y*z + x^2 - z", INT_MAX}}, 0, -1, 2, {}, {}, 13, "lams(3, #2*#1*#0 + #2^2 - #0)", ""},
        {"two pieces", {"x"}, {{"x^2", 0}, {"3*x + 1", INT_MAX}}, 0, -10, 9, {}, {}, 14,
         "lam(pw(when(lt(#0, 0), #0^2), 3*#0 + 1))", ""},
        {"two pieces 400", {"x"}, {{"x^2", 0}, {"3*x + 1", INT_MAX}}, 0, -200, 199, {}, {}, 14,
         "lam(pw(when(lt(#0, 0), #0^2), 3*#0 + 1))", ""},
        {"absolute value", {"x"}, {{"-1*x", 0}, {"x", INT_MAX}}, 0, -10, 10, {}, {}, 10, "lam(pw(when(lt(#0, 1), -1*#0), #0))", ""},
        {"three pieces", {"x"}, {{"5", 10}, {"x^2 - 7", 20}, {"(x - 1)^3", INT_MAX}}, 0, 0, 29, {}, {}, 21,
         "lam(pw(when(lt(#0, 10), 5), when(lt(#0, 20), #0^2 - 7), (#0 - 1)^3))", ""},
        {"pieces with outlier", {"x"}, {{"x^2", 0}, {"3*x + 1", INT_MAX}}, 0, -10, 9, {{4, 3}}, {}, 19,
         "pw(case(point(-6), 39), lam(pw(when(lt(#0, 0), #0^2), 3*#0 + 1)))", ""},
        {"split on x", {"x", "y"}, {{"x*y", 1}, {"x + y^2", INT_MAX}}, 0, -3, 3, {}, {}, 15, "lams(2, pw(when(lt(#1, 1), #1*#0), #0^2 + #1))", ""},
        {"split on y", {"x", "y"}, {{"x - y", 0}, {"(x + y)^2", INT_MAX}}, 1, -3, 3, {}, {}, 15,
         "lams(2, pw(when(lt(#0, 0), #1 - #0), (#1 + #0)^2))", ""},
        {"sign flip at zero", {"x", "y"}, {{"x - y", 0}, {"x + y", INT_MAX}}, 0, -3, 3, {}, {}, 13,
         "pw(when(lt(x, 0), x - y), x + y)", ""},
        // reduce request #8, and similar iterated polynomials, read with a helper by the iteration detector
        {"#8 g(g(g(x))), g = y^2 + 1", {"x"}, {{"x^8 + 4*x^6 + 8*x^4 + 8*x^2 + 5", INT_MAX}}, 0, -6, 6, {}, {}, 15,
         "lam(let(lam(#0^2 + 1), app(#0, app(#0, app(#0, #1)))))", ""},
        {"g(g(x)), g = y^3 - y", {"x"}, {{"(x^3 - x)^3 - (x^3 - x)", INT_MAX}}, 0, -6, 6, {}, {}, 13,
         "lam(let(lam(#0^3 - #0), app(#0, app(#0, #1))))", ""},
        {"g(g(g(x))), g = y^2 + y", {"x"}, {{"((x^2 + x)^2 + x^2 + x)^2 + (x^2 + x)^2 + x^2 + x", INT_MAX}}, 0, -6, 6, {},
         {}, 15, "lam(let(lam(#0^2 + #0), app(#0, app(#0, app(#0, #1)))))", ""},
        {"g(g(x)), g = y^2 - 2", {"x"}, {{"(x^2 - 2)^2 - 2", INT_MAX}}, 0, -6, 6, {}, {}, 10,
         "(x^2 - 2)^2 - 2 (smaller than the helper form, 12)", ""},
        // reduce request #18, and similar labels holding holes: a hole is bound to the model's value, or solved for
        {"#18 x^2 + 1, holes in two labels", {"x"}, {{"x^2 + 1", INT_MAX}}, 0, 0, 8, {}, {{1, "h1"}, {3, "2 * h2"}}, 6,
         "lam(#0^2 + 1), ?1 = 2, ?2 = 5", ""},
        {"two-variable square, holes", {"x", "y"}, {{"(x + y)^2", INT_MAX}}, 0, -2, 2, {}, {{3, "h1"}, {12, "-1 * h2"}, {23, "3 * h3"}},
         7, "lams(2, (#1 + #0)^2)", ""},
        {"hole with no integer solution", {"x"}, {{"x^2 + 1", INT_MAX}}, 0, 0, 8, {}, {{2, "2 * h1"}}, 13,
         "pw(case(point(2), 2 * ?1), lam(#0^2 + 1))", ""},
        // reduce requests #22, #23, #24: labels polynomial in a hole, labels holding several, keys holding one
        {"#22 a label nonlinear in its hole", {"x"}, {{"x^2 + 1", INT_MAX}}, 0, 0, 8, {}, {{3, "h1^2 + 1"}}, 6,
         "lam(#0^2 + 1), ?1 = 3", ""},
        {"#23 a label holding two holes", {"x"}, {{"x^2 + 1", INT_MAX}}, 0, 0, 8, {}, {{3, "h1 + h2"}}, 6,
         "lam(#0^2 + 1), ?1 = 0, ?2 = 10", ""},
        {"#24 a key holding a hole", {"x"}, {{"x^2 + 1", INT_MAX}}, 0, 0, 8, {}, {{4, "@h1"}}, 6,
         "lam(#0^2 + 1), ?1 = 4", ""},
        {"cubic label, hole cubed", {"x"}, {{"x^3 - x", INT_MAX}}, 0, -3, 4, {}, {{5, "h1^3 - 2"}}, 6,
         "lam(#0^3 - #0), ?1 = 2", ""},
        {"two-variable square, product and square holes", {"x", "y"}, {{"(x + y)^2", INT_MAX}}, 0, -2, 2, {},
         {{7, "h1 * h2"}, {24, "h3^2"}}, 7, "lams(2, (#1 + #0)^2), ?1 = 1, ?2 = 1, ?3 = 4", ""},
        {"two-variable square, key hole", {"x", "y"}, {{"(x + y)^2", INT_MAX}}, 0, -2, 2, {}, {{9, "@h1"}}, 7,
         "lams(2, (#1 + #0)^2), ?1 = 0", ""},
        {"key hole no value explains", {"x"}, {{"x^2 + 1", INT_MAX}}, 0, 0, 8, {{4, 1}}, {{4, "@h1"}}, 11,
         "pw(case(point(?1), 18), lam(#0^2 + 1))", ""},
        // every third point an outlier: once open, now read as a period-3 piecewise by the residue detector
        {"dense outliers", {"x"}, {{"x^2 + 1", INT_MAX}}, 0, 0, 29,
         {{1, 2}, {4, 2}, {7, 2}, {10, 2}, {13, 2}, {16, 2}, {19, 2}, {22, 2}, {25, 2}, {28, 2}}, {}, 47,
         "10 cases + #0^2 + 1", ""},
        // showcase (docs/showcase.md): realistic data, and variants measuring how far each generalizes
        {"showcase: bulk pricing", {"x"}, {{"12*x", 10}, {"10*x + 20", INT_MAX}}, 0, 0, 40, {}, {}, 14,
         "lam(pw(when(lt(#0, 10), 12*#0), 10*#0 + 20))", ""},
        {"showcase: bulk pricing, 400 orders", {"x"}, {{"12*x", 10}, {"10*x + 20", INT_MAX}}, 0, 0, 399, {}, {}, 14,
         "lam(pw(when(lt(#0, 10), 12*#0), 10*#0 + 20))", ""},
        {"showcase: bulk pricing, two typos", {"x"}, {{"12*x", 10}, {"10*x + 20", INT_MAX}}, 0, 0, 40,
         {{7, 3}, {25, -10}}, {}, 23, "two cases + the pricing", ""},
        {"bulk pricing, unknown totals", {"x"}, {{"12*x", 10}, {"10*x + 20", INT_MAX}}, 0, 0, 40, {},
         {{3, "h1"}, {18, "h2"}, {30, "2 * h3"}}, 14, "the pricing, holes bound", ""},
        {"showcase: projectile height", {"x"}, {{"-5*x^2 + 30*x + 2", INT_MAX}}, 0, 0, 8, {}, {}, 10,
         "lam(-5*#0^2 + 30*#0 + 2)", ""},
        {"showcase: projectile height, bad reading", {"x"}, {{"-5*x^2 + 30*x + 2", INT_MAX}}, 0, 0, 8, {{4, 9}}, {}, 15,
         "one case + the height", ""},
        {"showcase: framed picture area", {"x", "y"}, {{"(x + 2)*(y + 2)", INT_MAX}}, 0, 1, 6, {}, {}, 9,
         "lams(2, (#1 + 2)*(#0 + 2))", ""},
        {"showcase: box volume", {"x", "y", "z"}, {{"x*y*z", INT_MAX}}, 0, 1, 4, {}, {}, 7, "lams(3, #2*#1*#0)", ""},
        {"showcase: sensor log, four spikes", {"x"}, {{"3*x + 7", INT_MAX}}, 0, 0, 59,
         {{5, 40}, {17, -30}, {33, 25}, {48, 60}}, {}, 23, "four cases + lam(3*#0 + 7)", ""},
        {"showcase: tiered shipping", {"x"}, {{"8", 5}, {"2*x - 2", 20}, {"3*x - 22", INT_MAX}}, 0, 0, 40, {}, {}, 21,
         "lam(pw(when(lt(#0, 5), 8), when(lt(#0, 20), 2*#0 - 2), 3*#0 - 22))", ""},
        {"showcase: projectile height, three bad readings", {"x"}, {{"-5*x^2 + 30*x + 2", INT_MAX}}, 0, 0, 8,
         {{2, 9}, {4, 9}, {6, -7}}, {}, 23, "three cases + the height", ""},
        {"showcase: projectile height, 20 readings, five bad", {"x"}, {{"-5*x^2 + 30*x + 2", INT_MAX}}, 0, 0, 19,
         {{2, 9}, {6, -7}, {9, 4}, {13, 11}, {17, -3}}, {}, 31, "five cases + the height", ""},
        {"showcase: sensor log, spikes and gaps", {"x"}, {{"3*x + 7", INT_MAX}}, 0, 0, 59, {{5, 40}, {17, -30}}, {{10, "h1"}, {40, "h2"}},
         15, "two cases + lam(3*#0 + 7)", ""},
        {"showcase: tiered shipping, four tiers", {"x"}, {{"8", 5}, {"2*x - 2", 20}, {"3*x - 22", 30}, {"68", INT_MAX}}, 0,
         0, 40, {}, {}, 26, "four pieces", ""},
        {"showcase: framed picture area, bad reading", {"x", "y"}, {{"(x + 2)*(y + 2)", INT_MAX}}, 0, 1, 6, {{8, 5}}, {}, 15,
         "one case + the area", ""},
        {"showcase: three tiers, a typo and unknown labels", {"x"}, {{"8", 5}, {"2*x - 2", 20}, {"3*x - 22", INT_MAX}}, 0, 0,
         40, {{12, 5}}, {{3, "h1"}, {25, "h2"}}, 26, "pw(case(point(12), 27), lam(pw(when(lt(#0, 6), 8), when(lt(#0, 21), 2*#0 - 2), 3*#0 - 22)))", ""},
        {"showcase: three inputs, a typo and unknown labels", {"x", "y", "z"}, {{"x*y - z^2", INT_MAX}}, 0, -2, 2, {{40, 7}},
         {{11, "h1"}, {52, "2 * h2"}}, 16, "pw(case(point(-2, 1, -1), 4), lams(3, #2*#1 - #0^2))", ""},
        // piecewise data whose branch is guarded by a Boolean condition (pw_guarded_hypotheses)
        {"square where prime, else linear", {"x"}, {{"x^2", 0}, {"2*x + 1", 0}}, 0, 0, 40, {}, {}, 13,
         "lam(pw(when(is_prime(#0), #0^2), 2*#0 + 1))", "prime"},
        {"constant where even, else cubic", {"x"}, {{"7", 0}, {"x^3 - x", 0}}, 0, -10, 20, {}, {}, 14,
         "lam(pw(when(eq(mod(#0, 2), 0), 7), #0^3 - #0))", "even"},
        {"linear where odd above 10, else square", {"x"}, {{"5*x - 3", 0}, {"x^2", 0}}, 0, 0, 40, {}, {}, 20,
         "lam(pw(when(and(eq(mod(#0, 2), 1), lt(10, #0)), 5*#0 - 3), #0^2))", "odd above 10"},
        {"zero where prime or small, else doubled", {"x"}, {{"0", 0}, {"2*x", 0}}, 0, 0, 40, {}, {}, 13,
         "lam(pw(when(or(is_prime(#0), lt(#0, 5)), 0), 2*#0))", "prime or below 5"},
        // reduce request #30, and similar: several guarded branches, a constant under each condition, then a model
        {"#30 data with two guarded branches", {"x"}, {{"1", 0}, {"2", 0}, {"x^2", 0}}, 0, 0, 30, {}, {}, 19,
         "lam(pw(when(eq(mod(#0, 2), 0), 1), when(eq(mod(#0, 3), 0), 2), #0^2))", "even, then multiple of 3"},
        {"zero where prime, five at multiples of 4, else linear", {"x"}, {{"0", 0}, {"5", 0}, {"2*x + 1", 0}}, 0, 0, 40,
         {}, {}, 18, "lam(pw(when(is_prime(#0), 0), when(eq(mod(#0, 4), 0), 5), 2*#0 + 1))",
         "prime, then multiple of 4"},
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
        notes.push_back(p.name + "\n    input:    "
                        + (options.full_inputs ? best_first.input : abbreviate_piecewise(best_first.input)) + "\n    target:   " + p.target_text + "\n    best-1st: " + (options.full_inputs ? best_first.best : best_first.best.substr(0, 400))
                        + "\n    mcts:     " + mcts.best.substr(0, 160));
    }
    std::printf("\nsolved (of %zu): best-1st %zu, mcts %zu\n\n", problems.size(), solved_counts[0], solved_counts[1]);
    for (const std::string& note : notes)
        std::printf("%s\n", note.c_str());
    return finish_bench(options, gated);
}
