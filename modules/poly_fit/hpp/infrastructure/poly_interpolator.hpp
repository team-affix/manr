#ifndef POLY_INTERPOLATOR_HPP
#define POLY_INTERPOLATOR_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
#include "value_objects/integer.hpp"
#include "value_objects/poly_interpolant.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_sample.hpp"

constexpr size_t k_poly_fit_default_max_unknowns = 84;

// Exact polynomial interpolation over the integers, in any number of variables. For total degree d the unknowns are
// the coefficients of every monomial of degree <= d. Samples are taken in order from a starting index (wrapping),
// skipping any that add no new constraint (Gauss-Jordan elimination over the rationals), until every coefficient is
// pinned. Only integer coefficients make a polynomial. A sample's i-th input is the i-th variable.
struct poly_interpolator {
    poly_interpolator(std::vector<uint32_t> variables, size_t max_unknowns = k_poly_fit_default_max_unknowns);
    // Nothing when there are more unknowns than the limit, too few samples to also check one beyond them, the
    // samples run out first, or a coefficient is a proper fraction.
    std::optional<poly_interpolant> interpolate(const std::vector<poly_sample>& samples, size_t degree, size_t start) const;
    integer evaluate(const poly_polynomial& polynomial, const std::vector<integer>& inputs) const;
private:
    size_t count_monomials(size_t degree) const;
    void extend_monomials(size_t index, size_t remaining, poly_monomial& current, std::vector<poly_monomial>& out) const;
    integer evaluate_monomial(const poly_monomial& monomial, const std::vector<integer>& inputs) const;

    std::vector<uint32_t> variables_;           // in input order: inputs[i] is variables_[i]
    std::vector<uint32_t> sorted_variables_;    // in id order, as monomials list their powers
    size_t max_unknowns_;
};

#endif
