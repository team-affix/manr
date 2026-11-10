#include "infrastructure/seq_fit_rules_manifest.hpp"

seq_fit_rules_manifest::seq_fit_rules_manifest(
    kernel_t& kernel,
    const std::vector<poly_sample>& samples,
    const poly_fit_limits& fit_limits)
    : poly_rules_(kernel)
    , pw_symbols_{.piecewise = kernel.symbols_.intern_symbol("pw"),
                  .branch = kernel.symbols_.intern_symbol("case"),
                  .region = kernel.symbols_.intern_symbol("when")}
    , abs_symbols_{.lambda = kernel.symbols_.intern_symbol("lam"),
                   .lambdas = kernel.symbols_.intern_symbol("lams"),
                   .application = kernel.symbols_.intern_symbol("app"),
                   .binding = kernel.symbols_.intern_symbol("let")}
    , seq_symbols_{.recurrence = kernel.symbols_.intern_symbol("rec"),
                   .power = kernel.symbols_.intern_symbol("exp"),
                   .tabulation = kernel.symbols_.intern_symbol("tabulate"),
                   .unfolding = kernel.symbols_.intern_symbol("unfold"),
                   .lambda = kernel.symbols_.intern_symbol("lam"),
                   .index = kernel.symbols_.intern_symbol("#0"),
                   .previous = {kernel.symbols_.intern_symbol("#1"), kernel.symbols_.intern_symbol("#2"),
                                kernel.symbols_.intern_symbol("#3")}}
    , num_symbols_{.prime = kernel.symbols_.intern_symbol("prime"),
                   .is_prime = kernel.symbols_.intern_symbol("is_prime")}
    , point_symbol_(kernel.symbols_.intern_symbol("point"))
    , less_symbol_(kernel.symbols_.intern_symbol("lt"))
    , fit_(kernel, {"#0"}, fit_limits, pw_symbols_, point_symbol_, less_symbol_)
    , first_steps_(kernel, {"#0", "#1"}, fit_limits, pw_symbols_, point_symbol_, less_symbol_)
    , second_steps_(kernel, {"#0", "#1", "#2"}, fit_limits, pw_symbols_, point_symbol_, less_symbol_)
    , third_steps_(kernel, {"#0", "#1", "#2", "#3"}, fit_limits, pw_symbols_, point_symbol_, less_symbol_)
    , prime_table_(k_num_default_max_value)
    , binders_(kernel.literals_, abs_symbols_)
    , function_maker_(kernel.literals_, kernel.exprs_, abs_symbols_)
    , evaluator_(fit_.evaluator_, third_steps_.evaluator_, binders_, kernel.literals_, kernel.literals_, kernel.exprs_,
                 seq_symbols_, kernel.poly_symbols_, point_symbol_, k_seq_default_max_index)
    , conditions_(evaluator_, kernel.literals_, less_symbol_, fit_.equal_symbol_, fit_.modulo_symbol_)
    , body_evaluator_(evaluator_, conditions_, pw_symbols_)
    , numbers_(body_evaluator_, conditions_, kernel.literals_, kernel.literals_, prime_table_, prime_table_,
               num_symbols_)
    , applying_(binders_, numbers_)
    , pw_evaluator_(applying_, conditions_, pw_symbols_)
    , first_order_(kernel.literals_, first_steps_.converter_, first_steps_.interpolations_, kernel.literals_,
                   kernel.exprs_, first_steps_.evaluator_, seq_symbols_, fit_.variables_[0], point_symbol_, 1)
    , second_order_(kernel.literals_, second_steps_.converter_, second_steps_.interpolations_, kernel.literals_,
                    kernel.exprs_, second_steps_.evaluator_, seq_symbols_, fit_.variables_[0], point_symbol_, 2)
    , third_order_(kernel.literals_, third_steps_.converter_, third_steps_.interpolations_, kernel.literals_,
                   kernel.exprs_, third_steps_.evaluator_, seq_symbols_, fit_.variables_[0], point_symbol_, 3)
    , primes_(kernel.literals_, prime_table_, prime_table_, kernel.exprs_, num_symbols_, seq_symbols_.index)
    , hypotheses_(fit_.hypotheses_, first_order_, second_order_, third_order_, primes_)
    , closing_(hypotheses_, function_maker_, point_symbol_)
    , assembler_(kernel.exprs_, pw_symbols_)
    , builder_(kernel.exprs_, kernel.exprs_, assembler_, pw_symbols_)
    , solver_(kernel.literals_, kernel.literals_, kernel.poly_symbols_.add, kernel.poly_symbols_.sub,
              kernel.poly_symbols_.mul, kernel.poly_symbols_.pow)
    , matcher_(solver_)
    , filter_(pw_evaluator_, matcher_, pw_symbols_)
    , binder_(closing_, filter_, assembler_, pw_symbols_)
    , absorber_(filter_, assembler_, pw_symbols_)
    , closer_(binders_, kernel.expander_, kernel.literals_, kernel.literals_, kernel.exprs_, seq_symbols_,
              kernel.poly_symbols_)
    , unfolder_(binders_, kernel.exprs_, seq_symbols_)
    , merger_(binders_, kernel.literals_, kernel.exprs_, abs_symbols_)
    , union_(binder_, absorber_, closer_, unfolder_, merger_, poly_rules_.union_)
    , subterms_(union_, kernel.exprs_)
    , open_lists_(kernel.literals_, kernel.literals_, kernel.exprs_, kernel.symbols_.intern_symbol("cons"),
                  point_symbol_, seq_symbols_.tabulation, pw_symbols_)
    , rules_(subterms_, open_lists_)
    , initial_(builder_.build(fit_.converter_.convert(samples), 0)) {}
