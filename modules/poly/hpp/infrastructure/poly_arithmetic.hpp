#ifndef POLY_ARITHMETIC_HPP
#define POLY_ARITHMETIC_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include "value_objects/integer.hpp"
#include "value_objects/poly_monomial.hpp"
#include "value_objects/poly_polynomial.hpp"

// Exact arithmetic on normal forms, with arbitrary-precision coefficients. An operation has no result (nullopt) only
// when the result would have more than `max_terms` terms, so a caller can always fall back to leaving an expression
// unexpanded.
// A normal form of this many terms takes well under a second to compute and divide.
constexpr size_t k_poly_default_max_terms = 4096;

struct poly_arithmetic {
    poly_arithmetic(size_t max_terms = k_poly_default_max_terms);
    std::optional<poly_polynomial> add(const poly_polynomial& lhs, const poly_polynomial& rhs) const;
    std::optional<poly_polynomial> subtract(const poly_polynomial& lhs, const poly_polynomial& rhs) const;
    std::optional<poly_polynomial> multiply(const poly_polynomial& lhs, const poly_polynomial& rhs) const;
    std::optional<poly_polynomial> power(const poly_polynomial& base, uint32_t exponent) const;
    // The exact quotient, if `divisor` divides `dividend` over the integers.
    std::optional<poly_polynomial> find_quotient(const poly_polynomial& dividend, const poly_polynomial& divisor) const;
private:
    std::optional<poly_polynomial> combine(const poly_polynomial& lhs, const poly_polynomial& rhs, int sign) const;

    size_t max_terms_;
};

#endif
