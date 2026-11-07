#include "infrastructure/poly_divisibility_filter.hpp"

poly_divisibility_filter::poly_divisibility_filter(size_t points)
    : points_(points) {}

bool poly_divisibility_filter::may_divide(const poly_polynomial& dividend, const poly_polynomial& divisor) const {
    for (size_t point = 0; point < points_; ++point) {
        const integer at_divisor = evaluate(divisor, point);
        if (at_divisor == 0)
            continue;
        if (evaluate(dividend, point) % at_divisor != 0)
            return false;
    }
    return true;
}

integer poly_divisibility_filter::evaluate(const poly_polynomial& polynomial, size_t point) const {
    integer sum = 0;
    for (const auto& [monomial, coefficient] : polynomial.terms) {
        integer term = coefficient;
        for (const poly_power& power : monomial.powers)
            term *= boost::multiprecision::pow(integer(find_value(power.variable, point)), power.exponent);
        sum += term;
    }
    return sum;
}

// Small values that differ across variables and points, so a wrong divisor rarely divides by coincidence.
int64_t poly_divisibility_filter::find_value(uint32_t variable, size_t point) {
    static constexpr int64_t k_values[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
    return k_values[(variable * 5 + point * 7) % 12] * (point % 2 == 0 ? 1 : -1);
}
