#include "composition/poly_local_rules_manifest.hpp"

poly_local_rules_manifest::poly_local_rules_manifest(kernel_t& kernel)
    : low_level_(kernel.literals_, kernel.literals_, kernel.exprs_, kernel.poly_symbols_, kernel.limits_.max_exponent)
    , local_rules_(low_level_)
    , root_rules_()
    , subterms_(local_rules_, kernel.exprs_)
    , rules_(subterms_, root_rules_) {}
