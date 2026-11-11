#ifndef RAT_HOLE_BINDER_HPP
#define RAT_HOLE_BINDER_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/rat_symbols.hpp"
#include "value_objects/rewrite.hpp"

constexpr size_t k_rat_default_max_root_trials = 1000;

// Root rule (it binds holes, so it must see every occurrence: apply it through `hole_scope_proposer`): binds the holes
// `?k` of a quotient in one variable so that its denominator divides its numerator, and proposes the exact quotient
// when smaller. A quotient divides when every root of the denominator is a root of the numerator, so the candidates
// come from integer roots (trial division over the divisors of the lowest coefficient, at most `max_root_trials`):
//
// - holes in the numerator only: each root r of the denominator gives `n(r) = 0`, linear in the holes, solved for
//   integers together (`(x^2 - ?0) / (x - 1)` with `?0 = 1` is `x + 1`);
// - one hole in the denominator only: each root r of the numerator gives `d(r) = 0`, solved for the hole when linear
//   in it (`(x^2 - 4) / (x - ?0)` with `?0 = 2` is `x + 2`).
//
// Each binding is checked by exact division, so a repeated root that the equations miss is never proposed. The holes
// are read as the variables `hole_symbols` (`?k` for the k-th). Nothing for several variables besides the holes.
template<typename IFindPolynomial, typename IFindQuotient, typename ISolveLinearSystem, typename IRender,
         typename IMakeFunctor, typename IFindCost>
struct rat_hole_binder {
    rat_hole_binder(IFindPolynomial& find_polynomial,
                    IFindQuotient& find_quotient,
                    ISolveLinearSystem& solve_linear_system,
                    IRender& render,
                    IMakeFunctor& make_functor,
                    IFindCost& find_cost,
                    rat_symbols symbols,
                    std::vector<uint32_t> hole_symbols,
                    size_t max_root_trials = k_rat_default_max_root_trials);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    using values_t = std::map<uint32_t, integer>;

    const expr* fill_holes(const expr* e, bool& known);
    std::vector<integer> find_roots(const poly_polynomial& polynomial, uint32_t variable) const;
    std::optional<values_t> solve_holes(const std::vector<poly_polynomial>& equations,
                                        const std::set<uint32_t>& holes) const;
    static poly_polynomial evaluate_at(const poly_polynomial& polynomial, const values_t& values);
    static std::set<uint32_t> find_variables(const poly_polynomial& polynomial);

    IFindPolynomial& find_polynomial_;
    IFindQuotient& find_quotient_;
    ISolveLinearSystem& solve_linear_system_;
    IRender& render_;
    IMakeFunctor& make_functor_;
    IFindCost& find_cost_;
    rat_symbols symbols_;
    std::vector<uint32_t> hole_symbols_;
    size_t max_root_trials_;
};

#define RAT_HOLE_BINDER_TEMPLATE                                                                                     \
    template<typename IFindPolynomial, typename IFindQuotient, typename ISolveLinearSystem, typename IRender,       \
             typename IMakeFunctor, typename IFindCost>
#define RAT_HOLE_BINDER \
    rat_hole_binder<IFindPolynomial, IFindQuotient, ISolveLinearSystem, IRender, IMakeFunctor, IFindCost>

RAT_HOLE_BINDER_TEMPLATE RAT_HOLE_BINDER::rat_hole_binder(IFindPolynomial& find_polynomial,
                                                          IFindQuotient& find_quotient,
                                                          ISolveLinearSystem& solve_linear_system,
                                                          IRender& render,
                                                          IMakeFunctor& make_functor,
                                                          IFindCost& find_cost,
                                                          rat_symbols symbols,
                                                          std::vector<uint32_t> hole_symbols,
                                                          size_t max_root_trials)
    : find_polynomial_(find_polynomial)
    , find_quotient_(find_quotient)
    , solve_linear_system_(solve_linear_system)
    , render_(render)
    , make_functor_(make_functor)
    , find_cost_(find_cost)
    , symbols_(symbols)
    , hole_symbols_(std::move(hole_symbols))
    , max_root_trials_(max_root_trials) {}

