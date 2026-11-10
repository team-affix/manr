#ifndef POLY_HOLE_BINDER_HPP
#define POLY_HOLE_BINDER_HPP

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
#include "value_objects/poly_root.hpp"
#include "value_objects/rewrite.hpp"

// Detector: binds a free variable `?k` of a polynomial expression to the polynomial that makes the whole smallest
// (`rule-design.md`, section 5). A hole is a hole at the syntax level, so `?k` may become any polynomial in the other
// variables, and the binding substitutes it everywhere: this reads the whole expression, so it is applied at the root
// only (a binding found in a subterm would change the rest). The expression is read as a polynomial in each hole, its
// holes standing for the variables `hole_symbols` (`?k` for the k-th), and the candidates are:
//
// - 0, the binding that drops every term holding the hole (`?0 * ?1` gives 0);
// - for a hole of degree 1, `c1 * ?k + c0`: the exact quotient `-c0 / c1`, which cancels everything (`x * ?0 + x * y`
//   with `?0 = -y` gives 0), or, when c1 is a constant, the nearest quotient coefficient by coefficient, which leaves
//   the smallest remainder (`2 * ?0 + 1` with `?0 = 0` gives 1);
// - for a hole of degree d > 1 that occurs only as `c * ?k^d`, c constant: the d-th roots of `-c0 / c` by completing
//   the power (`?0^2 - x^2 + 1` with `?0 = x` gives 1);
// - for a hole of degree d > 1 with a term of degree d - 1: the shift `-c(d-1) / (d * cd)`, when exact, which
//   completes the square (or the d-th power) and leaves what is not part of it (`(?0 - x)^2 + y` with `?0 = x`
//   gives y).
//
// When the expression is linear in its holes (no term holds two, or a power of one), they are also bound jointly to
// integers by solving the linear system that cancels the coefficient of every monomial of the other variables, or
// failing that every one but the constant (`?0 * x^2 + ?1 * x + ?0 - (x + 1)^2` with `?0 = 1, ?1 = 2` gives 0).
//
// Each candidate is substituted into the normal form, and proposed when the render is smaller than the expression.
// Nothing for an expression without holes, outside polynomial algebra, or holding a hole past `hole_symbols`.
template<typename IFindPolynomial, typename IRender, typename IFindRenderCost, typename IAddPolynomials,
         typename IMultiplyPolynomials, typename IPowerPolynomial, typename IFindQuotient, typename IFindRoots,
         typename IMakeFunctor, typename IMakeVar, typename IFindCost, typename ISolveLinearSystem>
struct poly_hole_binder {
    poly_hole_binder(IFindPolynomial& find_polynomial,
                     IRender& render,
                     IFindRenderCost& find_render_cost,
                     IAddPolynomials& add_polynomials,
                     IMultiplyPolynomials& multiply_polynomials,
                     IPowerPolynomial& power_polynomial,
                     IFindQuotient& find_quotient,
                     IFindRoots& find_roots,
                     IMakeFunctor& make_functor,
                     IMakeVar& make_var,
                     IFindCost& find_cost,
                     ISolveLinearSystem& solve_linear_system,
                     std::vector<uint32_t> hole_symbols);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    std::vector<poly_polynomial> split_by_degree(const poly_polynomial& polynomial, uint32_t symbol);
    void collect_candidates(const std::vector<poly_polynomial>& by_degree, std::vector<poly_polynomial>& candidates);
    std::optional<poly_polynomial> substitute(const std::vector<poly_polynomial>& by_degree,
                                              const poly_polynomial& binding);
    std::optional<poly_polynomial> bind_jointly(const poly_polynomial& polynomial, const std::set<uint32_t>& holes,
                                                bool cancel_constant);
    static std::optional<poly_polynomial> divide_exactly(const poly_polynomial& dividend, const integer& divisor);
    static poly_polynomial find_nearest_quotient(const poly_polynomial& dividend, const integer& divisor);
    static poly_polynomial negate(const poly_polynomial& polynomial);
    static std::optional<integer> find_constant(const poly_polynomial& polynomial);
    static bool holds_hole(const expr* e);
    const expr* fill_holes(const expr* e, std::set<uint32_t>& holes, bool& known);
    const expr* free_holes(const expr* e);

    IFindPolynomial& find_polynomial_;
    IRender& render_;
    IFindRenderCost& find_render_cost_;
    IAddPolynomials& add_polynomials_;
    IMultiplyPolynomials& multiply_polynomials_;
    IPowerPolynomial& power_polynomial_;
    IFindQuotient& find_quotient_;
    IFindRoots& find_roots_;
    IMakeFunctor& make_functor_;
    IMakeVar& make_var_;
    IFindCost& find_cost_;
    ISolveLinearSystem& solve_linear_system_;
    std::vector<uint32_t> hole_symbols_;
};

