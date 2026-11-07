#include "infrastructure/arith_fit_manifest.hpp"

arith_fit_manifest::arith_fit_manifest(const std::vector<arith_sample>& samples, size_t frontier_capacity)
    : symbols_()
    , exprs_()
    , sizes_()
    , arith_symbols_{
          .input = symbols_.intern_symbol("x"),
          .add = symbols_.intern_symbol("add"),
          .sub = symbols_.intern_symbol("sub"),
          .mul = symbols_.intern_symbol("mul")}
    , pw_symbols_{.piecewise = symbols_.intern_symbol("pw"),
                  .branch = symbols_.intern_symbol("case"),
                  .region = symbols_.intern_symbol("when")}
    , literals_(symbols_, exprs_)
    , converter_(literals_)
    , evaluator_(literals_, literals_, arith_symbols_)
    , assembler_(exprs_, pw_symbols_)
    , builder_(exprs_, exprs_, assembler_, pw_symbols_)
    , filter_(evaluator_, pw_symbols_)
    , constants_()
    , lines_(literals_, literals_, exprs_, arith_symbols_)
    , hypotheses_(constants_, lines_)
    , binder_(hypotheses_, filter_, assembler_, pw_symbols_)
    , absorber_(filter_, assembler_, pw_symbols_)
    , simplifier_(literals_, literals_, arith_symbols_)
    , local_rules_(binder_, absorber_, simplifier_)
    , rules_(local_rules_, exprs_)
    , initial_(builder_.build(converter_.convert(samples), 0))
    , search_(rules_, sizes_, initial_, frontier_capacity)
    , printer_(symbols_) {}
