#include "infrastructure/logic_fit_rules_manifest.hpp"

logic_fit_rules_manifest::logic_fit_rules_manifest(
    kernel_t& kernel, size_t arity, const std::vector<poly_sample>& samples)
    : pw_symbols_{.piecewise = kernel.symbols_.intern_symbol("pw"),
                  .branch = kernel.symbols_.intern_symbol("case"),
                  .region = kernel.symbols_.intern_symbol("when")}
    , abs_symbols_{.lambda = kernel.symbols_.intern_symbol("lam"),
                   .lambdas = kernel.symbols_.intern_symbol("lams"),
                   .application = kernel.symbols_.intern_symbol("app"),
                   .binding = kernel.symbols_.intern_symbol("let")}
    , logic_symbols_{.conjunction = kernel.symbols_.intern_symbol("and"),
                     .disjunction = kernel.symbols_.intern_symbol("or"),
                     .exclusion = kernel.symbols_.intern_symbol("xor"),
                     .negation = kernel.symbols_.intern_symbol("not"),
                     .equivalence = kernel.symbols_.intern_symbol("xnor"),
                     .alternative_denial = kernel.symbols_.intern_symbol("nand"),
                     .joint_denial = kernel.symbols_.intern_symbol("nor"),
                     .implication = kernel.symbols_.intern_symbol("imp"),
                     .majority = kernel.symbols_.intern_symbol("maj")}
    , point_symbol_(kernel.symbols_.intern_symbol("point"))
    , variables_(intern_inputs(kernel, arity))
    , converter_(kernel.literals_, kernel.exprs_, kernel.literals_, point_symbol_, variables_.size())
    , evaluator_(kernel.literals_, kernel.literals_, logic_symbols_, variables_)
    , binders_(kernel.literals_, abs_symbols_)
    , applying_(binders_, evaluator_)
    , pw_evaluator_(applying_, evaluator_, pw_symbols_)
    , function_maker_(kernel.literals_, kernel.exprs_, abs_symbols_)
    , reader_(kernel.literals_, variables_.size())
    , constants_(kernel.sizes_)
    , parity_solver_()
    , parities_(reader_, parity_solver_, kernel.exprs_, logic_symbols_, variables_)
    , cover_solver_(k_logic_default_max_cover_width)
    , covers_(reader_, cover_solver_, kernel.exprs_, logic_symbols_, variables_)
    , primitives_(reader_, kernel.exprs_, logic_symbols_, variables_)
    , inner_(parities_, primitives_)
    , decompositions_(reader_, inner_, kernel.literals_, kernel.exprs_, logic_symbols_, variables_, point_symbol_)
    , hypotheses_(constants_, parities_, covers_, primitives_, decompositions_)
    , closing_(hypotheses_, function_maker_, point_symbol_)
    , assembler_(kernel.exprs_, pw_symbols_)
    , builder_(kernel.exprs_, kernel.exprs_, assembler_, pw_symbols_)
    , solver_(kernel.literals_, kernel.literals_, kernel.poly_symbols_.add, kernel.poly_symbols_.sub, kernel.poly_symbols_.mul)
    , matcher_(solver_)
    , filter_(pw_evaluator_, matcher_, pw_symbols_)
    , binder_(closing_, filter_, assembler_, pw_symbols_)
    , absorber_(filter_, assembler_, pw_symbols_)
    , simplifier_(kernel.literals_, kernel.literals_, kernel.exprs_, logic_symbols_)
    , refits_(parities_, covers_, primitives_)
    , refitter_(kernel.sizes_, evaluator_, refits_, kernel.literals_, kernel.literals_, kernel.exprs_,
                logic_symbols_, variables_, point_symbol_)
    , union_(binder_, absorber_, simplifier_, refitter_)
    , rules_(union_, kernel.exprs_)
    , initial_(builder_.build(converter_.convert(samples), 0)) {}

// The inputs' index symbols, first to last: `#(arity-1)` down to `#0`.
std::vector<uint32_t> logic_fit_rules_manifest::intern_inputs(kernel_t& kernel, size_t arity) {
    std::vector<uint32_t> ids;
    ids.reserve(arity);
    for (size_t position = 0; position < arity; ++position)
        ids.push_back(kernel.symbols_.intern_symbol("#" + std::to_string(arity - 1 - position)));
    return ids;
}