#define POLY_HOLE_BINDER_TEMPLATE                                                                                 \
    template<typename IFindPolynomial, typename IRender, typename IFindRenderCost, typename IAddPolynomials,      \
             typename IMultiplyPolynomials, typename IPowerPolynomial, typename IFindQuotient, typename IFindRoots, \
             typename IMakeFunctor, typename IMakeVar, typename IFindCost, typename ISolveLinearSystem>
#define POLY_HOLE_BINDER                                                                                         \
    poly_hole_binder<IFindPolynomial, IRender, IFindRenderCost, IAddPolynomials, IMultiplyPolynomials,           \
                     IPowerPolynomial, IFindQuotient, IFindRoots, IMakeFunctor, IMakeVar, IFindCost,           \
                     ISolveLinearSystem>

POLY_HOLE_BINDER_TEMPLATE POLY_HOLE_BINDER::poly_hole_binder(IFindPolynomial& find_polynomial,
                                                             IRender& render,
                                                             IFindRenderCost& find_render_cost,
                                                             IAddPolynomials& add_polynomials,
                                                             IMultiplyPolynomials& multiply_polynomials,
                                                             IPowerPolynomial& power_polynomial,
                                                             IFindQuotient& find_quotient,
                                                             IFindRoots& find_roots,
                                                             IMakeFunctor& make_functor,
                                                             IMakeVar& make_var,
                                                             IFindCost& find_cost,
                                                             ISolveLinearSystem& solve_linear_system,
                                                             std::vector<uint32_t> hole_symbols)
    : find_polynomial_(find_polynomial)
    , render_(render)
    , find_render_cost_(find_render_cost)
    , add_polynomials_(add_polynomials)
    , multiply_polynomials_(multiply_polynomials)
    , power_polynomial_(power_polynomial)
    , find_quotient_(find_quotient)
    , find_roots_(find_roots)
    , make_functor_(make_functor)
    , make_var_(make_var)
    , find_cost_(find_cost)
    , solve_linear_system_(solve_linear_system)
    , hole_symbols_(std::move(hole_symbols)) {}

POLY_HOLE_BINDER_TEMPLATE void POLY_HOLE_BINDER::detect(const expr* e, std::vector<rewrite>& out) {
    if (!holds_hole(e))
        return;
    std::set<uint32_t> holes;
    bool known = true;
    const expr* filled = fill_holes(e, holes, known);
    if (holes.empty() || !known)
        return;
    const std::optional<poly_polynomial> polynomial = find_polynomial_.find_polynomial(filled);
    if (!polynomial.has_value())
        return;
    const size_t cost = find_cost_.cost(e);
    std::set<poly_polynomial> proposed;
    for (const uint32_t hole : holes) {
        const std::vector<poly_polynomial> by_degree = split_by_degree(polynomial.value(), hole_symbols_[hole]);
        if (by_degree.size() < 2)
            continue;
        std::vector<poly_polynomial> candidates;
        collect_candidates(by_degree, candidates);
        for (const poly_polynomial& binding : candidates) {
            const std::optional<poly_polynomial> bound = substitute(by_degree, binding);
            if (!bound.has_value() || !proposed.insert(bound.value()).second)
                continue;
            if (find_render_cost_.find_cost(bound.value()) >= cost)
                continue;
            out.push_back([this, result = bound.value()]() { return free_holes(render_.render(result)); });
        }
    }
    if (holes.size() < 2)
        return;
    for (const bool cancel_constant : {true, false}) {
        const std::optional<poly_polynomial> bound = bind_jointly(polynomial.value(), holes, cancel_constant);
        if (!bound.has_value() || !proposed.insert(bound.value()).second)
            continue;
        if (find_render_cost_.find_cost(bound.value()) >= cost)
            continue;
        out.push_back([this, result = bound.value()]() { return free_holes(render_.render(result)); });
        return;
    }
}

