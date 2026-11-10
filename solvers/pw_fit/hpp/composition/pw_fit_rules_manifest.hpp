#ifndef PW_FIT_RULES_MANIFEST_HPP
#define PW_FIT_RULES_MANIFEST_HPP

#include <cstdint>
#include "hypotheses/abs_closing_hypotheses.hpp"
#include "infrastructure/arith_hole_solver.hpp"
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/pw_assembler.hpp"
#include "infrastructure/pw_branch_filter.hpp"
#include "infrastructure/pw_builder.hpp"
#include "infrastructure/pw_hole_matcher.hpp"
#include "rules/pw_branch_absorber.hpp"
#include "rules/pw_extension_binder.hpp"
#include "value_objects/pw_symbols.hpp"

// The piecewise rules every fitting solver shares, over one domain's hypotheses and evaluator. `binder_` binds a data
// piecewise's catch-all to a model the hypotheses propose, closed over the piecewise's own inputs (`closing_`), and
// drops the branches it reproduces; `absorber_` drops the branches a model already in place reproduces. A branch is
// reproduced when the model's value at its input matches its label under one binding of the label's holes, solved
// over the domain's `add`, `sub`, `mul` and `pow`. `builder_` turns data points into the piecewise a search starts
// from, and `matcher_` is how a test or a benchmark checks that a result reproduces a sample.
template<typename IProposeHypotheses, typename IEvaluateAt, typename IMakeFunction, typename ILiterals>
struct pw_fit_rules_manifest {
    struct closing_t : abs_closing_hypotheses<IProposeHypotheses, IMakeFunction> {
        using abs_closing_hypotheses<IProposeHypotheses, IMakeFunction>::abs_closing_hypotheses;
    };
    struct assembler_t : pw_assembler<expr_pool> {
        using pw_assembler<expr_pool>::pw_assembler;
    };
    struct builder_t : pw_builder<expr_pool, expr_pool, assembler_t> {
        using pw_builder<expr_pool, expr_pool, assembler_t>::pw_builder;
    };
    struct solver_t : arith_hole_solver<ILiterals, ILiterals> {
        using arith_hole_solver<ILiterals, ILiterals>::arith_hole_solver;
    };
    struct matcher_t : pw_hole_matcher<solver_t> {
        using pw_hole_matcher<solver_t>::pw_hole_matcher;
    };
    struct filter_t : pw_branch_filter<IEvaluateAt, matcher_t> {
        using pw_branch_filter<IEvaluateAt, matcher_t>::pw_branch_filter;
    };
    struct binder_t : pw_extension_binder<closing_t, filter_t, assembler_t> {
        using pw_extension_binder<closing_t, filter_t, assembler_t>::pw_extension_binder;
    };
    struct absorber_t : pw_branch_absorber<filter_t, assembler_t> {
        using pw_branch_absorber<filter_t, assembler_t>::pw_branch_absorber;
    };

    pw_fit_rules_manifest(IProposeHypotheses& propose_hypotheses,
                          IEvaluateAt& evaluate_at,
                          IMakeFunction& make_function,
                          ILiterals& literals,
                          expr_pool& exprs,
                          pw_symbols symbols,
                          uint32_t point_symbol,
                          uint32_t add_symbol,
                          uint32_t sub_symbol,
                          uint32_t mul_symbol,
                          uint32_t pow_symbol);

    closing_t closing_;
    assembler_t assembler_;
    builder_t builder_;
    solver_t solver_;
    matcher_t matcher_;
    filter_t filter_;
    binder_t binder_;
    absorber_t absorber_;
};

#define PW_FIT_RULES_MANIFEST_TEMPLATE \
    template<typename IProposeHypotheses, typename IEvaluateAt, typename IMakeFunction, typename ILiterals>
#define PW_FIT_RULES_MANIFEST pw_fit_rules_manifest<IProposeHypotheses, IEvaluateAt, IMakeFunction, ILiterals>

PW_FIT_RULES_MANIFEST_TEMPLATE PW_FIT_RULES_MANIFEST::pw_fit_rules_manifest(IProposeHypotheses& propose_hypotheses,
                                                                            IEvaluateAt& evaluate_at,
                                                                            IMakeFunction& make_function,
                                                                            ILiterals& literals,
                                                                            expr_pool& exprs,
                                                                            pw_symbols symbols,
                                                                            uint32_t point_symbol,
                                                                            uint32_t add_symbol,
                                                                            uint32_t sub_symbol,
                                                                            uint32_t mul_symbol,
                                                                            uint32_t pow_symbol)
    : closing_(propose_hypotheses, make_function, point_symbol)
    , assembler_(exprs, symbols)
    , builder_(exprs, exprs, assembler_, symbols)
    , solver_(literals, literals, add_symbol, sub_symbol, mul_symbol, pow_symbol)
    , matcher_(solver_)
    , filter_(evaluate_at, matcher_, symbols)
    , binder_(closing_, filter_, assembler_, symbols)
    , absorber_(filter_, assembler_, symbols) {}

#undef PW_FIT_RULES_MANIFEST
#undef PW_FIT_RULES_MANIFEST_TEMPLATE

#endif
