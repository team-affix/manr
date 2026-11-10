#ifndef ARITH_HOLE_SOLVER_HPP
#define ARITH_HOLE_SOLVER_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"

constexpr size_t k_arith_default_max_hole_degree = 8;
constexpr size_t k_arith_default_max_root_trials = 100000;

// Solves `pattern = value` over the integers for the pattern's unbound extension terms (`?k`), returning their
// bindings. With several unbound holes, one that occurs once, under sums and differences only, is solved for in terms
// of the others, which stay free (`?1 + ?2` against 10 gives `?2 = 10 - ?1`): a later equation can pin them. Failing
// that, one hole is solved for, the last, and the others are tried at 0, then at 1 (the identities of `add` and
// `mul`). A pattern without unbound holes gives no new bindings when it equals the value. The hole is solved for when
// the pattern is a polynomial in it: built from the hole, literals, bound holes (whose values are such polynomials,
// `7 - ?2` with `?2` the hole), `add`, `sub`, `mul` and `pow` by a literal exponent, of degree at
// most `max_degree`. The equation is then `q(?hole) = 0` with integer coefficients: a linear one is solved by
// division; a higher one by its integer roots, which divide q's lowest nonzero coefficient (0 when q has no constant
// term), trying at most `max_root_trials` divisors. The root of least magnitude is chosen, the positive one first.
// When every coefficient vanishes the hole is free and is bound to 0. A value holding holes is moved to the pattern's
// side (`pattern - value = 0`), so two forms of one polynomial in the holes match. Nothing for any other value that
// is not a literal, any other leaf or functor, or no integer solution.
template<typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
struct arith_hole_solver {
    arith_hole_solver(IFindLiteralValue& find_literal_value,
                      IMakeLiteral& make_literal,
                      IMakeFunctor& make_functor,
                      uint32_t add_symbol,
                      uint32_t sub_symbol,
                      uint32_t mul_symbol,
                      uint32_t pow_symbol,
                      size_t max_degree = k_arith_default_max_hole_degree,
                      size_t max_root_trials = k_arith_default_max_root_trials);
    std::optional<std::map<uint32_t, const expr*>> solve_holes(const expr* pattern, const expr* value,
                                                               const std::map<uint32_t, const expr*>& bindings);
private:
    std::optional<const expr*> solve_hole(const expr* pattern, uint32_t hole, const expr* value,
                                          const std::map<uint32_t, const expr*>& bindings);
    std::optional<const expr*> isolate(const expr* e, uint32_t hole, const expr* target);
    void collect_holes(const expr* e, std::set<uint32_t>& holes);
    using coefficients_t = std::vector<integer>;
    static constexpr int k_max_exponent = 64;
    std::optional<coefficients_t> find_coefficients(const expr* e, uint32_t hole,
                                                    const std::map<uint32_t, const expr*>& bindings);
    std::optional<coefficients_t> multiply(const coefficients_t& left, const coefficients_t& right);
    std::optional<integer> find_root(const coefficients_t& equation);
    static integer evaluate(const coefficients_t& equation, const integer& at);

    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    uint32_t add_symbol_;
    uint32_t sub_symbol_;
    uint32_t mul_symbol_;
    uint32_t pow_symbol_;
    size_t max_degree_;
    size_t max_root_trials_;
};

#define ARITH_HOLE_SOLVER_TEMPLATE template<typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
#define ARITH_HOLE_SOLVER          arith_hole_solver<IFindLiteralValue, IMakeLiteral, IMakeFunctor>

ARITH_HOLE_SOLVER_TEMPLATE ARITH_HOLE_SOLVER::arith_hole_solver(IFindLiteralValue& find_literal_value,
                                                                 IMakeLiteral& make_literal,
                                                                 IMakeFunctor& make_functor,
                                                                 uint32_t add_symbol,
                                                                 uint32_t sub_symbol,
                                                                 uint32_t mul_symbol,
                                                                 uint32_t pow_symbol,
                                                                 size_t max_degree,
                                                                 size_t max_root_trials)
    : find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , add_symbol_(add_symbol)
    , sub_symbol_(sub_symbol)
    , mul_symbol_(mul_symbol)
    , pow_symbol_(pow_symbol)
    , max_degree_(max_degree)
    , max_root_trials_(max_root_trials) {}