RAT_HOLE_BINDER_TEMPLATE void RAT_HOLE_BINDER::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->id != symbols_.quotient || f->args.size() != 2)
        return;
    bool known = true;
    const expr* numerator_text = fill_holes(f->args[0], known);
    const expr* denominator_text = fill_holes(f->args[1], known);
    if (!known || (numerator_text == f->args[0] && denominator_text == f->args[1]))
        return;
    const std::optional<poly_polynomial> numerator = find_polynomial_.find_polynomial(numerator_text);
    const std::optional<poly_polynomial> denominator = find_polynomial_.find_polynomial(denominator_text);
    if (!numerator.has_value() || !denominator.has_value() || denominator.value().terms.empty())
        return;
    // the holes of each side, and the one variable besides them
    const std::set<uint32_t> hole_set(hole_symbols_.begin(), hole_symbols_.end());
    std::set<uint32_t> top_holes;
    std::set<uint32_t> bottom_holes;
    std::set<uint32_t> variables;
    for (const uint32_t variable : find_variables(numerator.value()))
        (hole_set.contains(variable) ? top_holes : variables).insert(variable);
    for (const uint32_t variable : find_variables(denominator.value()))
        (hole_set.contains(variable) ? bottom_holes : variables).insert(variable);
    if (variables.size() != 1)
        return;
    const uint32_t variable = *variables.begin();
    std::vector<values_t> candidates;
    const bool holed_top = !top_holes.empty() && bottom_holes.empty();
    const bool holed_bottom = top_holes.empty() && bottom_holes.size() == 1;
    if (holed_top) {
        std::vector<poly_polynomial> equations;
        for (const integer& root : find_roots(denominator.value(), variable))
            equations.push_back(evaluate_at(numerator.value(), values_t{{variable, root}}));
        const std::optional<values_t> solved = equations.empty() ? std::nullopt : solve_holes(equations, top_holes);
        if (solved.has_value())
            candidates.push_back(solved.value());
    }
    for (const integer& root : holed_bottom ? find_roots(numerator.value(), variable) : std::vector<integer>{}) {
        const std::optional<values_t> solved =
            solve_holes({evaluate_at(denominator.value(), values_t{{variable, root}})}, bottom_holes);
        if (solved.has_value())
            candidates.push_back(solved.value());
    }
    const size_t cost = find_cost_.cost(e);
    std::set<poly_polynomial> proposed;
    for (const values_t& values : candidates) {
        const std::optional<poly_polynomial> exact =
            find_quotient_.find_quotient(evaluate_at(numerator.value(), values), evaluate_at(denominator.value(), values));
        if (!exact.has_value() || !proposed.insert(exact.value()).second)
            continue;
        const expr* result = render_.render(exact.value());
        if (find_cost_.cost(result) >= cost)
            continue;
        out.push_back(ready(result));
    }
}

// e with each hole `?k` as the variable `hole_symbols[k]`; `known` false for a hole past them
RAT_HOLE_BINDER_TEMPLATE const expr* RAT_HOLE_BINDER::fill_holes(const expr* e, bool& known) {
    const expr::var* hole = std::get_if<expr::var>(&e->content);
    if (hole != nullptr) {
        known = known && hole->index < hole_symbols_.size();
        return known ? make_functor_.make_functor(hole_symbols_[hole->index], {}) : e;
    }
    const expr::functor& f = std::get<expr::functor>(e->content);
    if (f.args.empty())
        return e;
    std::vector<const expr*> args;
    args.reserve(f.args.size());
    for (const expr* arg : f.args)
        args.push_back(fill_holes(arg, known));
    return make_functor_.make_functor(f.id, args);
}