// The polynomial with every hole bound to the integer that cancels the most coefficients: each monomial of the other
// variables is one equation, `sum_k c_k * A_k[m] = -A_0[m]`, in the holes' values c_k (its constant only when
// `cancel_constant`). Nothing when a term holds two holes or a power of one, or the system has no integer solution.
POLY_HOLE_BINDER_TEMPLATE std::optional<poly_polynomial> POLY_HOLE_BINDER::bind_jointly(
    const poly_polynomial& polynomial, const std::set<uint32_t>& holes, bool cancel_constant) {
    const std::vector<uint32_t> order(holes.begin(), holes.end());
    std::map<uint32_t, size_t> columns;
    for (size_t column = 0; column < order.size(); ++column)
        columns.emplace(hole_symbols_[order[column]], column);
    // per monomial of the other variables: the coefficient of each hole, then minus the hole-free coefficient
    std::map<poly_monomial, std::vector<integer>> equations;
    for (const auto& [monomial, coefficient] : polynomial.terms) {
        poly_monomial rest;
        std::optional<size_t> column;
        for (const poly_power& power : monomial.powers) {
            const auto hole = columns.find(power.variable);
            if (hole == columns.end()) {
                rest.powers.push_back(power);
                continue;
            }
            const bool linear = power.exponent == 1 && !column.has_value();
            if (!linear)
                return std::nullopt;
            column = hole->second;
        }
        std::vector<integer>& row = equations.try_emplace(rest, order.size() + 1, integer(0)).first->second;
        if (column.has_value())
            row[column.value()] += coefficient;
        else
            row[order.size()] -= coefficient;
    }
    std::vector<std::vector<integer>> rows;
    for (const auto& [monomial, row] : equations)
        if (cancel_constant || !monomial.powers.empty())
            rows.push_back(row);
    const std::optional<std::vector<integer>> values = solve_linear_system_.solve(rows, order.size());
    if (!values.has_value())
        return std::nullopt;
    poly_polynomial bound;
    for (const auto& [monomial, coefficient] : polynomial.terms) {
        poly_monomial rest;
        integer scale = 1;
        for (const poly_power& power : monomial.powers) {
            const auto hole = columns.find(power.variable);
            if (hole == columns.end())
                rest.powers.push_back(power);
            else
                scale = values.value()[hole->second];
        }
        integer& entry = bound.terms.try_emplace(rest, integer(0)).first->second;
        entry += coefficient * scale;
        if (entry == 0)
            bound.terms.erase(rest);
    }
    return bound;
}

// The coefficients of the polynomial in one variable, lowest degree first, each a polynomial in the others.
POLY_HOLE_BINDER_TEMPLATE std::vector<poly_polynomial> POLY_HOLE_BINDER::split_by_degree(
    const poly_polynomial& polynomial, uint32_t symbol) {
    std::vector<poly_polynomial> by_degree(1);
    for (const auto& [monomial, coefficient] : polynomial.terms) {
        poly_monomial rest;
        uint32_t degree = 0;
        for (const poly_power& power : monomial.powers) {
            if (power.variable == symbol) {
                degree = power.exponent;
                continue;
            }
            rest.powers.push_back(power);
        }
        if (by_degree.size() <= degree)
            by_degree.resize(degree + 1);
        by_degree[degree].terms.emplace(rest, coefficient);
    }
    return by_degree;
}

POLY_HOLE_BINDER_TEMPLATE void POLY_HOLE_BINDER::collect_candidates(const std::vector<poly_polynomial>& by_degree,
                                                                    std::vector<poly_polynomial>& candidates) {
    candidates.push_back(poly_polynomial{});
    const size_t degree = by_degree.size() - 1;
    const poly_polynomial target = negate(by_degree[0]);
    const std::optional<integer> leading = find_constant(by_degree[degree]);
    if (degree == 1) {
        const std::optional<poly_polynomial> exact = find_quotient_.find_quotient(target, by_degree[1]);
        if (exact.has_value())
            candidates.push_back(exact.value());
        if (!exact.has_value() && leading.has_value())
            candidates.push_back(find_nearest_quotient(target, leading.value()));
        return;
    }
    if (!by_degree[degree - 1].terms.empty()) {
        const std::optional<poly_polynomial> ratio = find_quotient_.find_quotient(negate(by_degree[degree - 1]),
                                                                                  by_degree[degree]);
        const std::optional<poly_polynomial> shift =
            ratio.has_value() ? divide_exactly(ratio.value(), integer(degree)) : std::nullopt;
        if (shift.has_value())
            candidates.push_back(shift.value());
    }
    bool pure = leading.has_value();
    for (size_t middle = 1; middle < degree && pure; ++middle)
        pure = by_degree[middle].terms.empty();
    if (!pure)
        return;
    const poly_polynomial scaled = find_nearest_quotient(target, leading.value());
    for (const poly_root& root : find_roots_.find_roots(scaled, static_cast<uint32_t>(degree)))
        candidates.push_back(root.root);
}

