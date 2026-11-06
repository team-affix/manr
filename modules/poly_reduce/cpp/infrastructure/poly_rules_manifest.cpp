#include "infrastructure/poly_rules_manifest.hpp"

poly_rules_manifest::poly_rules_manifest(kernel_t& kernel)
    : local_(kernel.literals_, kernel.literals_, kernel.exprs_, kernel.poly_symbols_)
    , collector_(kernel.expander_, kernel.renderer_, kernel.sizes_)
    , horner_(kernel.expander_, kernel.horner_, kernel.sizes_)
    , factor_(kernel.expander_, kernel.arithmetic_, kernel.linear_factors_, kernel.renderer_, kernel.exprs_, kernel.sizes_,
              kernel.poly_symbols_, 6)
    , power_(kernel.expander_, kernel.roots_, kernel.renderer_, kernel.exprs_, kernel.literals_, kernel.sizes_,
             kernel.poly_symbols_, 8)
    , splitter_(kernel.expander_, kernel.renderer_, kernel.exprs_, kernel.literals_, kernel.poly_symbols_)
    , like_terms_(kernel.expander_, kernel.literals_, kernel.exprs_, kernel.literals_, kernel.sizes_,
                  kernel.poly_symbols_)
    , union_(local_, collector_, horner_, factor_, power_, splitter_, like_terms_)
    , rules_(union_, kernel.exprs_) {}
