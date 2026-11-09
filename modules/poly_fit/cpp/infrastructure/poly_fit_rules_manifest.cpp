#include "infrastructure/poly_fit_rules_manifest.hpp"

poly_fit_rules_manifest::poly_fit_rules_manifest(
    kernel_t& kernel,
    size_t arity,
    const std::vector<poly_sample>& samples,
    const poly_fit_limits& fit_limits)
    : poly_rules_(kernel)
    , pw_symbols_{.piecewise = kernel.symbols_.intern_symbol("pw"),
                  .branch = kernel.symbols_.intern_symbol("case"),
                  .region = kernel.symbols_.intern_symbol("when")}
    , point_symbol_(kernel.symbols_.intern_symbol("point"))
    , less_symbol_(kernel.symbols_.intern_symbol("lt"))
    , fit_(kernel, name_inputs(arity), fit_limits, pw_symbols_, point_symbol_, less_symbol_)
    , abs_symbols_{.lambda = kernel.symbols_.intern_symbol("lam"),
                   .lambdas = kernel.symbols_.intern_symbol("lams"),
                   .application = kernel.symbols_.intern_symbol("app"),
                   .binding = kernel.symbols_.intern_symbol("let")}
    , indices_(kernel.symbols_, kernel.exprs_, k_abs_default_indices)
    , binders_(kernel.literals_, abs_symbols_)
    , reducer_(indices_, indices_, binders_, kernel.literals_, kernel.exprs_, abs_symbols_)
    , function_maker_(kernel.literals_, kernel.exprs_, abs_symbols_)
    , applying_(binders_, fit_.pw_evaluator_)
    , evaluator_(reducer_, applying_)
    , pw_evaluator_(evaluator_, fit_.conditions_, pw_symbols_)
    , iterations_(fit_.converter_, fit_.interpolator_, fit_.interpolator_, kernel.renderer_, indices_, kernel.exprs_,
                  abs_symbols_)
    , hypotheses_(fit_.hypotheses_, iterations_)
    , closing_(hypotheses_, function_maker_, point_symbol_)
    , assembler_(kernel.exprs_, pw_symbols_)
    , builder_(kernel.exprs_, kernel.exprs_, assembler_, pw_symbols_)
    , solver_(kernel.literals_, kernel.literals_, kernel.poly_symbols_.add, kernel.poly_symbols_.sub, kernel.poly_symbols_.mul)
    , matcher_(solver_)
    , filter_(pw_evaluator_, matcher_, pw_symbols_)
    , binder_(closing_, filter_, assembler_, pw_symbols_)
    , absorber_(filter_, assembler_, pw_symbols_)
    , union_(binder_, absorber_, poly_rules_.union_)
    , rules_(union_, kernel.exprs_)
    , initial_(builder_.build(fit_.converter_.convert(samples), 0)) {}

// The inputs' names, first to last: `#(arity-1)` down to `#0`.
std::vector<std::string> poly_fit_rules_manifest::name_inputs(size_t arity) {
    std::vector<std::string> names;
    for (size_t position = 0; position < arity; ++position)
        names.push_back("#" + std::to_string(arity - 1 - position));
    return names;
}