// sum of coefficient_i * binding^i
POLY_HOLE_BINDER_TEMPLATE std::optional<poly_polynomial> POLY_HOLE_BINDER::substitute(
    const std::vector<poly_polynomial>& by_degree, const poly_polynomial& binding) {
    poly_polynomial sum = by_degree[0];
    for (size_t degree = 1; degree < by_degree.size(); ++degree) {
        if (by_degree[degree].terms.empty())
            continue;
        const std::optional<poly_polynomial> power = power_polynomial_.power(binding, static_cast<uint32_t>(degree));
        if (!power.has_value())
            return std::nullopt;
        const std::optional<poly_polynomial> term = multiply_polynomials_.multiply(by_degree[degree], power.value());
        if (!term.has_value())
            return std::nullopt;
        const std::optional<poly_polynomial> next = add_polynomials_.add(sum, term.value());
        if (!next.has_value())
            return std::nullopt;
        sum = next.value();
    }
    return sum;
}

// dividend / divisor, when every coefficient divides
POLY_HOLE_BINDER_TEMPLATE std::optional<poly_polynomial> POLY_HOLE_BINDER::divide_exactly(
    const poly_polynomial& dividend, const integer& divisor) {
    poly_polynomial quotient;
    for (const auto& [monomial, coefficient] : dividend.terms) {
        if (coefficient % divisor != 0)
            return std::nullopt;
        quotient.terms.emplace(monomial, coefficient / divisor);
    }
    return quotient;
}

// q with every coefficient of `dividend - divisor * q` of magnitude at most |divisor| / 2
POLY_HOLE_BINDER_TEMPLATE poly_polynomial POLY_HOLE_BINDER::find_nearest_quotient(const poly_polynomial& dividend,
                                                                                  const integer& divisor) {
    poly_polynomial quotient;
    for (const auto& [monomial, coefficient] : dividend.terms) {
        integer nearest = coefficient / divisor;
        const integer remainder = coefficient - divisor * nearest;
        const bool far = 2 * boost::multiprecision::abs(remainder) > boost::multiprecision::abs(divisor);
        if (far)
            nearest += (remainder > 0) == (divisor > 0) ? 1 : -1;
        if (nearest != 0)
            quotient.terms.emplace(monomial, nearest);
    }
    return quotient;
}

POLY_HOLE_BINDER_TEMPLATE poly_polynomial POLY_HOLE_BINDER::negate(const poly_polynomial& polynomial) {
    poly_polynomial negated;
    for (const auto& [monomial, coefficient] : polynomial.terms)
        negated.terms.emplace(monomial, -coefficient);
    return negated;
}

POLY_HOLE_BINDER_TEMPLATE std::optional<integer> POLY_HOLE_BINDER::find_constant(const poly_polynomial& polynomial) {
    const bool constant = polynomial.terms.size() == 1 && polynomial.terms.begin()->first.powers.empty();
    if (!constant)
        return std::nullopt;
    return polynomial.terms.begin()->second;
}

POLY_HOLE_BINDER_TEMPLATE bool POLY_HOLE_BINDER::holds_hole(const expr* e) {
    if (std::holds_alternative<expr::var>(e->content))
        return true;
    for (const expr* arg : std::get<expr::functor>(e->content).args)
        if (holds_hole(arg))
            return true;
    return false;
}

// e with each hole `?k` replaced by the variable `hole_symbols[k]`; `known` is cleared by a hole past them
POLY_HOLE_BINDER_TEMPLATE const expr* POLY_HOLE_BINDER::fill_holes(const expr* e, std::set<uint32_t>& holes,
                                                                   bool& known) {
    const expr::var* hole = std::get_if<expr::var>(&e->content);
    if (hole != nullptr) {
        known = known && hole->index < hole_symbols_.size();
        if (!known)
            return e;
        holes.insert(hole->index);
        return make_functor_.make_functor(hole_symbols_[hole->index], {});
    }
    const expr::functor& f = std::get<expr::functor>(e->content);
    if (f.args.empty())
        return e;
    std::vector<const expr*> args;
    args.reserve(f.args.size());
    for (const expr* arg : f.args)
        args.push_back(fill_holes(arg, holes, known));
    return make_functor_.make_functor(f.id, args);
}

// the mirror of fill_holes: each variable `hole_symbols[k]` back to `?k`
POLY_HOLE_BINDER_TEMPLATE const expr* POLY_HOLE_BINDER::free_holes(const expr* e) {
    const expr::functor& f = std::get<expr::functor>(e->content);
    if (f.args.empty()) {
        for (uint32_t index = 0; index < hole_symbols_.size(); ++index)
            if (f.id == hole_symbols_[index])
                return make_var_.make_var(index);
        return e;
    }
    std::vector<const expr*> args;
    args.reserve(f.args.size());
    for (const expr* arg : f.args)
        args.push_back(free_holes(arg));
    return make_functor_.make_functor(f.id, args);
}

#undef POLY_HOLE_BINDER
#undef POLY_HOLE_BINDER_TEMPLATE

#endif
