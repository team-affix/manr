#include "infrastructure/arith_fit_mcts_manifest.hpp"

arith_fit_mcts_manifest::arith_fit_mcts_manifest(
    const std::vector<arith_sample>& samples,
    size_t probing_depth,
    size_t max_simulations,
    double exploration_constant,
    uint32_t seed)
    : symbols_()
    , exprs_()
    , sizes_()
    , arith_symbols_{
          .input = symbols_.intern_symbol("#0"),
          .add = symbols_.intern_symbol("add"),
          .sub = symbols_.intern_symbol("sub"),
          .mul = symbols_.intern_symbol("mul")}
    , pw_symbols_{.piecewise = symbols_.intern_symbol("pw"),
                  .branch = symbols_.intern_symbol("case"),
                  .region = symbols_.intern_symbol("when")}
    , abs_symbols_{.lambda = symbols_.intern_symbol("lam"),
                   .lambdas = symbols_.intern_symbol("lams"),
                   .application = symbols_.intern_symbol("app"),
                   .binding = symbols_.intern_symbol("let")}
    , literals_(symbols_, exprs_)
    , function_maker_(literals_, exprs_, abs_symbols_)
    , converter_(literals_)
    , evaluator_(literals_, literals_, arith_symbols_)
    , binders_(literals_, abs_symbols_)
    , applying_(binders_, evaluator_)
    , assembler_(exprs_, pw_symbols_)
    , builder_(exprs_, exprs_, assembler_, pw_symbols_)
    , filter_(applying_, pw_symbols_)
    , constants_(sizes_)
    , lines_(literals_, literals_, exprs_, arith_symbols_)
    , hypotheses_(constants_, lines_)
    , closing_(hypotheses_, function_maker_, 1)
    , binder_(closing_, filter_, assembler_, pw_symbols_)
    , absorber_(filter_, assembler_, pw_symbols_)
    , simplifier_(literals_, literals_, arith_symbols_)
    , local_rules_(binder_, absorber_, simplifier_)
    , rules_(local_rules_, exprs_)
    , initial_(builder_.build(converter_.convert(samples), 0))
    , visits_()
    , values_()
    , rng_(seed)
    , uct_(visits_, visits_, values_, values_, rng_, exploration_constant, 0)
    , memo_(rules_)
    , search_(memo_, sizes_, uct_.chooser, uct_.delta, uct_.terminator, initial_, probing_depth, max_simulations)
    , printer_(symbols_) {}
