#include "composition/poly_fit_rules_manifest.hpp"

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
    , num_symbols_{.prime = kernel.symbols_.intern_symbol("prime"),
                   .is_prime = kernel.symbols_.intern_symbol("is_prime")}
    , logic_symbols_{.conjunction = kernel.symbols_.intern_symbol("and"),
                     .disjunction = kernel.symbols_.intern_symbol("or"),
                     .exclusion = kernel.symbols_.intern_symbol("xor"),
                     .negation = kernel.symbols_.intern_symbol("not"),
                     .equivalence = kernel.symbols_.intern_symbol("xnor"),
                     .alternative_denial = kernel.symbols_.intern_symbol("nand"),
                     .joint_denial = kernel.symbols_.intern_symbol("nor"),
                     .implication = kernel.symbols_.intern_symbol("imp"),
                     .majority = kernel.symbols_.intern_symbol("maj"),
                     .multiplexer = kernel.symbols_.intern_symbol("mux")}
    , indices_(kernel.symbols_, kernel.exprs_, k_abs_default_indices)
    , binders_(kernel.literals_, abs_symbols_)
    , reducer_(indices_, indices_, binders_, kernel.literals_, kernel.exprs_, abs_symbols_)
    , function_maker_(kernel.literals_, kernel.exprs_, abs_symbols_)
    , prime_table_(k_num_default_max_value)
    , numbers_(fit_.pw_evaluator_, fit_.conditions_, kernel.literals_, kernel.literals_, prime_table_, prime_table_,
               num_symbols_)
    , conditions_(numbers_, logic_symbols_)
    , body_(numbers_, conditions_, pw_symbols_)
    , applying_(binders_, body_)
    , evaluator_(reducer_, applying_)
    , pw_evaluator_(evaluator_, fit_.conditions_, pw_symbols_)
    , iterations_(fit_.converter_, fit_.interpolator_, fit_.interpolator_, kernel.renderer_, indices_, kernel.exprs_,
                  abs_symbols_)
    , primes_(kernel.literals_, prime_table_, prime_table_, kernel.exprs_, num_symbols_,
              kernel.symbols_.intern_symbol("#0"))
    , atoms_(kernel.literals_, kernel.literals_, kernel.exprs_, kernel.symbols_.intern_symbol("#0"), less_symbol_,
             fit_.equal_symbol_, fit_.modulo_symbol_, num_symbols_.is_prime)
    , condition_logic_(kernel, logic_symbols_, intern_placeholders(kernel), point_symbol_)
    , combinations_(atoms_, conditions_, condition_logic_.hypotheses_, kernel.sizes_, kernel.literals_,
                    kernel.literals_, kernel.exprs_, logic_symbols_, condition_logic_.variables_, point_symbol_)
    , conditions_union_(primes_, combinations_)
    , guarded_(conditions_union_, fit_.hypotheses_, body_, kernel.sizes_, kernel.literals_, kernel.exprs_,
               pw_symbols_)
    , hypotheses_(guarded_, iterations_, primes_)
    , piecewise_(hypotheses_, pw_evaluator_, function_maker_, kernel.literals_, kernel.exprs_, pw_symbols_,
                 point_symbol_, kernel.poly_symbols_.add, kernel.poly_symbols_.sub, kernel.poly_symbols_.mul,
                 kernel.poly_symbols_.pow)
    , local_rules_(piecewise_.binder_, piecewise_.absorber_, poly_rules_.local_rules_)
    , root_rules_()
    , subterms_(local_rules_, kernel.exprs_)
    , rules_(subterms_, root_rules_)
    , initial_(piecewise_.builder_.build(fit_.converter_.convert(samples), 0)) {}

// The inputs' names, first to last: `#(arity-1)` down to `#0`.
std::vector<std::string> poly_fit_rules_manifest::name_inputs(size_t arity) {
    std::vector<std::string> names;
    for (size_t position = 0; position < arity; ++position)
        names.push_back("#" + std::to_string(arity - 1 - position));
    return names;
}

// The placeholder inputs of a region condition's truth table over its atoms.
std::vector<uint32_t> poly_fit_rules_manifest::intern_placeholders(kernel_t& kernel) {
    std::vector<uint32_t> ids;
    for (size_t index = 0; index < k_poly_condition_atoms; ++index)
        ids.push_back(kernel.symbols_.intern_symbol("%atom" + std::to_string(index)));
    return ids;
}
