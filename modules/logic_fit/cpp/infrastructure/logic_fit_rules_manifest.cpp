#include "infrastructure/logic_fit_rules_manifest.hpp"

logic_fit_rules_manifest::logic_fit_rules_manifest(
    kernel_t& kernel, const std::vector<std::string>& variables, const std::vector<poly_sample>& samples)
    : pw_symbols_{.piecewise = kernel.symbols_.intern_symbol("pw"),
                  .branch = kernel.symbols_.intern_symbol("case"),
                  .region = kernel.symbols_.intern_symbol("when")}
    , logic_symbols_{.conjunction = kernel.symbols_.intern_symbol("and"),
                     .disjunction = kernel.symbols_.intern_symbol("or"),
                     .exclusion = kernel.symbols_.intern_symbol("xor"),
                     .negation = kernel.symbols_.intern_symbol("not")}
    , point_symbol_(kernel.symbols_.intern_symbol("point"))
    , variables_(intern_variables(kernel, variables))
    , converter_(kernel.literals_, kernel.exprs_, kernel.literals_, point_symbol_, variables_.size())
    , evaluator_(kernel.literals_, kernel.literals_, logic_symbols_, variables_)
    , pw_evaluator_(evaluator_, evaluator_, pw_symbols_)
    , reader_(kernel.literals_, variables_.size())
    , constants_(kernel.sizes_)
    , parity_solver_()
    , parities_(reader_, parity_solver_, kernel.exprs_, logic_symbols_, variables_)
    , cover_solver_(k_logic_default_max_cover_width)
    , covers_(reader_, cover_solver_, kernel.exprs_, logic_symbols_, variables_)
    , hypotheses_(constants_, parities_, covers_)
    , assembler_(kernel.exprs_, pw_symbols_)
    , builder_(kernel.exprs_, kernel.exprs_, assembler_, pw_symbols_)
    , filter_(pw_evaluator_, pw_symbols_)
    , binder_(hypotheses_, filter_, assembler_, pw_symbols_)
    , absorber_(filter_, assembler_, pw_symbols_)
    , union_(binder_, absorber_)
    , rules_(union_, kernel.exprs_)
    , initial_(builder_.build(converter_.convert(samples), 0)) {}

std::vector<uint32_t> logic_fit_rules_manifest::intern_variables(
    kernel_t& kernel, const std::vector<std::string>& variables) {
    std::vector<uint32_t> ids;
    ids.reserve(variables.size());
    for (const std::string& variable : variables)
        ids.push_back(kernel.symbols_.intern_symbol(variable));
    return ids;
}
