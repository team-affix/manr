#ifndef RAT_FRACTION_COMBINER_HPP
#define RAT_FRACTION_COMBINER_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_symbols.hpp"
#include "value_objects/rat_fraction.hpp"
#include "value_objects/rat_symbols.hpp"
#include "value_objects/rewrite.hpp"

constexpr uint32_t k_rat_default_max_exponent = 16;

// Local rule: reads a sum, difference, product or power of quotients and polynomials as one quotient `n / d` and
// cancels it: quotients over one denominator add their numerators (`x / (x + 1) + 1 / (x + 1)` is 1), others meet
// over the product of their denominators (`1 / (x - 1) - 1 / (x + 1)` is `2 / (x^2 - 1)`), a product folds its
// factors into one quotient (`(x^2 + 1) / (x + 1) * (x + 1)` is `x^2 + 1`), and the result loses its common factor
// (`poly_arithmetic::find_gcd`, in any number of variables) and integer content, with a positive leading coefficient
// below. The combined form also has a value where a cancelled factor was 0 (`rat_quotient_canceller`). Fires only when
// smaller, on an expression holding a quotient below its root (a lone quotient is the canceller's).
template<typename IFindPolynomial, typename IAddPolynomials, typename ISubtractPolynomials,
         typename IMultiplyPolynomials, typename IPowerPolynomial, typename IFindQuotient, typename IFindGcd,
         typename IFindLiteralValue, typename IRender, typename IMakeFunctor, typename IFindCost>
struct rat_fraction_combiner {
    rat_fraction_combiner(IFindPolynomial& find_polynomial,
                          IAddPolynomials& add_polynomials,
                          ISubtractPolynomials& subtract_polynomials,
                          IMultiplyPolynomials& multiply_polynomials,
                          IPowerPolynomial& power_polynomial,
                          IFindQuotient& find_quotient,
                          IFindGcd& find_gcd,
                          IFindLiteralValue& find_literal_value,
                          IRender& render,
                          IMakeFunctor& make_functor,
                          IFindCost& find_cost,
                          rat_symbols symbols,
                          poly_symbols polynomial_symbols,
                          uint32_t max_exponent = k_rat_default_max_exponent);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    bool holds_quotient(const expr* e);
    std::optional<rat_fraction> find_fraction(const expr* e, size_t& quotients);
    std::optional<rat_fraction> combine_sum(const rat_fraction& lhs, const rat_fraction& rhs, bool subtracted);
    std::optional<rat_fraction> combine_product(const rat_fraction& lhs, const rat_fraction& rhs);
    const expr* render_cancelled(const rat_fraction& fraction);

    IFindPolynomial& find_polynomial_;
    IAddPolynomials& add_polynomials_;
    ISubtractPolynomials& subtract_polynomials_;
    IMultiplyPolynomials& multiply_polynomials_;
    IPowerPolynomial& power_polynomial_;
    IFindQuotient& find_quotient_;
    IFindGcd& find_gcd_;
    IFindLiteralValue& find_literal_value_;
    IRender& render_;
    IMakeFunctor& make_functor_;
    IFindCost& find_cost_;
    rat_symbols symbols_;
    poly_symbols polynomial_symbols_;
    uint32_t max_exponent_;
    std::unordered_map<const expr*, bool> holding_;
};

#define RAT_FRACTION_COMBINER_TEMPLATE                                                                          \
    template<typename IFindPolynomial, typename IAddPolynomials, typename ISubtractPolynomials,                \
             typename IMultiplyPolynomials, typename IPowerPolynomial, typename IFindQuotient, typename IFindGcd, \
             typename IFindLiteralValue, typename IRender, typename IMakeFunctor, typename IFindCost>
#define RAT_FRACTION_COMBINER                                                                                  \
    rat_fraction_combiner<IFindPolynomial, IAddPolynomials, ISubtractPolynomials, IMultiplyPolynomials,        \
                          IPowerPolynomial, IFindQuotient, IFindGcd, IFindLiteralValue, IRender, IMakeFunctor, \
                          IFindCost>

