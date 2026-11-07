#ifndef POLY_LIMITS_HPP
#define POLY_LIMITS_HPP

#include <compare>
#include <cstddef>
#include <cstdint>

// Every limit the polynomial rules observe, supplied from the outermost level (a solver manifest's constructor).
// Each one bounds how much work one rule may do on one subterm; none bounds the expression as a whole.
struct poly_limits {
    size_t max_terms;                  // largest normal form any computation may produce or pass through
    uint32_t max_exponent;             // largest exponent the expander expands and the local rules fold
    uint32_t max_power_degree;         // largest n the power detector tries for q^n + r
    size_t max_root_starts;            // starting terms the root extractor tries per degree
    size_t max_factor_candidates;      // candidate divisors the factor detector examines per subterm
    size_t divisibility_points;        // evaluation points the divisibility filter checks per candidate
    size_t max_constant_divisors;      // constant-term divisors the factor detector tries as offsets
    uint64_t max_constant_divisor;     // largest such divisor
    size_t max_linear_candidates;      // candidates the linear factor finder returns per polynomial
    uint64_t max_root_divisor;         // largest numerator/denominator the rational root search tries
    auto operator<=>(const poly_limits&) const = default;
};

#endif
