#include "infrastructure/poly_fit_hypotheses_manifest.hpp"

poly_fit_hypotheses_manifest::poly_fit_hypotheses_manifest(
    kernel_t& kernel,
    const std::vector<std::string>& variables,
    const poly_fit_limits& fit_limits,
    pw_symbols symbols,
    uint32_t point_symbol,
    uint32_t less_symbol)
    : variables_(intern_variables(kernel, variables))
    , converter_(kernel.literals_, kernel.exprs_, kernel.literals_, point_symbol, variables_.size())
    , interpolator_(variables_, fit_limits.max_unknowns)
    , evaluator_(kernel.expander_, kernel.literals_, kernel.literals_, variables_)
    , conditions_(evaluator_, kernel.literals_, less_symbol)
    , pw_evaluator_(evaluator_, conditions_, symbols)
    , constants_(kernel.sizes_)
    , interpolations_(converter_, interpolator_, interpolator_, kernel.renderer_, fit_limits.max_degree,
                      fit_limits.max_starts)
    , splits_(converter_, interpolator_, interpolator_, kernel.renderer_, kernel.sizes_, kernel.literals_, kernel.exprs_,
              variables_, symbols, less_symbol, fit_limits.max_degree, fit_limits.max_starts)
    , hypotheses_(constants_, interpolations_, splits_) {}

std::vector<uint32_t> poly_fit_hypotheses_manifest::intern_variables(
    kernel_t& kernel, const std::vector<std::string>& variables) {
    std::vector<uint32_t> ids;
    ids.reserve(variables.size());
    for (const std::string& variable : variables)
        ids.push_back(kernel.symbols_.intern_symbol(variable));
    return ids;
}
