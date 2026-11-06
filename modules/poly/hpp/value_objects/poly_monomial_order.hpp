#ifndef POLY_MONOMIAL_ORDER_HPP
#define POLY_MONOMIAL_ORDER_HPP

#include "value_objects/poly_monomial.hpp"

// Graded lexicographic order: higher total degree is greater; ties go to the larger exponent of the first variable
// (lowest id) where the two differ. A polynomial's leading term is its greatest monomial under this order.
struct poly_monomial_order {
    bool operator()(const poly_monomial& lhs, const poly_monomial& rhs) const;
};

#endif
