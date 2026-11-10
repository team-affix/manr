#ifndef POLY_FIT_RULES_MANIFEST_HPP
#define POLY_FIT_RULES_MANIFEST_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "composition/logic_fit_hypotheses_manifest.hpp"
#include "composition/poly_fit_hypotheses_manifest.hpp"
#include "composition/poly_kernel_manifest.hpp"
#include "composition/poly_rules_manifest.hpp"
#include "composition/pw_fit_rules_manifest.hpp"
#include "hypotheses/logic_condition_hypotheses.hpp"
#include "hypotheses/num_prime_hypotheses.hpp"
#include "hypotheses/poly_iteration_hypotheses.hpp"
#include "hypotheses/pw_guarded_hypotheses.hpp"
#include "infrastructure/abs_applying_evaluator.hpp"
#include "infrastructure/abs_binders.hpp"
#include "infrastructure/abs_evaluator.hpp"
#include "infrastructure/abs_function_maker.hpp"
#include "infrastructure/abs_indices.hpp"
#include "infrastructure/abs_reducer.hpp"
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/logic_condition_evaluator.hpp"
#include "infrastructure/num_condition_atoms.hpp"
#include "infrastructure/num_evaluator.hpp"
#include "infrastructure/num_prime_table.hpp"
#include "infrastructure/poly_interpolator.hpp"
#include "infrastructure/proposer_union.hpp"
#include "infrastructure/pw_evaluator.hpp"
#include "infrastructure/pw_hypotheses_union.hpp"
#include "infrastructure/subterm_proposer.hpp"
#include "value_objects/abs_symbols.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/logic_symbols.hpp"
#include "value_objects/num_symbols.hpp"
#include "value_objects/poly_fit_limits.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_symbols.hpp"

// The rules for fitting polynomial data, over a kernel: the samples become a piecewise with an open catch-all; the
// piecewise rules bind it to what `fit_` proposes (constants, interpolated polynomials, and piecewise polynomials split
// over intervals of one variable, periodic over residues, and over one variable a polynomial iterated with a helper,
// `let(lam(g(#0)), app(#0, app(#0, #1)))`) and drop the branches they reproduce; and the full polynomial rule set
// compresses whatever polynomial results, all at every position. Expressions are evaluated through the abstraction
// reducer, so helpers reduce to polynomials before the polynomial evaluator sees them. The detectors read keys of
// `arity` coordinates and fit in `#(arity-1)` ... `#0`. A piecewise is a function of its keys: what replaces its
// catch-all is closed over that piecewise's own inputs (`lam(b)` or `lams(k, b)`, k read from its keys), and once no
// case is left the piecewise is that function. Evaluating a function at an input applies it. `rules_` is what a search
// injects and `initial_` is where it starts.
// The most atoms a region condition combines (the placeholder inputs of its truth table).
constexpr size_t k_poly_condition_atoms = 6;

struct poly_fit_rules_manifest {
    using kernel_t = poly_kernel_manifest;
    // ---- support: what the hypotheses and rules use ----
    using fit_t = poly_fit_hypotheses_manifest;
    struct indices_t : abs_indices<symbol_table, expr_pool> {
        using abs_indices<symbol_table, expr_pool>::abs_indices;
    };
    struct binders_t : abs_binders<kernel_t::literals_t> {
        using abs_binders<kernel_t::literals_t>::abs_binders;
    };
    struct reducer_t : abs_reducer<indices_t, indices_t, binders_t, kernel_t::literals_t, expr_pool> {
        using abs_reducer<indices_t, indices_t, binders_t, kernel_t::literals_t, expr_pool>::abs_reducer;
    };
    struct function_maker_t : abs_function_maker<kernel_t::literals_t, expr_pool> {
        using abs_function_maker<kernel_t::literals_t, expr_pool>::abs_function_maker;
    };
    using numbers_base_t = num_evaluator<fit_t::pw_evaluator_t, fit_t::conditions_t, kernel_t::literals_t,
                                         kernel_t::literals_t, num_prime_table, num_prime_table>;
    struct numbers_t : numbers_base_t {
        using numbers_base_t::numbers_base_t;
    };
    struct conditions_t : logic_condition_evaluator<numbers_t> {
        using logic_condition_evaluator<numbers_t>::logic_condition_evaluator;
    };
    struct body_t : pw_evaluator<numbers_t, conditions_t> {
        using pw_evaluator<numbers_t, conditions_t>::pw_evaluator;
    };
    struct applying_t : abs_applying_evaluator<binders_t, body_t> {
        using abs_applying_evaluator<binders_t, body_t>::abs_applying_evaluator;
    };
    struct evaluator_t : abs_evaluator<reducer_t, applying_t> {
        using abs_evaluator<reducer_t, applying_t>::abs_evaluator;
    };
    struct pw_evaluator_t : pw_evaluator<evaluator_t, fit_t::conditions_t> {
        using pw_evaluator<evaluator_t, fit_t::conditions_t>::pw_evaluator;
    };

