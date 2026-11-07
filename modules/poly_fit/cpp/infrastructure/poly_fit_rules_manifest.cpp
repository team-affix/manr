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
    , variables_(intern_variables(kernel, variables))
    , converter_(kernel.literals_, kernel.exprs_, kernel.literals_, point_symbol_, variables_.size())
    , interpolator_(variables_, fit_limits.max_unknowns)
    , evaluator_(kernel.expander_, kernel.literals_, kernel.literals_, variables_)
    , conditions_(evaluator_, kernel.literals_, less_symbol_)
    , pw_evaluator_(evaluator_, conditions_, pw_symbols_)
    , assembler_(kernel.exprs_, pw_symbols_)
    , builder_(kernel.exprs_, kernel.exprs_, assembler_, pw_symbols_)
    , filter_(pw_evaluator_, pw_symbols_)
    , constants_(kernel.sizes_)
    , interpolations_(converter_, interpolator_, interpolator_, kernel.renderer_, fit_limits.max_degree,
                      fit_limits.max_starts)
    , splits_(converter_, interpolator_, interpolator_, kernel.renderer_, kernel.literals_, kernel.exprs_, variables_,
              pw_symbols_, less_symbol_, fit_limits.max_degree, fit_limits.max_starts)
    , hypotheses_(constants_, interpolations_, splits_)
    , binder_(hypotheses_, filter_, assembler_, pw_symbols_)
    , absorber_(filter_, assembler_, pw_symbols_)
    , union_(binder_, absorber_, poly_rules_.union_)
    , rules_(union_, kernel.exprs_)
    , initial_(builder_.build(converter_.convert(samples), 0)) {}

std::vector<uint32_t> poly_fit_rules_manifest::intern_variables(
    kernel_t& kernel, const std::vector<std::string>& variables) {
    std::vector<uint32_t> ids;
    ids.reserve(variables.size());
    for (const std::string& variable : variables)
        ids.push_back(kernel.symbols_.intern_symbol(variable));
    return ids;
}
