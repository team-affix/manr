#include "infrastructure/poly_rules_manifest.hpp"
#include <string>

poly_rules_manifest::poly_rules_manifest(kernel_t& kernel)
    : local_(kernel.literals_, kernel.literals_, kernel.exprs_, kernel.poly_symbols_, kernel.limits_.max_exponent)
    , collector_(kernel.expander_, kernel.renderer_, kernel.renderer_, kernel.sizes_)
    , horner_(kernel.expander_, kernel.horner_, kernel.sizes_)
    , factor_(kernel.expander_, kernel.divisibility_, kernel.arithmetic_, kernel.linear_factors_, kernel.renderer_,
              kernel.renderer_, kernel.exprs_, kernel.sizes_,
              kernel.poly_symbols_, kernel.limits_.max_factor_candidates, kernel.limits_.max_constant_divisors,
              kernel.limits_.max_constant_divisor)
    , power_(kernel.expander_, kernel.roots_, kernel.renderer_, kernel.renderer_, kernel.exprs_, kernel.literals_, kernel.sizes_,
             kernel.poly_symbols_, kernel.limits_.max_power_degree)
    , splitter_(kernel.expander_, kernel.renderer_, kernel.exprs_, kernel.literals_, kernel.poly_symbols_)
    , like_terms_(kernel.expander_, kernel.literals_, kernel.exprs_, kernel.literals_, kernel.sizes_,
                  kernel.poly_symbols_)
    , union_(local_, collector_, horner_, factor_, power_, splitter_, like_terms_)
    , subterms_(union_, kernel.exprs_)
    , holes_(kernel.expander_, kernel.renderer_, kernel.renderer_, kernel.arithmetic_, kernel.arithmetic_,
             kernel.arithmetic_, kernel.arithmetic_, kernel.roots_, kernel.exprs_, kernel.exprs_, kernel.sizes_,
             intern_hole_symbols(kernel))
    , rules_(subterms_, holes_) {}

// the variables standing for the holes `?0` ... `?7` while the binder reads an expression as a polynomial
std::vector<uint32_t> poly_rules_manifest::intern_hole_symbols(kernel_t& kernel) {
    std::vector<uint32_t> symbols;
    for (int index = 0; index < 8; ++index)
        symbols.push_back(kernel.symbols_.intern_symbol("hole_" + std::to_string(index)));
    return symbols;
}
