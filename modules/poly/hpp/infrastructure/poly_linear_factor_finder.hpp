#ifndef POLY_LINEAR_FACTOR_FINDER_HPP
#define POLY_LINEAR_FACTOR_FINDER_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>
#include "value_objects/integer.hpp"
#include "value_objects/poly_polynomial.hpp"

// Candidate linear factors `a1*v1 + ... + ak*vk + d` (d != 0) of a multivariate polynomial, found by restriction.
// If L divides P, then setting every variable but v to zero turns L into `a_v * v + d`, which divides the
// univariate restriction P_v. So each root r_v of P_v (by the rational root theorem) fixes the ratio a_v / d = -1/r_v.
// Every choice of one root, or none, per variable rebuilds a candidate L with the same d, scaled to integer
// coefficients with no common factor. The candidates still have to be checked by exact division: a combination
// of roots from different factors can produce a form that divides nothing.
//
// Needs a nonzero constant term (a factor through the origin restricts to `a_v * v`, which carries no root
// information); content extraction covers those.
struct poly_linear_factor_finder {
    poly_linear_factor_finder(size_t max_candidates);
    std::vector<poly_polynomial> find_linear_factors(const poly_polynomial& polynomial) const;
private:
    struct rational {
        integer numerator;
        integer denominator;
    };

    static std::vector<rational> find_rational_roots(const std::map<uint32_t, integer>& coefficients);
    static std::vector<integer> find_divisors(const integer& value);

    size_t max_candidates_;
};

#endif
