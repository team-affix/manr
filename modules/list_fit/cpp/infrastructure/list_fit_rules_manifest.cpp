#include "infrastructure/list_fit_rules_manifest.hpp"

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
    , point_symbol_(kernel.symbols_.intern_symbol("point"))
    , less_symbol_(kernel.symbols_.intern_symbol("lt"))
    , element_fit_(kernel, {"#0"}, fit_limits, pw_symbols_, point_symbol_, less_symbol_)
    , step_fit_(kernel, {"#0", "#1"}, fit_limits, pw_symbols_, point_symbol_, less_symbol_)
    , values_(kernel.exprs_, list_symbols_)
    , binders_(kernel.literals_, abs_symbols_)
    , function_maker_(kernel.literals_, kernel.exprs_, abs_symbols_)
    , evaluator_(values_, values_, kernel.exprs_, kernel.literals_, element_fit_.pw_evaluator_,
                 element_fit_.conditions_, step_fit_.pw_evaluator_, binders_, binders_, list_symbols_, point_symbol_,
                 less_symbol_)
    , applying_(binders_, evaluator_)
    , pw_evaluator_(applying_, evaluator_, pw_symbols_)
    , converter_(values_, kernel.literals_)
    , assembler_(kernel.exprs_, pw_symbols_)
    , builder_(kernel.exprs_, kernel.exprs_, assembler_, pw_symbols_)
    , solver_(kernel.literals_, kernel.literals_, kernel.poly_symbols_.add, kernel.poly_symbols_.sub, kernel.poly_symbols_.mul)
    , matcher_(solver_)
    , filter_(pw_evaluator_, matcher_, pw_symbols_)
    , constants_(kernel.sizes_)
    , integer_maps_(values_, kernel.literals_, element_fit_.converter_, element_fit_.hypotheses_, kernel.exprs_,
                    list_symbols_)
    , integer_folds_(kernel.literals_, step_fit_.converter_, step_fit_.hypotheses_, kernel.exprs_, list_symbols_)
    , filterings_(values_, kernel.literals_, kernel.literals_, kernel.exprs_, list_symbols_, less_symbol_)
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
    , map_filters_(values_, kernel.literals_, element_fit_.converter_, element_fit_.hypotheses_, kernel.literals_,
                   kernel.exprs_, list_symbols_, less_symbol_)
    , selection_folds_(values_, kernel.literals_, element_fit_.converter_, element_fit_.hypotheses_,
                       kernel.literals_, kernel.exprs_, list_symbols_, pw_symbols_, less_symbol_)
    , factor_solver_()
    , factor_folds_(values_, kernel.literals_, factor_solver_, factor_solver_, element_fit_.converter_,
                    element_fit_.hypotheses_, kernel.literals_, kernel.exprs_, list_symbols_,
                    kernel.poly_symbols_.mul)
    , sum_solver_()
    , sum_folds_(values_, kernel.literals_, sum_solver_, sum_solver_, element_fit_.converter_, element_fit_.hypotheses_,
                 kernel.literals_, kernel.exprs_, list_symbols_, pw_symbols_, kernel.poly_symbols_.add)
    , listed_folds_(values_, kernel.exprs_, list_symbols_)
    , reversals_(values_, kernel.exprs_, list_symbols_)
    , hypotheses_(constants_, integer_maps_, integer_folds_, filterings_, maps_, folds_, affine_folds_, extremum_folds_,
                  product_folds_, count_folds_, map_filters_, selection_folds_, factor_folds_, sum_folds_,
                  listed_folds_, reversals_)
    , closing_(hypotheses_, function_maker_, point_symbol_)
    , binder_(closing_, filter_, assembler_, pw_symbols_)
    , absorber_(filter_, assembler_, pw_symbols_)
    , union_(binder_, absorber_, poly_rules_.union_)
    , rules_(union_, kernel.exprs_)
    , initial_(builder_.build(converter_.convert(samples), 0)) {}
