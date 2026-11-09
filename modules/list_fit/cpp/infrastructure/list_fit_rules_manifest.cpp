#include "infrastructure/list_fit_rules_manifest.hpp"

list_fit_rules_manifest::list_fit_rules_manifest(
    kernel_t& kernel, const std::vector<list_sample>& samples, const poly_fit_limits& fit_limits)
    : poly_rules_(kernel)
    , pw_symbols_{.piecewise = kernel.symbols_.intern_symbol("pw"),
                  .branch = kernel.symbols_.intern_symbol("case"),
                  .region = kernel.symbols_.intern_symbol("when")}
    , list_symbols_{.nil = kernel.symbols_.intern_symbol("nil"),
                    .cons = kernel.symbols_.intern_symbol("cons"),
                    .map = kernel.symbols_.intern_symbol("map"),
                    .filter = kernel.symbols_.intern_symbol("filter"),
                    .fold = kernel.symbols_.intern_symbol("fold"),
                    .lambda = kernel.symbols_.intern_symbol("lam"),
                    .input = kernel.symbols_.intern_symbol("xs"),
                    .element = kernel.symbols_.intern_symbol("#0"),
                    .head = kernel.symbols_.intern_symbol("#0"),
                    .accumulator = kernel.symbols_.intern_symbol("#1")}
    , point_symbol_(kernel.symbols_.intern_symbol("point"))
    , less_symbol_(kernel.symbols_.intern_symbol("lt"))
    , element_fit_(kernel, {"#0"}, fit_limits, pw_symbols_, point_symbol_, less_symbol_)
    , step_fit_(kernel, {"#0", "#1"}, fit_limits, pw_symbols_, point_symbol_, less_symbol_)
    , values_(kernel.exprs_, list_symbols_)
    , evaluator_(values_, values_, kernel.exprs_, kernel.literals_, element_fit_.pw_evaluator_,
                 element_fit_.conditions_, step_fit_.pw_evaluator_, list_symbols_, point_symbol_, less_symbol_)
    , pw_evaluator_(evaluator_, evaluator_, pw_symbols_)
    , converter_(values_, kernel.literals_)
    , assembler_(kernel.exprs_, pw_symbols_)
    , builder_(kernel.exprs_, kernel.exprs_, assembler_, pw_symbols_)
    , filter_(pw_evaluator_, pw_symbols_)
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
    , hypotheses_(constants_, integer_maps_, integer_folds_, filterings_, maps_, folds_, affine_folds_, extremum_folds_,
                  product_folds_, count_folds_, map_filters_, selection_folds_, factor_folds_)
    , binder_(hypotheses_, filter_, assembler_, pw_symbols_)
    , absorber_(filter_, assembler_, pw_symbols_)
    , union_(binder_, absorber_, poly_rules_.union_)
    , rules_(union_, kernel.exprs_)
    , initial_(builder_.build(converter_.convert(samples), 0)) {}
