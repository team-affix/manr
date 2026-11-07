#ifndef POLY_DIVISIBILITY_FILTER_HPP
#define POLY_DIVISIBILITY_FILTER_HPP

#include <cstddef>
#include <cstdint>
#include "value_objects/integer.hpp"
#include "value_objects/poly_polynomial.hpp"

constexpr size_t k_poly_default_divisibility_points = 3;

// A necessary condition for exact division, checked before doing one. If d divides p over the integers, then d(a)
// divides p(a) at every integer point a. At each of `points` fixed small points it evaluates both and rejects on the
// first point where d(a) != 0 does not divide p(a). It never accepts: passing means only "may divide".
// Cost: one evaluation of each polynomial per point, against an exact division's many multiplications.
struct poly_divisibility_filter {
    poly_divisibility_filter(size_t points = k_poly_default_divisibility_points);
    bool may_divide(const poly_polynomial& dividend, const poly_polynomial& divisor) const;
private:
    integer evaluate(const poly_polynomial& polynomial, size_t point) const;
    static int64_t find_value(uint32_t variable, size_t point);

    size_t points_;
};

#endif