RAT_FRACTION_COMBINER_TEMPLATE RAT_FRACTION_COMBINER::rat_fraction_combiner(
    IFindPolynomial& find_polynomial,
    IAddPolynomials& add_polynomials,
    ISubtractPolynomials& subtract_polynomials,
    IMultiplyPolynomials& multiply_polynomials,
    IPowerPolynomial& power_polynomial,
    IFindQuotient& find_quotient,
    IFindGcd& find_gcd,
    IFindLiteralValue& find_literal_value,
    IRender& render,
    IMakeFunctor& make_functor,
    IFindCost& find_cost,
    rat_symbols symbols,
    poly_symbols polynomial_symbols,
    uint32_t max_exponent)
    : find_polynomial_(find_polynomial)
    , add_polynomials_(add_polynomials)
    , subtract_polynomials_(subtract_polynomials)
    , multiply_polynomials_(multiply_polynomials)
    , power_polynomial_(power_polynomial)
    , find_quotient_(find_quotient)
    , find_gcd_(find_gcd)
    , find_literal_value_(find_literal_value)
    , render_(render)
    , make_functor_(make_functor)
    , find_cost_(find_cost)
    , symbols_(symbols)
    , polynomial_symbols_(polynomial_symbols)
    , max_exponent_(max_exponent)
    , holding_() {}

RAT_FRACTION_COMBINER_TEMPLATE void RAT_FRACTION_COMBINER::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.empty())
        return;
    const bool combining = f->id == polynomial_symbols_.add || f->id == polynomial_symbols_.sub
                           || f->id == polynomial_symbols_.mul || f->id == polynomial_symbols_.pow
                           || f->id == symbols_.quotient;
    if (!combining || !holds_quotient(e))
        return;
    size_t quotients = 0;
    const std::optional<rat_fraction> fraction = find_fraction(e, quotients);
    const bool lone = f->id == symbols_.quotient && quotients == 1;
    if (!fraction.has_value() || quotients == 0 || lone)
        return;
    const expr* combined = render_cancelled(fraction.value());
    if (combined == nullptr || find_cost_.cost(combined) >= find_cost_.cost(e))
        return;
    out.push_back(ready(combined));
}

// whether a quotient occurs in e: the cheap refutation before any polynomial work, memoized (expressions are interned)
RAT_FRACTION_COMBINER_TEMPLATE bool RAT_FRACTION_COMBINER::holds_quotient(const expr* e) {
    const auto known = holding_.find(e);
    if (known != holding_.end())
        return known->second;
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    bool holds = f != nullptr && f->id == symbols_.quotient;
    for (size_t i = 0; f != nullptr && i < f->args.size() && !holds; ++i)
        holds = holds_quotient(f->args[i]);
    holding_.emplace(e, holds);
    return holds;
}

// e as one quotient, counting the quotients it holds; nothing outside sums, products, powers and quotients of
// polynomials, for a denominator that is 0, or past the arithmetic's term cap
RAT_FRACTION_COMBINER_TEMPLATE std::optional<rat_fraction> RAT_FRACTION_COMBINER::find_fraction(const expr* e,
                                                                                               size_t& quotients) {
    const std::optional<poly_polynomial> polynomial = find_polynomial_.find_polynomial(e);
    if (polynomial.has_value())
        return rat_fraction{.numerator = polynomial.value(),
                            .denominator = poly_polynomial{.terms = {{poly_monomial{}, integer(1)}}}};
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.empty())
        return std::nullopt;
    const bool power = f->id == polynomial_symbols_.pow && f->args.size() == 2;
    if (power) {
        const std::optional<integer> exponent = find_literal_value_.find_value(f->args[1]);
        const bool small = exponent.has_value() && exponent.value() >= 0 && exponent.value() <= max_exponent_;
        if (!small)
            return std::nullopt;
        const std::optional<rat_fraction> base = find_fraction(f->args[0], quotients);
        if (!base.has_value())
            return std::nullopt;
        const uint32_t count = static_cast<uint32_t>(exponent.value());
        const std::optional<poly_polynomial> top = power_polynomial_.power(base.value().numerator, count);
        const std::optional<poly_polynomial> bottom = power_polynomial_.power(base.value().denominator, count);
        if (!top.has_value() || !bottom.has_value())
            return std::nullopt;
        return rat_fraction{.numerator = top.value(), .denominator = bottom.value()};
    }
    const bool quotient = f->id == symbols_.quotient && f->args.size() == 2;
    const bool sum = f->id == polynomial_symbols_.add || (f->id == polynomial_symbols_.sub && f->args.size() == 2);
    const bool product = f->id == polynomial_symbols_.mul;
    if (!quotient && !sum && !product)
        return std::nullopt;
    std::optional<rat_fraction> result = find_fraction(f->args[0], quotients);
    for (size_t i = 1; i < f->args.size() && result.has_value(); ++i) {
        const std::optional<rat_fraction> next = find_fraction(f->args[i], quotients);
        if (!next.has_value())
            return std::nullopt;
        if (sum)
            result = combine_sum(result.value(), next.value(), f->id == polynomial_symbols_.sub);
        if (product)
            result = combine_product(result.value(), next.value());
        if (!quotient)
            continue;
        if (next.value().numerator.terms.empty())
            return std::nullopt;
        ++quotients;
        result = combine_product(result.value(), rat_fraction{.numerator = next.value().denominator,
                                                              .denominator = next.value().numerator});
    }
    return result;
}

