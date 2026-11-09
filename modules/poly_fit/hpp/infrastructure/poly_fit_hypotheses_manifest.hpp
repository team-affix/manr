#ifndef POLY_FIT_HYPOTHESES_MANIFEST_HPP
#define POLY_FIT_HYPOTHESES_MANIFEST_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/poly_condition_evaluator.hpp"
#include "infrastructure/poly_interpolation_hypotheses.hpp"
#include "infrastructure/poly_interpolator.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "infrastructure/poly_point_evaluator.hpp"
#include "infrastructure/poly_residue_hypotheses.hpp"
#include "infrastructure/poly_sample_converter.hpp"
#include "infrastructure/poly_split_hypotheses.hpp"
#include "infrastructure/pw_constant_hypotheses.hpp"
#include "infrastructure/pw_evaluator.hpp"
#include "infrastructure/pw_hypotheses_union.hpp"
#include "value_objects/poly_fit_limits.hpp"
#include "value_objects/pw_symbols.hpp"

// Polynomial fitting over the given index variables (`#k`), as a hypothesis source and an evaluator: given data points
// whose inputs are `point(v1, ..., vk)` of literals, `hypotheses_` proposes constants, interpolated polynomials,
// piecewise polynomials over intervals of one variable and, over one variable, periodic ones (a polynomial per residue
// class, regions `eq(mod(x, m), r)`); `pw_evaluator_` evaluates any of them at a point. Another domain that reduces
// part of its problem to polynomial data owns one per set of variables.
struct poly_fit_hypotheses_manifest {
    using kernel_t = poly_kernel_manifest;
    struct converter_t : poly_sample_converter<kernel_t::literals_t, expr_pool, kernel_t::literals_t> {
        using poly_sample_converter<kernel_t::literals_t, expr_pool, kernel_t::literals_t>::poly_sample_converter;
    };
    struct evaluator_t : poly_point_evaluator<kernel_t::expander_t, kernel_t::literals_t, kernel_t::literals_t> {
        using poly_point_evaluator<kernel_t::expander_t, kernel_t::literals_t, kernel_t::literals_t>::poly_point_evaluator;
    };
    struct conditions_t : poly_condition_evaluator<evaluator_t, kernel_t::literals_t> {
        using poly_condition_evaluator<evaluator_t, kernel_t::literals_t>::poly_condition_evaluator;
    };
    struct pw_evaluator_t : pw_evaluator<evaluator_t, conditions_t> {
        using pw_evaluator<evaluator_t, conditions_t>::pw_evaluator;
    };
    struct interpolations_t
        : poly_interpolation_hypotheses<converter_t, poly_interpolator, poly_interpolator, kernel_t::renderer_t> {
        using poly_interpolation_hypotheses<converter_t, poly_interpolator, poly_interpolator, kernel_t::renderer_t>::
            poly_interpolation_hypotheses;
    };
    struct splits_t : poly_split_hypotheses<converter_t, poly_interpolator, poly_interpolator, kernel_t::renderer_t,
                                            expr_size, kernel_t::literals_t, expr_pool> {
        using poly_split_hypotheses<converter_t, poly_interpolator, poly_interpolator, kernel_t::renderer_t,
                                    expr_size, kernel_t::literals_t, expr_pool>::poly_split_hypotheses;
    };
    struct residues_t : poly_residue_hypotheses<converter_t, poly_interpolator, poly_interpolator, kernel_t::renderer_t,
                                                kernel_t::literals_t, expr_pool> {
        using poly_residue_hypotheses<converter_t, poly_interpolator, poly_interpolator, kernel_t::renderer_t,
                                      kernel_t::literals_t, expr_pool>::poly_residue_hypotheses;
    };
    struct constants_t : pw_constant_hypotheses<expr_size> {
        using pw_constant_hypotheses<expr_size>::pw_constant_hypotheses;
    };
    struct hypotheses_t : pw_hypotheses_union<constants_t, interpolations_t, splits_t, residues_t> {
        using pw_hypotheses_union<constants_t, interpolations_t, splits_t, residues_t>::pw_hypotheses_union;
    };

    poly_fit_hypotheses_manifest(kernel_t& kernel,
                                 const std::vector<std::string>& variables,
                                 const poly_fit_limits& fit_limits,
                                 pw_symbols symbols,
                                 uint32_t point_symbol,
                                 uint32_t less_symbol);

    std::vector<uint32_t> variables_;
    uint32_t equal_symbol_;
    uint32_t modulo_symbol_;
    converter_t converter_;
    poly_interpolator interpolator_;
    evaluator_t evaluator_;
    conditions_t conditions_;
    pw_evaluator_t pw_evaluator_;
    constants_t constants_;
    interpolations_t interpolations_;
    splits_t splits_;
    residues_t residues_;
    hypotheses_t hypotheses_;
private:
    static std::vector<uint32_t> intern_variables(kernel_t& kernel, const std::vector<std::string>& variables);
};

#endif
