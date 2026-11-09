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
    , abs_symbols_{.lambda = kernel.symbols_.intern_symbol("lam"),
                   .application = kernel.symbols_.intern_symbol("app"),
                   .binding = kernel.symbols_.intern_symbol("let")}
    , indices_(kernel.symbols_, kernel.exprs_, k_abs_default_indices)
    , reducer_(indices_, indices_, kernel.exprs_, abs_symbols_)
    , evaluator_(reducer_, fit_.evaluator_)
    , pw_evaluator_(evaluator_, fit_.conditions_, pw_symbols_)
    , iterations_(fit_.converter_, fit_.interpolator_, fit_.interpolator_, kernel.renderer_, indices_, kernel.exprs_,
                  fit_.variables_[0], abs_symbols_)
    , hypotheses_(fit_.hypotheses_, iterations_)
    , assembler_(kernel.exprs_, pw_symbols_)
    , builder_(kernel.exprs_, kernel.exprs_, assembler_, pw_symbols_)
    , filter_(pw_evaluator_, pw_symbols_)
    , binder_(hypotheses_, filter_, assembler_, pw_symbols_)
    , absorber_(filter_, assembler_, pw_symbols_)
    , union_(binder_, absorber_, poly_rules_.union_)
    , rules_(union_, kernel.exprs_)
    , initial_(builder_.build(fit_.converter_.convert(samples), 0)) {}

