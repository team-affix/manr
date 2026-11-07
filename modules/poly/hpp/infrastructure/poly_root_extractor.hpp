#ifndef POLY_ROOT_EXTRACTOR_HPP
#define POLY_ROOT_EXTRACTOR_HPP

#include <cstdint>
#include <optional>
#include <vector>
#include "value_objects/integer.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_root.hpp"

// Finds `p = q^n + r` with r as small as it can make it ("completing the power"), one attempt per term of p that is
// a perfect n-th power, tried from the largest (at most `max_starts`). From a start q = s, it repeatedly takes the
// largest term t of the remainder that n * s^(n-1) divides into a monomial smaller than q's last term, appends that
// quotient to q, and recomputes r = p - q^n. Terms that don't fit the pattern are skipped, not fatal, so
// `x^2 + 2x + y^2 + 2y + 2` gives `(x + 1)^2 + (y^2 + 2y + 1)`. When r = 0, p is an exact power.
constexpr size_t k_poly_default_max_root_starts = 4;

template<typename IAddPolynomials,
         typename ISubtractPolynomials,
         typename IMultiplyPolynomials,
         typename IPowerPolynomial,
         typename IFindQuotient>
struct poly_root_extractor {
    poly_root_extractor(IAddPolynomials& add_polynomials,
                        ISubtractPolynomials& subtract_polynomials,
                        IMultiplyPolynomials& multiply_polynomials,
                        IPowerPolynomial& power_polynomial,
                        IFindQuotient& find_quotient,
                        size_t max_starts = k_poly_default_max_root_starts);
    std::vector<poly_root> find_roots(const poly_polynomial& polynomial, uint32_t degree);
private:
    std::optional<poly_root> find_root_from(const poly_polynomial& polynomial, uint32_t degree, const poly_polynomial& start);
    static std::optional<integer> find_integer_root(const integer& value, uint32_t degree);

    IAddPolynomials& add_polynomials_;
    ISubtractPolynomials& subtract_polynomials_;
    IMultiplyPolynomials& multiply_polynomials_;
    IPowerPolynomial& power_polynomial_;
    IFindQuotient& find_quotient_;
    size_t max_starts_;
};

#define POLY_ROOT_EXTRACTOR_TEMPLATE                                                                       \
    template<typename IAddPolynomials, typename ISubtractPolynomials, typename IMultiplyPolynomials, \
             typename IPowerPolynomial, typename IFindQuotient>
#define POLY_ROOT_EXTRACTOR                                                                       \
    poly_root_extractor<IAddPolynomials, ISubtractPolynomials, IMultiplyPolynomials, IPowerPolynomial, \
                        IFindQuotient>

POLY_ROOT_EXTRACTOR_TEMPLATE POLY_ROOT_EXTRACTOR::poly_root_extractor(
    IAddPolynomials& add_polynomials,
    ISubtractPolynomials& subtract_polynomials,
    IMultiplyPolynomials& multiply_polynomials,
    IPowerPolynomial& power_polynomial,
    IFindQuotient& find_quotient,
    size_t max_starts)
    : add_polynomials_(add_polynomials)
    , subtract_polynomials_(subtract_polynomials)
    , multiply_polynomials_(multiply_polynomials)
    , power_polynomial_(power_polynomial)
    , find_quotient_(find_quotient)
    , max_starts_(max_starts) {}

POLY_ROOT_EXTRACTOR_TEMPLATE std::vector<poly_root> POLY_ROOT_EXTRACTOR::find_roots(
    const poly_polynomial& polynomial, uint32_t degree) {
    std::vector<poly_root> roots;
    size_t starts = 0;
    for (auto term = polynomial.terms.rbegin(); term != polynomial.terms.rend() && starts < max_starts_; ++term) {
        const std::optional<integer> coefficient = find_integer_root(term->second, degree);
        if (!coefficient.has_value())
            continue;
        poly_monomial monomial;
        bool divisible = !term->first.powers.empty();
        for (const poly_power& power : term->first.powers) {
            divisible = divisible && power.exponent % degree == 0;
            monomial.powers.push_back(poly_power{.variable = power.variable, .exponent = power.exponent / degree});
        }
        if (!divisible)
            continue;
        ++starts;
        poly_polynomial start;
        start.terms.emplace(monomial, coefficient.value());
        const std::optional<poly_root> root = find_root_from(polynomial, degree, start);
        if (!root.has_value())
            continue;
        roots.push_back(root.value());
    }
    return roots;
}

POLY_ROOT_EXTRACTOR_TEMPLATE std::optional<poly_root> POLY_ROOT_EXTRACTOR::find_root_from(
    const poly_polynomial& polynomial, uint32_t degree, const poly_polynomial& start) {
    poly_polynomial scale;
    scale.terms.emplace(poly_monomial{}, integer(degree));
    const std::optional<poly_polynomial> start_power = power_polynomial_.power(start, degree - 1);
    if (!start_power.has_value())
        return std::nullopt;
    const std::optional<poly_polynomial> divisor = multiply_polynomials_.multiply(scale, start_power.value());
    if (!divisor.has_value())
        return std::nullopt;
    poly_polynomial root = start;
    poly_monomial last = start.terms.begin()->first;
    const poly_monomial_order order;
    std::optional<poly_polynomial> remainder = std::nullopt;
    for (size_t step = 0; step <= polynomial.terms.size(); ++step) {
        const std::optional<poly_polynomial> raised = power_polynomial_.power(root, degree);
        if (!raised.has_value())
            return std::nullopt;
        remainder = subtract_polynomials_.subtract(polynomial, raised.value());
        if (!remainder.has_value())
            return std::nullopt;
        std::optional<poly_polynomial> next_term = std::nullopt;
        for (auto term = remainder.value().terms.rbegin(); term != remainder.value().terms.rend(); ++term) {
            poly_polynomial single;
            single.terms.emplace(term->first, term->second);
            const std::optional<poly_polynomial> quotient = find_quotient_.find_quotient(single, divisor.value());
            if (!quotient.has_value() || !order(quotient.value().terms.begin()->first, last))
                continue;
            next_term = quotient;
            break;
        }
        if (!next_term.has_value())
            break;
        last = next_term.value().terms.begin()->first;
        const std::optional<poly_polynomial> grown = add_polynomials_.add(root, next_term.value());
        if (!grown.has_value())
            return std::nullopt;
        root = grown.value();
    }
    return poly_root{.root = root, .degree = degree, .remainder = remainder.value()};
}

// The exact integer n-th root of value, if it has one, by bisection on [1, 2^(bits / n + 1)].
POLY_ROOT_EXTRACTOR_TEMPLATE std::optional<integer> POLY_ROOT_EXTRACTOR::find_integer_root(
    const integer& value, uint32_t degree) {
    const bool even = degree % 2 == 0;
    if (value == 0 || (even && value < 0))
        return std::nullopt;
    const integer magnitude = boost::multiprecision::abs(value);
    integer low = 1;
    integer high = integer(1) << (boost::multiprecision::msb(magnitude) / degree + 1);
    while (low <= high) {
        const integer middle = (low + high) / 2;
        const integer raised = boost::multiprecision::pow(middle, degree);
        if (raised == magnitude)
            return value < 0 ? integer(-middle) : middle;
        if (raised < magnitude) low = middle + 1;
        else high = middle - 1;
    }
    return std::nullopt;
}

#undef POLY_ROOT_EXTRACTOR
#undef POLY_ROOT_EXTRACTOR_TEMPLATE

#endif
