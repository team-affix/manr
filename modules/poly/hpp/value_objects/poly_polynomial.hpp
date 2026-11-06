#ifndef POLY_POLYNOMIAL_HPP
#define POLY_POLYNOMIAL_HPP

#include <compare>
#include <map>
#include "value_objects/integer.hpp"
#include "value_objects/poly_monomial.hpp"
#include "value_objects/poly_monomial_order.hpp"

// The expanded normal form of a polynomial over the integers: monomial -> nonzero coefficient, in ascending monomial
// order (so the leading term is `terms.rbegin()`). Two expressions are equal as polynomials iff their normal forms
// are equal, which makes this the oracle every rule can be checked against.
struct poly_polynomial {
    std::map<poly_monomial, integer, poly_monomial_order> terms;
    auto operator<=>(const poly_polynomial&) const = default;
};

#endif