// a / b + c / d: (a + c) / b when b = d, (a * d + c * b) / (b * d) otherwise
RAT_FRACTION_COMBINER_TEMPLATE std::optional<rat_fraction> RAT_FRACTION_COMBINER::combine_sum(
    const rat_fraction& lhs, const rat_fraction& rhs, bool subtracted) {
    const bool shared = lhs.denominator == rhs.denominator;
    const std::optional<poly_polynomial> left =
        shared ? lhs.numerator : multiply_polynomials_.multiply(lhs.numerator, rhs.denominator);
    const std::optional<poly_polynomial> right =
        shared ? rhs.numerator : multiply_polynomials_.multiply(rhs.numerator, lhs.denominator);
    const std::optional<poly_polynomial> below =
        shared ? lhs.denominator : multiply_polynomials_.multiply(lhs.denominator, rhs.denominator);
    if (!left.has_value() || !right.has_value() || !below.has_value())
        return std::nullopt;
    const std::optional<poly_polynomial> above = subtracted ? subtract_polynomials_.subtract(left.value(), right.value())
                                                            : add_polynomials_.add(left.value(), right.value());
    if (!above.has_value())
        return std::nullopt;
    return rat_fraction{.numerator = above.value(), .denominator = below.value()};
}

RAT_FRACTION_COMBINER_TEMPLATE std::optional<rat_fraction> RAT_FRACTION_COMBINER::combine_product(
    const rat_fraction& lhs, const rat_fraction& rhs) {
    const std::optional<poly_polynomial> above = multiply_polynomials_.multiply(lhs.numerator, rhs.numerator);
    const std::optional<poly_polynomial> below = multiply_polynomials_.multiply(lhs.denominator, rhs.denominator);
    if (!above.has_value() || !below.has_value())
        return std::nullopt;
    return rat_fraction{.numerator = above.value(), .denominator = below.value()};
}

// The fraction without its common factor and integer content, its denominator's leading coefficient positive: a
// polynomial when the denominator divides, a quotient otherwise; nullptr when the gcd is out of reach.
RAT_FRACTION_COMBINER_TEMPLATE const expr* RAT_FRACTION_COMBINER::render_cancelled(const rat_fraction& fraction) {
    const std::optional<poly_polynomial> exact = find_quotient_.find_quotient(fraction.numerator, fraction.denominator);
    if (exact.has_value())
        return render_.render(exact.value());
    const std::optional<poly_polynomial> common = find_gcd_.find_gcd(fraction.numerator, fraction.denominator);
    if (!common.has_value())
        return nullptr;
    const std::optional<poly_polynomial> top = find_quotient_.find_quotient(fraction.numerator, common.value());
    const std::optional<poly_polynomial> bottom = find_quotient_.find_quotient(fraction.denominator, common.value());
    if (!top.has_value() || !bottom.has_value())
        return nullptr;
    integer content = 0;
    for (const poly_polynomial* side : {&top.value(), &bottom.value()})
        for (const auto& [monomial, coefficient] : side->terms)
            content = boost::multiprecision::gcd(content, coefficient);
    if (bottom.value().terms.rbegin()->second < 0)
        content = -content;
    rat_fraction cancelled;
    for (const auto& [monomial, coefficient] : top.value().terms)
        cancelled.numerator.terms.emplace(monomial, integer(coefficient / content));
    for (const auto& [monomial, coefficient] : bottom.value().terms)
        cancelled.denominator.terms.emplace(monomial, integer(coefficient / content));
    return make_functor_.make_functor(symbols_.quotient,
                                      {render_.render(cancelled.numerator), render_.render(cancelled.denominator)});
}

#undef RAT_FRACTION_COMBINER
#undef RAT_FRACTION_COMBINER_TEMPLATE

#endif
