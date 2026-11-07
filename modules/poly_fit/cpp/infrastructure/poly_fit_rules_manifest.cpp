#include "infrastructure/poly_fit_rules_manifest.hpp"

poly_fit_rules_manifest::poly_fit_rules_manifest(
    kernel_t& kernel,
    const std::vector<std::string>& variables,
    const std::vector<poly_sample>& samples,
    const poly_fit_limits& fit_limits)
    : poly_rules_(kernel)
    , pw_symbols_{.piecewise = kernel.symbols_.intern_symbol("pw"),
                  .branch = kernel.symbols_.intern_symbol("case"),
                  .region = kernel.symbols_.intern_symbol("when")}
    , point_symbol_(kernel.symbols_.intern_symbol("point"))
    , less_symbol_(kernel.symbols_.intern_symbol("lt"))
    , fit_(kernel, variables, fit_limits, pw_symbols_, point_symbol_, less_symbol_)
    , assembler_(kernel.exprs_, pw_symbols_)
    , builder_(kernel.exprs_, kernel.exprs_, assembler_, pw_symbols_)
    , filter_(fit_.pw_evaluator_, pw_symbols_)
    , binder_(fit_.hypotheses_, filter_, assembler_, pw_symbols_)
    , absorber_(filter_, assembler_, pw_symbols_)
    , union_(binder_, absorber_, poly_rules_.union_)
    , rules_(union_, kernel.exprs_)
    , initial_(builder_.build(fit_.converter_.convert(samples), 0)) {}

