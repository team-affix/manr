#include "composition/arith_fit_rules_manifest.hpp"

arith_fit_rules_manifest::arith_fit_rules_manifest(kernel_t& kernel, const std::vector<arith_sample>& samples)
    : pw_symbols_{.piecewise = kernel.symbols_.intern_symbol("pw"),
                  .branch = kernel.symbols_.intern_symbol("case"),
                  .region = kernel.symbols_.intern_symbol("when")}
    , abs_symbols_{.lambda = kernel.symbols_.intern_symbol("lam"),
                   .lambdas = kernel.symbols_.intern_symbol("lams"),
                   .application = kernel.symbols_.intern_symbol("app"),
                   .binding = kernel.symbols_.intern_symbol("let")}
    , pow_symbol_(kernel.symbols_.intern_symbol("pow"))
    , point_symbol_(kernel.symbols_.intern_symbol("point"))
    , function_maker_(kernel.literals_, kernel.exprs_, abs_symbols_)
    , converter_(kernel.literals_)
    , evaluator_(kernel.literals_, kernel.literals_, kernel.arith_symbols_)
    , binders_(kernel.literals_, abs_symbols_)
    , applying_(binders_, evaluator_)
    , constants_(kernel.sizes_)
    , lines_(kernel.literals_, kernel.literals_, kernel.exprs_, kernel.arith_symbols_)
    , hypotheses_(constants_, lines_)
    , piecewise_(hypotheses_, applying_, function_maker_, kernel.literals_, kernel.exprs_, pw_symbols_, point_symbol_,
                 kernel.arith_symbols_.add, kernel.arith_symbols_.sub, kernel.arith_symbols_.mul, pow_symbol_)
    , simplifier_(kernel.literals_, kernel.literals_, kernel.arith_symbols_)
    , local_rules_(piecewise_.binder_, piecewise_.absorber_, simplifier_)
    , root_rules_()
    , subterms_(local_rules_, kernel.exprs_)
    , rules_(subterms_, root_rules_)
    , initial_(piecewise_.builder_.build(converter_.convert(samples), 0)) {}