    // ---- hypotheses: add one's type here, then to `hypotheses_t` ----
    struct iterations_t : poly_iteration_hypotheses<fit_t::converter_t, poly_interpolator, poly_interpolator,
                                                    kernel_t::renderer_t, indices_t, expr_pool> {
        using poly_iteration_hypotheses<fit_t::converter_t, poly_interpolator, poly_interpolator, kernel_t::renderer_t,
                                        indices_t, expr_pool>::poly_iteration_hypotheses;
    };
    using primes_base_t = num_prime_hypotheses<kernel_t::literals_t, num_prime_table, num_prime_table, expr_pool>;
    struct primes_t : primes_base_t {
        using primes_base_t::primes_base_t;
    };
    struct atoms_t : num_condition_atoms<kernel_t::literals_t, kernel_t::literals_t, expr_pool> {
        using num_condition_atoms<kernel_t::literals_t, kernel_t::literals_t, expr_pool>::num_condition_atoms;
    };
    using formulas_t = logic_fit_hypotheses_manifest::hypotheses_t;
    using combinations_base_t =
        logic_condition_hypotheses<atoms_t, conditions_t, formulas_t, expr_size, kernel_t::literals_t,
                                   kernel_t::literals_t, expr_pool>;
    struct combinations_t : combinations_base_t {
        using combinations_base_t::combinations_base_t;
    };
    struct conditions_union_t : pw_hypotheses_union<primes_t, combinations_t> {
        using pw_hypotheses_union<primes_t, combinations_t>::pw_hypotheses_union;
    };
    using guarded_base_t = pw_guarded_hypotheses<conditions_union_t, atoms_t, fit_t::hypotheses_t, fit_t::interpolations_t, body_t, expr_size,
                                                 kernel_t::literals_t, expr_pool>;
    struct guarded_t : guarded_base_t {
        using guarded_base_t::guarded_base_t;
    };
    using hypotheses_base_t = pw_hypotheses_union<
        guarded_t,
        iterations_t,
        primes_t>;
    struct hypotheses_t : hypotheses_base_t {
        using hypotheses_base_t::hypotheses_base_t;
    };

    // ---- rules: add a rule's type here, then to `local_rules_t` (every position) or `root_rules_t` (the root) ----
    struct piecewise_t : pw_fit_rules_manifest<hypotheses_t, pw_evaluator_t, function_maker_t, kernel_t::literals_t> {
        using pw_fit_rules_manifest<hypotheses_t, pw_evaluator_t, function_maker_t,
                                    kernel_t::literals_t>::pw_fit_rules_manifest;
    };

    // ---- the rule lists ----
    using local_rules_base_t = proposer_union<
        piecewise_t::binder_t,
        piecewise_t::absorber_t,
        poly_rules_manifest::local_rules_t>;
    struct local_rules_t : local_rules_base_t {
        using local_rules_base_t::local_rules_base_t;
    };
    struct root_rules_t : proposer_union<> {
        using proposer_union<>::proposer_union;
    };
    struct subterms_t : subterm_proposer<local_rules_t, expr_pool> {
        using subterm_proposer<local_rules_t, expr_pool>::subterm_proposer;
    };
    struct rules_t : proposer_union<subterms_t, root_rules_t> {
        using proposer_union<subterms_t, root_rules_t>::proposer_union;
    };

    poly_fit_rules_manifest(kernel_t& kernel,
                            size_t arity,
                            const std::vector<poly_sample>& samples,
                            const poly_fit_limits& fit_limits);

    poly_rules_manifest poly_rules_;
    pw_symbols pw_symbols_;
    uint32_t point_symbol_;
    uint32_t less_symbol_;
    fit_t fit_;
    abs_symbols abs_symbols_;
    num_symbols num_symbols_;
    logic_symbols logic_symbols_;
    indices_t indices_;
    binders_t binders_;
    reducer_t reducer_;
    function_maker_t function_maker_;
    num_prime_table prime_table_;
    numbers_t numbers_;
    conditions_t conditions_;
    body_t body_;
    applying_t applying_;
    evaluator_t evaluator_;
    pw_evaluator_t pw_evaluator_;
    iterations_t iterations_;
    primes_t primes_;
    atoms_t atoms_;
    logic_fit_hypotheses_manifest condition_logic_;
    combinations_t combinations_;
    conditions_union_t conditions_union_;
    guarded_t guarded_;
    hypotheses_t hypotheses_;
    // rules
    piecewise_t piecewise_;

    // the rule lists
    local_rules_t local_rules_;
    root_rules_t root_rules_;
    subterms_t subterms_;
    rules_t rules_;
    const expr* initial_;
private:
    static std::vector<std::string> name_inputs(size_t arity);
    static std::vector<uint32_t> intern_placeholders(kernel_t& kernel);
};

#endif
