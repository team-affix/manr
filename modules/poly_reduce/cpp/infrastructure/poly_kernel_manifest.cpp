#include "infrastructure/poly_kernel_manifest.hpp"

poly_kernel_manifest::poly_kernel_manifest(size_t max_terms)
    : symbols_()
    , exprs_()
    , sizes_()
    , poly_symbols_{
          .add = symbols_.intern_symbol("add"),
          .sub = symbols_.intern_symbol("sub"),
          .mul = symbols_.intern_symbol("mul"),
          .pow = symbols_.intern_symbol("pow")}
    , literals_(symbols_, exprs_)
    , arithmetic_(max_terms)
    , linear_factors_(64)
    , expander_(literals_, arithmetic_, arithmetic_, arithmetic_, arithmetic_, poly_symbols_)
    , renderer_(exprs_, literals_, poly_symbols_)
    , horner_(exprs_, literals_, renderer_, poly_symbols_)
    , roots_(arithmetic_, arithmetic_, arithmetic_, arithmetic_, arithmetic_, 4)
    , parser_(symbols_, exprs_, literals_, literals_, poly_symbols_)
    , printer_(symbols_, poly_symbols_) {}
