#include "composition/arith_kernel_manifest.hpp"

arith_kernel_manifest::arith_kernel_manifest()
    : symbols_()
    , exprs_()
    , sizes_()
    , arith_symbols_{
          .input = symbols_.intern_symbol("#0"),
          .add = symbols_.intern_symbol("add"),
          .sub = symbols_.intern_symbol("sub"),
          .mul = symbols_.intern_symbol("mul")}
    , literals_(symbols_, exprs_)
    , printer_(symbols_) {}