// The integer roots of a polynomial in `variable` alone: 0 when it has no constant term, and the divisors (both signs)
// of its lowest nonzero coefficient that make it vanish, trying at most `max_root_trials` of them.
RAT_HOLE_BINDER_TEMPLATE std::vector<integer> RAT_HOLE_BINDER::find_roots(const poly_polynomial& polynomial,
                                                                          uint32_t variable) const {
    std::vector<integer> roots;
    if (find_variables(polynomial) != std::set<uint32_t>{variable})
        return roots;
    std::map<uint32_t, integer> by_degree;
    for (const auto& [monomial, coefficient] : polynomial.terms)
        by_degree.emplace(monomial.powers.empty() ? 0 : monomial.powers[0].exponent, coefficient);
    if (by_degree.begin()->first > 0)
        roots.push_back(integer(0));
    const integer lowest = boost::multiprecision::abs(by_degree.begin()->second);
    size_t trials = 0;
    for (integer divisor = 1; divisor <= lowest && trials < max_root_trials_; ++divisor, ++trials) {
        if (lowest % divisor != 0)
            continue;
        for (const integer& candidate : {divisor, integer(-divisor)}) {
            const poly_polynomial value = evaluate_at(polynomial, values_t{{variable, candidate}});
            if (value.terms.empty())
                roots.push_back(candidate);
        }
    }
    return roots;
}

// The integer values of the holes that make every equation 0, each linear in the holes; nothing otherwise.
RAT_HOLE_BINDER_TEMPLATE std::optional<typename RAT_HOLE_BINDER::values_t> RAT_HOLE_BINDER::solve_holes(
    const std::vector<poly_polynomial>& equations, const std::set<uint32_t>& holes) const {
    const std::vector<uint32_t> order(holes.begin(), holes.end());
    std::vector<std::vector<integer>> rows;
    for (const poly_polynomial& equation : equations) {
        std::vector<integer> row(order.size() + 1, integer(0));
        for (const auto& [monomial, coefficient] : equation.terms) {
            if (monomial.powers.empty()) {
                row[order.size()] -= coefficient;
                continue;
            }
            const bool linear = monomial.powers.size() == 1 && monomial.powers[0].exponent == 1;
            if (!linear)
                return std::nullopt;
            const auto column = std::find(order.begin(), order.end(), monomial.powers[0].variable);
            if (column == order.end())
                return std::nullopt;
            row[static_cast<size_t>(column - order.begin())] += coefficient;
        }
        rows.push_back(row);
    }
    const std::optional<std::vector<integer>> solution = solve_linear_system_.solve(rows, order.size());
    if (!solution.has_value())
        return std::nullopt;
    values_t values;
    for (size_t column = 0; column < order.size(); ++column)
        values.emplace(order[column], solution.value()[column]);
    return values;
}

// The polynomial with the given variables set to their values.
RAT_HOLE_BINDER_TEMPLATE poly_polynomial RAT_HOLE_BINDER::evaluate_at(const poly_polynomial& polynomial,
                                                                      const values_t& values) {
    poly_polynomial result;
    for (const auto& [monomial, coefficient] : polynomial.terms) {
        poly_monomial rest;
        integer scale = coefficient;
        for (const poly_power& power : monomial.powers) {
            const auto value = values.find(power.variable);
            if (value == values.end()) {
                rest.powers.push_back(power);
                continue;
            }
            scale *= boost::multiprecision::pow(value->second, power.exponent);
        }
        integer& entry = result.terms.try_emplace(rest, integer(0)).first->second;
        entry += scale;
        if (entry == 0)
            result.terms.erase(rest);
    }
    return result;
}

RAT_HOLE_BINDER_TEMPLATE std::set<uint32_t> RAT_HOLE_BINDER::find_variables(const poly_polynomial& polynomial) {
    std::set<uint32_t> variables;
    for (const auto& [monomial, coefficient] : polynomial.terms)
        for (const poly_power& power : monomial.powers)
            variables.insert(power.variable);
    return variables;
}

#undef RAT_HOLE_BINDER
#undef RAT_HOLE_BINDER_TEMPLATE

#endif
