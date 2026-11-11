#include "composition/list_fit_rules_manifest.hpp"

list_fit_rules_manifest::list_fit_rules_manifest(
    kernel_t& kernel, const std::vector<list_sample>& samples, const poly_fit_limits& fit_limits)
    : poly_rules_(kernel)
    , pw_symbols_{.piecewise = kernel.symbols_.intern_symbol("pw"),
                  .branch = kernel.symbols_.intern_symbol("case"),
                  .region = kernel.symbols_.intern_symbol("when")}
    , abs_symbols_{.lambda = kernel.symbols_.intern_symbol("lam"),
                   .lambdas = kernel.symbols_.intern_symbol("lams"),
                   .application = kernel.symbols_.intern_symbol("app"),
                   .binding = kernel.symbols_.intern_symbol("let")}
    , list_symbols_{.nil = kernel.symbols_.intern_symbol("nil"),
                    .cons = kernel.symbols_.intern_symbol("cons"),
                    .append = kernel.symbols_.intern_symbol("append"),
                    .reverse = kernel.symbols_.intern_symbol("reverse"),
                    .map = kernel.symbols_.intern_symbol("map"),
                    .filter = kernel.symbols_.intern_symbol("filter"),
                    .fold = kernel.symbols_.intern_symbol("fold"),
                    .lambda = kernel.symbols_.intern_symbol("lam"),
                    .input = kernel.symbols_.intern_symbol("#0"),
                    .element = kernel.symbols_.intern_symbol("#0"),
                    .head = kernel.symbols_.intern_symbol("#0"),
                    .accumulator = kernel.symbols_.intern_symbol("#1")}
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
    , point_symbol_(kernel.symbols_.intern_symbol("point"))
    , less_symbol_(kernel.symbols_.intern_symbol("lt"))
    , element_fit_(kernel, {"#0"}, fit_limits, pw_symbols_, point_symbol_, less_symbol_)
    , step_fit_(kernel, {"#0", "#1"}, fit_limits, pw_symbols_, point_symbol_, less_symbol_)
    , values_(kernel.exprs_, list_symbols_)
    , binders_(kernel.literals_, abs_symbols_)
    , function_maker_(kernel.literals_, kernel.exprs_, abs_symbols_)
    , prime_table_(k_num_default_max_value)
    , numbers_(element_fit_.pw_evaluator_, element_fit_.conditions_, kernel.literals_, kernel.literals_, prime_table_,
               prime_table_, num_symbols_)
    , conditions_(numbers_, logic_symbols_)
    , element_values_(numbers_, conditions_, pw_symbols_)
    , step_numbers_(step_fit_.pw_evaluator_, step_fit_.conditions_, kernel.literals_, kernel.literals_, prime_table_,
                    prime_table_, num_symbols_)
    , step_conditions_(step_numbers_, logic_symbols_)
    , step_values_(step_numbers_, step_conditions_, pw_symbols_)
    , evaluator_(values_, values_, kernel.exprs_, kernel.literals_, element_values_, conditions_, step_values_,
                 binders_, binders_, list_symbols_, point_symbol_, less_symbol_)
    , applying_(binders_, evaluator_)
    , pw_evaluator_(applying_, evaluator_, pw_symbols_)
    , converter_(values_, kernel.literals_)
    , constants_(kernel.sizes_)
    , primes_(kernel.literals_, prime_table_, prime_table_, kernel.exprs_, num_symbols_, list_symbols_.element)
    , atoms_(kernel.literals_, kernel.literals_, kernel.exprs_, list_symbols_.element, less_symbol_,
             element_fit_.equal_symbol_, element_fit_.modulo_symbol_, num_symbols_.is_prime)
    , condition_logic_(kernel, logic_symbols_, intern_placeholders(kernel), point_symbol_)
    , combinations_(atoms_, conditions_, condition_logic_.hypotheses_, kernel.sizes_, kernel.literals_,
                    kernel.literals_, kernel.exprs_, logic_symbols_, condition_logic_.variables_, point_symbol_)
    , conditions_union_(primes_, combinations_)
    , guarded_(conditions_union_, atoms_, element_fit_.hypotheses_, element_fit_.interpolations_, element_values_,
               kernel.sizes_, kernel.literals_, kernel.exprs_, pw_symbols_)
    , integer_maps_(values_, kernel.literals_, element_fit_.converter_, guarded_, kernel.exprs_,
                    list_symbols_)
    , integer_folds_(kernel.literals_, step_fit_.converter_, step_fit_.hypotheses_, kernel.exprs_, list_symbols_)
    , filterings_(values_, kernel.literals_, kernel.literals_, kernel.exprs_, conditions_union_, list_symbols_,
                  less_symbol_, point_symbol_)
    , maps_(values_, element_fit_.hypotheses_, kernel.exprs_, list_symbols_, point_symbol_)
    , folds_(step_fit_.hypotheses_, kernel.exprs_, list_symbols_, point_symbol_)
    , affine_solver_()
    , affine_folds_(values_, kernel.literals_, affine_solver_, affine_solver_, kernel.renderer_, kernel.literals_,
                    kernel.exprs_, list_symbols_, fit_limits.max_degree, k_list_default_max_multiplier,
                    k_list_default_max_fold_starts)
    , extremum_folds_(values_, kernel.literals_, kernel.literals_, kernel.exprs_, list_symbols_, pw_symbols_,
                      less_symbol_)
    , product_folds_(values_, kernel.literals_, kernel.renderer_, kernel.literals_, kernel.exprs_, list_symbols_,
                     k_list_default_max_factor_slope, k_list_default_max_factor_offset)
    , count_folds_(values_, kernel.literals_, kernel.renderer_, kernel.literals_, kernel.exprs_, list_symbols_,
                   pw_symbols_, less_symbol_)
    , map_filters_(values_, kernel.literals_, element_fit_.converter_, element_fit_.hypotheses_, conditions_union_,
                   kernel.literals_, kernel.exprs_, list_symbols_, less_symbol_, point_symbol_)
    , selection_folds_(values_, kernel.literals_, element_fit_.converter_, element_fit_.hypotheses_,
                       kernel.literals_, kernel.exprs_, list_symbols_, pw_symbols_, less_symbol_)
    , factor_solver_()
    , factor_folds_(values_, kernel.literals_, factor_solver_, factor_solver_, element_fit_.converter_,
                    element_fit_.hypotheses_, kernel.literals_, kernel.exprs_, list_symbols_,
                    kernel.poly_symbols_.mul)
    , sum_solver_()
    , sum_folds_(values_, kernel.literals_, sum_solver_, sum_solver_, element_fit_.converter_, guarded_,
                 kernel.literals_, kernel.exprs_, list_symbols_, pw_symbols_, kernel.poly_symbols_.add)
    , listed_folds_(values_, kernel.exprs_, list_symbols_)
    , reversals_(values_, kernel.exprs_, list_symbols_)
    , hypotheses_(constants_, integer_maps_, integer_folds_, filterings_, maps_, folds_, affine_folds_, extremum_folds_,
                  product_folds_, count_folds_, map_filters_, selection_folds_, factor_folds_, sum_folds_,
                  listed_folds_, reversals_)
    , piecewise_(hypotheses_, pw_evaluator_, function_maker_, kernel.literals_, kernel.exprs_, pw_symbols_,
                 point_symbol_, kernel.poly_symbols_.add, kernel.poly_symbols_.sub, kernel.poly_symbols_.mul,
                 kernel.poly_symbols_.pow)
    , indices_(kernel.symbols_, kernel.exprs_, k_abs_default_indices)
    , reducer_(indices_, indices_, binders_, kernel.literals_, kernel.exprs_, abs_symbols_)
    , map_fuser_(reducer_, kernel.sizes_, kernel.exprs_, list_symbols_)
    , identity_maps_(list_symbols_)
    , filter_fuser_(kernel.exprs_, list_symbols_, logic_symbols_.conjunction)
    , reverse_canceller_(kernel.exprs_, list_symbols_)
    , holes_(poly_rules_.holes_, kernel.exprs_)
    , local_rules_(piecewise_.binder_, piecewise_.absorber_, poly_rules_.local_rules_, map_fuser_, identity_maps_,
                   filter_fuser_, reverse_canceller_)
    , root_rules_(holes_)
    , subterms_(local_rules_, kernel.exprs_)
    , rules_(subterms_, root_rules_)
    , initial_(piecewise_.builder_.build(converter_.convert(samples), 0)) {}

// The placeholder inputs of a condition's truth table over its atoms, one per atom a condition may combine.
std::vector<uint32_t> list_fit_rules_manifest::intern_placeholders(kernel_t& kernel) {
    std::vector<uint32_t> ids;
    for (size_t index = 0; index < k_list_condition_atoms; ++index)
        ids.push_back(kernel.symbols_.intern_symbol("%atom" + std::to_string(index)));
    return ids;
}