ARITH_HOLE_SOLVER_TEMPLATE std::optional<std::map<uint32_t, const expr*>> ARITH_HOLE_SOLVER::solve_holes(
    const expr* pattern, const expr* value, const std::map<uint32_t, const expr*>& bindings) {
    const bool literal = find_literal_value_.find_value(value).has_value();
    if (!literal) {
        std::set<uint32_t> value_holes;
        collect_holes(value, value_holes);
        if (value_holes.empty())
            return std::nullopt;
        const expr* difference = make_functor_.make_functor(sub_symbol_, {pattern, value});
        return solve_holes(difference, make_literal_.make_literal(integer(0)), bindings);
    }
    std::set<uint32_t> holes;
    collect_holes(pattern, holes);
    std::vector<uint32_t> unbound;
    for (const uint32_t hole : holes)
        if (bindings.find(hole) == bindings.end())
            unbound.push_back(hole);
    if (unbound.empty()) {
        // a hole outside the pattern stands for nothing: the pattern's constant part is its value
        const uint32_t absent = holes.empty() ? 0 : *holes.rbegin() + 1;
        const std::optional<const expr*> free = solve_hole(pattern, absent, value, bindings);
        if (!free.has_value())
            return std::nullopt;
        return std::map<uint32_t, const expr*>{};
    }
    for (auto hole = unbound.rbegin(); unbound.size() > 1 && hole != unbound.rend(); ++hole) {
        const std::optional<const expr*> isolated = isolate(pattern, *hole, value);
        if (isolated.has_value())
            return std::map<uint32_t, const expr*>{{*hole, isolated.value()}};
    }
    const uint32_t solved = unbound.back();
    for (const int filler : {0, 1}) {
        std::map<uint32_t, const expr*> trial = bindings;
        std::map<uint32_t, const expr*> found;
        for (size_t index = 0; index + 1 < unbound.size(); ++index) {
            trial.emplace(unbound[index], make_literal_.make_literal(integer(filler)));
            found.emplace(unbound[index], make_literal_.make_literal(integer(filler)));
        }
        const std::optional<const expr*> root = solve_hole(pattern, solved, value, trial);
        if (!root.has_value())
            continue;
        found.emplace(solved, root.value());
        return found;
    }
    return std::nullopt;
}

ARITH_HOLE_SOLVER_TEMPLATE std::optional<const expr*> ARITH_HOLE_SOLVER::solve_hole(
    const expr* pattern, uint32_t hole, const expr* value, const std::map<uint32_t, const expr*>& bindings) {
    const std::optional<integer> target = find_literal_value_.find_value(value);
    if (!target.has_value())
        return std::nullopt;
    std::optional<coefficients_t> equation = find_coefficients(pattern, hole, bindings);
    if (!equation.has_value())
        return std::nullopt;
    equation.value()[0] -= target.value();
    const std::optional<integer> root = find_root(equation.value());
    if (!root.has_value())
        return std::nullopt;
    return make_literal_.make_literal(root.value());
}

// the pattern's coefficients as a polynomial in the hole, lowest degree first
ARITH_HOLE_SOLVER_TEMPLATE std::optional<typename ARITH_HOLE_SOLVER::coefficients_t>
ARITH_HOLE_SOLVER::find_coefficients(const expr* e, uint32_t hole, const std::map<uint32_t, const expr*>& bindings) {
    const expr::var* variable = std::get_if<expr::var>(&e->content);
    if (variable != nullptr) {
        if (variable->index == hole)
            return coefficients_t{0, 1};
        const auto bound = bindings.find(variable->index);
        if (bound == bindings.end())
            return std::nullopt;
        return find_coefficients(bound->second, hole, bindings);
    }
    const std::optional<integer> literal = find_literal_value_.find_value(e);
    if (literal.has_value())
        return coefficients_t{literal.value()};
    const expr::functor& f = std::get<expr::functor>(e->content);
    const bool binary = f.args.size() == 2
                        && (f.id == add_symbol_ || f.id == sub_symbol_ || f.id == mul_symbol_ || f.id == pow_symbol_);
    if (!binary)
        return std::nullopt;
    const std::optional<coefficients_t> left = find_coefficients(f.args[0], hole, bindings);
    if (!left.has_value())
        return std::nullopt;
    if (f.id == pow_symbol_) {
        const std::optional<integer> exponent = find_literal_value_.find_value(f.args[1]);
        const bool bounded = exponent.has_value() && exponent.value() >= 0 && exponent.value() <= k_max_exponent
                             && exponent.value() * (left.value().size() - 1) <= max_degree_;
        if (!bounded)
            return std::nullopt;
        coefficients_t power{1};
        for (integer step = 0; step < exponent.value(); ++step)
            power = multiply(power, left.value()).value();
        return power;
    }
    const std::optional<coefficients_t> right = find_coefficients(f.args[1], hole, bindings);
    if (!right.has_value())
        return std::nullopt;
    if (f.id == mul_symbol_)
        return multiply(left.value(), right.value());
    coefficients_t sum(std::max(left.value().size(), right.value().size()), integer(0));
    for (size_t degree = 0; degree < left.value().size(); ++degree)
        sum[degree] += left.value()[degree];
    for (size_t degree = 0; degree < right.value().size(); ++degree)
        sum[degree] += f.id == add_symbol_ ? right.value()[degree] : integer(-right.value()[degree]);
    return sum;
}

