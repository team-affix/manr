#ifndef POLY_FACTOR_DETECTOR_HPP
#define POLY_FACTOR_DETECTOR_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_symbols.hpp"
#include "value_objects/rewrite.hpp"

// Detector: factors a subterm as `d * (p / d)`. Detection decides: for each candidate divisor (at most
// `max_candidates` examined), the cheap divisibility filter rejects most; survivors get the exact division and an
// exact render-cost check. Only a d that divides p, leaves a nontrivial quotient, and gives a result no larger than
// the subterm becomes a rewrite. The rewrite only builds it. Candidates:
//   content      gcd of the coefficients times the monomial every term shares: 6x^2y + 9xy^2 -> 3xy(2x + 3y)
//   roots        a*v + c for each variable v, a in {1, 2, 3}, c = +-1, 2, 3 and +- the constant term's divisors
//                (the rational root theorem, for monic and near-monic factors): x^3 + 6x^2 + 11x + 6 -> (x+1)(...)
//   pairs        v +- w, v +- 2w, 2v +- w over pairs of variables: x^2 - y^2 + x + y -> (x + y)(x - y + 1)
//   restriction  multivariate linear factors with any coefficients, from the roots of each one-variable
//                restriction (`IFindLinearFactors`): ... -> (x - 3z + 2) * (...)
//   subterms     the normal form of every sum inside the subterm itself, since a factor is often already written
//                somewhere in the expression
// Repeated factors come out one per step; the quotient is proposed expanded and may be factored again.
constexpr size_t k_poly_default_max_factor_candidates = 128;
constexpr size_t k_poly_default_max_constant_divisors = 16;
constexpr uint64_t k_poly_default_max_constant_divisor = 1000;

template<typename IFindPolynomial, typename IMayDivide, typename IFindQuotient, typename IFindLinearFactors,
         typename IRender, typename IFindRenderCost, typename IMakeFunctor, typename IGetCost>
struct poly_factor_detector {
    poly_factor_detector(IFindPolynomial& find_polynomial,
                         IMayDivide& may_divide,
                         IFindQuotient& find_quotient,
                         IFindLinearFactors& find_linear_factors,
                         IRender& render,
                         IFindRenderCost& find_render_cost,
                         IMakeFunctor& make_functor,
                         IGetCost& get_cost,
                         poly_symbols symbols,
                         size_t max_candidates = k_poly_default_max_factor_candidates,
                         size_t max_constant_divisors = k_poly_default_max_constant_divisors,
                         uint64_t max_constant_divisor = k_poly_default_max_constant_divisor);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    void collect_subterm_candidates(const expr* e, std::set<poly_polynomial>& candidates);
    static poly_polynomial make_linear(uint32_t v, int a, uint32_t w, int b, const integer& constant);

    IFindPolynomial& find_polynomial_;
    IMayDivide& may_divide_;
    IFindQuotient& find_quotient_;
    IFindLinearFactors& find_linear_factors_;
    IRender& render_;
    IFindRenderCost& find_render_cost_;
    IMakeFunctor& make_functor_;
    IGetCost& get_cost_;
    poly_symbols symbols_;
    size_t max_candidates_;
    size_t max_constant_divisors_;
    uint64_t max_constant_divisor_;
};

#define POLY_FACTOR_DETECTOR_TEMPLATE                                                                         \
    template<typename IFindPolynomial, typename IMayDivide, typename IFindQuotient, typename IFindLinearFactors, \
             typename IRender, typename IFindRenderCost, typename IMakeFunctor, typename IGetCost>
#define POLY_FACTOR_DETECTOR                                                                                \
    poly_factor_detector<IFindPolynomial, IMayDivide, IFindQuotient, IFindLinearFactors, IRender, IFindRenderCost, \
                         IMakeFunctor, IGetCost>

POLY_FACTOR_DETECTOR_TEMPLATE POLY_FACTOR_DETECTOR::poly_factor_detector(
    IFindPolynomial& find_polynomial,
    IMayDivide& may_divide,
    IFindQuotient& find_quotient,
    IFindLinearFactors& find_linear_factors,
    IRender& render,
    IFindRenderCost& find_render_cost,
    IMakeFunctor& make_functor,
    IGetCost& get_cost,
    poly_symbols symbols,
    size_t max_candidates,
    size_t max_constant_divisors,
    uint64_t max_constant_divisor)
    : find_polynomial_(find_polynomial)
    , may_divide_(may_divide)
    , find_quotient_(find_quotient)
    , find_linear_factors_(find_linear_factors)
    , render_(render)
    , find_render_cost_(find_render_cost)
    , make_functor_(make_functor)
    , get_cost_(get_cost)
    , symbols_(symbols)
    , max_candidates_(max_candidates)
    , max_constant_divisors_(max_constant_divisors)
    , max_constant_divisor_(max_constant_divisor) {}

