#include "value_objects/poly_monomial_order.hpp"
#include <cstddef>
#include <cstdint>

bool poly_monomial_order::operator()(const poly_monomial& lhs, const poly_monomial& rhs) const {
    uint64_t lhs_degree = 0;
    uint64_t rhs_degree = 0;
    for (const poly_power& power : lhs.powers)
        lhs_degree += power.exponent;
    for (const poly_power& power : rhs.powers)
        rhs_degree += power.exponent;
    if (lhs_degree != rhs_degree)
        return lhs_degree < rhs_degree;
    size_t l = 0;
    size_t r = 0;
    while (l < lhs.powers.size() && r < rhs.powers.size()) {
        const poly_power& lp = lhs.powers[l];
        const poly_power& rp = rhs.powers[r];
        if (lp.variable < rp.variable)
            return false;
        if (rp.variable < lp.variable)
            return true;
        if (lp.exponent != rp.exponent)
            return lp.exponent < rp.exponent;
        ++l;
        ++r;
    }
    return l == lhs.powers.size() && r < rhs.powers.size();
}