// `e = target` rewritten as `hole = ...`, when the hole occurs once in e, under sums and differences only
ARITH_HOLE_SOLVER_TEMPLATE std::optional<const expr*> ARITH_HOLE_SOLVER::isolate(
    const expr* e, uint32_t hole, const expr* target) {
    const expr::var* variable = std::get_if<expr::var>(&e->content);
    if (variable != nullptr)
        return variable->index == hole ? std::optional<const expr*>(target) : std::nullopt;
    const expr::functor& f = std::get<expr::functor>(e->content);
    const bool additive = f.args.size() == 2 && (f.id == add_symbol_ || f.id == sub_symbol_);
    if (!additive)
        return std::nullopt;
    std::set<uint32_t> left_holes;
    std::set<uint32_t> right_holes;
    collect_holes(f.args[0], left_holes);
    collect_holes(f.args[1], right_holes);
    const bool in_left = left_holes.contains(hole);
    const bool in_right = right_holes.contains(hole);
    if (in_left == in_right)
        return std::nullopt;
    if (in_left) {
        const uint32_t inverse = f.id == add_symbol_ ? sub_symbol_ : add_symbol_;
        return isolate(f.args[0], hole, make_functor_.make_functor(inverse, {target, f.args[1]}));
    }
    if (f.id == add_symbol_)
        return isolate(f.args[1], hole, make_functor_.make_functor(sub_symbol_, {target, f.args[0]}));
    return isolate(f.args[1], hole, make_functor_.make_functor(sub_symbol_, {f.args[0], target}));
}

ARITH_HOLE_SOLVER_TEMPLATE std::optional<typename ARITH_HOLE_SOLVER::coefficients_t> ARITH_HOLE_SOLVER::multiply(
    const coefficients_t& left, const coefficients_t& right) {
    const size_t degree = left.size() + right.size() - 2;
    if (degree > max_degree_)
        return std::nullopt;
    coefficients_t product(degree + 1, integer(0));
    for (size_t i = 0; i < left.size(); ++i)
        for (size_t j = 0; j < right.size(); ++j)
            product[i + j] += left[i] * right[j];
    return product;
}

ARITH_HOLE_SOLVER_TEMPLATE std::optional<integer> ARITH_HOLE_SOLVER::find_root(const coefficients_t& equation) {
    size_t lowest = 0;
    while (lowest < equation.size() && equation[lowest] == 0)
        ++lowest;
    if (lowest == equation.size())
        return integer(0);
    size_t highest = equation.size() - 1;
    while (equation[highest] == 0)
        --highest;
    if (highest == lowest)
        return lowest > 0 ? std::optional<integer>(integer(0)) : std::nullopt;
    if (lowest > 0)
        return integer(0);
    if (highest == 1) {
        const bool divides = equation[0] % equation[1] == 0;
        if (!divides)
            return std::nullopt;
        return integer(-equation[0] / equation[1]);
    }
    const integer constant = boost::multiprecision::abs(equation[0]);
    std::optional<integer> best;
    size_t trials = 0;
    for (integer divisor = 1; divisor * divisor <= constant && trials < max_root_trials_; ++divisor, ++trials) {
        if (constant % divisor != 0)
            continue;
        for (const integer& magnitude : {divisor, integer(constant / divisor)})
            for (const integer& root : {magnitude, integer(-magnitude)}) {
                const bool smaller = !best.has_value()
                                     || boost::multiprecision::abs(root) < boost::multiprecision::abs(best.value());
                if (smaller && evaluate(equation, root) == 0)
                    best = root;
            }
    }
    return best;
}

ARITH_HOLE_SOLVER_TEMPLATE void ARITH_HOLE_SOLVER::collect_holes(const expr* e, std::set<uint32_t>& holes) {
    const expr::var* hole = std::get_if<expr::var>(&e->content);
    if (hole != nullptr) {
        holes.insert(hole->index);
        return;
    }
    for (const expr* arg : std::get<expr::functor>(e->content).args)
        collect_holes(arg, holes);
}

ARITH_HOLE_SOLVER_TEMPLATE integer ARITH_HOLE_SOLVER::evaluate(const coefficients_t& equation, const integer& at) {
    integer result = 0;
    for (size_t degree = equation.size(); degree > 0; --degree)
        result = result * at + equation[degree - 1];
    return result;
}

#undef ARITH_HOLE_SOLVER
#undef ARITH_HOLE_SOLVER_TEMPLATE

#endif
