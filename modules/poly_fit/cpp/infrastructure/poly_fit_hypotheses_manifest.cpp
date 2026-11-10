#include "infrastructure/poly_fit_hypotheses_manifest.hpp"

poly_fit_hypotheses_manifest::poly_fit_hypotheses_manifest(
    kernel_t& kernel,
    const std::vector<std::string>& variables,
    const poly_fit_limits& fit_limits,
    pw_symbols symbols,
    uint32_t point_symbol,
    uint32_t less_symbol)
    : variables_(intern_variables(kernel, variables))
    , equal_symbol_(kernel.symbols_.intern_symbol("eq"))
    , modulo_symbol_(kernel.symbols_.intern_symbol("mod"))
    , converter_(kernel.literals_, kernel.exprs_, kernel.literals_, point_symbol, variables_.size())
    , interpolator_(variables_, fit_limits.max_unknowns)
    , evaluator_(kernel.expander_, kernel.literals_, kernel.literals_, kernel.exprs_, kernel.poly_symbols_, variables_)
    , conditions_(evaluator_, kernel.literals_, less_symbol, equal_symbol_, modulo_symbol_)
    , pw_evaluator_(evaluator_, conditions_, symbols)
    , constants_(kernel.sizes_)
    , interpolations_(converter_, interpolator_, interpolator_, kernel.renderer_, fit_limits.max_degree,
                      fit_limits.max_starts)
    , splits_(converter_, interpolator_, interpolator_, kernel.renderer_, kernel.sizes_, kernel.literals_, kernel.exprs_,
              variables_, symbols, less_symbol, fit_limits.max_degree, fit_limits.max_starts)
    , residues_(converter_, interpolator_, interpolator_, kernel.renderer_, kernel.literals_, kernel.exprs_,
                variables_[0], symbols, equal_symbol_, modulo_symbol_, fit_limits.max_degree)
    , hypotheses_(constants_, interpolations_, splits_, residues_) {}

std::vector<uint32_t> poly_fit_hypotheses_manifest::intern_variables(
    kernel_t& kernel, const std::vector<std::string>& variables) {
    std::vector<uint32_t> ids;
    ids.reserve(variables.size());
    for (const std::string& variable : variables)
        ids.push_back(kernel.symbols_.intern_symbol(variable));
    return ids;
}