POLY_FACTOR_DETECTOR_TEMPLATE void POLY_FACTOR_DETECTOR::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.empty())
        return;
    const std::optional<poly_polynomial> found = find_polynomial_.find_polynomial(e);
    if (!found.has_value() || found.value().terms.size() < 2)
        return;
    const poly_polynomial& polynomial = found.value();
    std::set<poly_polynomial> candidates;

    // content
    integer gcd = 0;
    std::map<uint32_t, uint32_t> shared;
    std::map<uint32_t, size_t> occurrences;
    for (const auto& [monomial, coefficient] : polynomial.terms) {
        gcd = boost::multiprecision::gcd(gcd, coefficient);
        for (const poly_power& power : monomial.powers) {
            const bool first = !shared.contains(power.variable);
            shared[power.variable] = first ? power.exponent : std::min(shared[power.variable], power.exponent);
            ++occurrences[power.variable];
        }
    }
    poly_monomial common;
    for (const auto& [variable, exponent] : shared)
        if (occurrences[variable] == polynomial.terms.size())
            common.powers.push_back(poly_power{.variable = variable, .exponent = exponent});
    const int sign = polynomial.terms.rbegin()->second < 0 ? -1 : 1;
    if (gcd * sign != 1 || !common.powers.empty()) {
        poly_polynomial content;
        content.terms.emplace(common, integer(gcd * sign));
        candidates.insert(content);
    }

    // roots and pairs
    std::set<integer> offsets{1, 2, 3};
    const auto constant = polynomial.terms.find(poly_monomial{});
    const integer constant_term = constant == polynomial.terms.end() ? integer(0) : integer(boost::multiprecision::abs(constant->second));
    for (integer divisor = 1; divisor <= constant_term && divisor <= max_constant_divisor_ && offsets.size() < max_constant_divisors_;
         ++divisor)
        if (constant_term % divisor == 0)
            offsets.insert(divisor);
    std::set<uint32_t> variables;
    for (const auto& [monomial, coefficient] : polynomial.terms)
        for (const poly_power& power : monomial.powers)
            variables.insert(power.variable);
    for (const uint32_t v : variables) {
        for (const int a : {1, 2, 3})
            for (const integer& offset : offsets) {
                candidates.insert(make_linear(v, a, v, 0, offset));
                candidates.insert(make_linear(v, a, v, 0, integer(-offset)));
            }
        for (const uint32_t w : variables) {
            if (w <= v)
                continue;
            for (const int a : {1, 2})
                for (const int b : {1, 2, -1, -2})
                    candidates.insert(make_linear(v, a, w, b, 0));
        }
    }

    // restriction
    for (const poly_polynomial& linear : find_linear_factors_.find_linear_factors(polynomial))
        candidates.insert(linear);

    // subterms
    collect_subterm_candidates(e, candidates);
    candidates.erase(polynomial);

    const size_t cost = get_cost_.cost(e);
    size_t examined = 0;
    for (const poly_polynomial& candidate : candidates) {
        if (examined == max_candidates_)
            return;
        ++examined;
        if (!may_divide_.may_divide(polynomial, candidate))
            continue;
        const std::optional<poly_polynomial> quotient = find_quotient_.find_quotient(polynomial, candidate);
        if (!quotient.has_value())
            continue;
        const bool trivial = quotient.value().terms.size() == 1
            && quotient.value().terms.begin()->first.powers.empty()
            && boost::multiprecision::abs(quotient.value().terms.begin()->second) == 1;
        if (trivial)
            continue;
        const size_t factored_cost =
            1 + find_render_cost_.find_cost(candidate) + find_render_cost_.find_cost(quotient.value());
        if (factored_cost > cost)
            continue;
        out.push_back([this, candidate, quotient = quotient.value()]() {
            return make_functor_.make_functor(symbols_.mul, {render_.render(candidate), render_.render(quotient)});
        });
    }
}

POLY_FACTOR_DETECTOR_TEMPLATE void POLY_FACTOR_DETECTOR::collect_subterm_candidates(
    const expr* e, std::set<poly_polynomial>& candidates) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.empty())
        return;
    const std::optional<poly_polynomial> polynomial = find_polynomial_.find_polynomial(e);
    if (polynomial.has_value() && polynomial.value().terms.size() >= 2)
        candidates.insert(polynomial.value());
    for (const expr* arg : f->args)
        collect_subterm_candidates(arg, candidates);
}

// a*v + b*w + constant; pass b = 0 for a single variable.
POLY_FACTOR_DETECTOR_TEMPLATE poly_polynomial POLY_FACTOR_DETECTOR::make_linear(
    uint32_t v, int a, uint32_t w, int b, const integer& constant) {
    poly_polynomial linear;
    linear.terms.emplace(poly_monomial{.powers = {poly_power{.variable = v, .exponent = 1}}}, a);
    if (b != 0)
        linear.terms.emplace(poly_monomial{.powers = {poly_power{.variable = w, .exponent = 1}}}, b);
    if (constant != 0)
        linear.terms.emplace(poly_monomial{}, constant);
    return linear;
}

#undef POLY_FACTOR_DETECTOR
#undef POLY_FACTOR_DETECTOR_TEMPLATE

#endif
