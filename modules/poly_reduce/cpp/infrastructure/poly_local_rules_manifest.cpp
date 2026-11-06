#include "infrastructure/poly_local_rules_manifest.hpp"

poly_local_rules_manifest::poly_local_rules_manifest(kernel_t& kernel)
    : local_(kernel.literals_, kernel.literals_, kernel.exprs_, kernel.poly_symbols_)
    , rules_(local_, kernel.exprs_) {}
