#ifndef POLY_RESIDUE_HYPOTHESES_HPP
#define POLY_RESIDUE_HYPOTHESES_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_interpolant.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_data_point.hpp"
#include "value_objects/pw_symbols.hpp"

constexpr size_t k_poly_fit_default_max_modulus = 6;

// A detector that reads data over one variable as periodic: a polynomial per residue class of the variable modulo m,
// `pw(when(eq(mod(x, m), r1), P1), ..., Q)`, for the smallest m from 2 to `max_modulus` that fits. Each class is
// interpolated at the lowest degree reproducing every sample in it, and must hold at least two samples (a class
// with none is free); the polynomial most classes share becomes the otherwise. So a parity, `pw(when(eq(mod(x, 2),
// 0), 1), 0)`, is found as easily as `pw(when(eq(mod(x, 2), 0), x), 0)`. Data that is one polynomial in every class
// is left to the interpolation detector.
template<typename IFindSamples,
         typename IInterpolate,
         typename IEvaluatePolynomial,
         typename IRender,
         typename IMakeLiteral,
         typename IMakeFunctor>
struct poly_residue_hypotheses {
    poly_residue_hypotheses(IFindSamples& find_samples,
                            IInterpolate& interpolate,
                            IEvaluatePolynomial& evaluate_polynomial,
                            IRender& render,
                            IMakeLiteral& make_literal,
                            IMakeFunctor& make_functor,
                            uint32_t variable,
                            pw_symbols symbols,
                            uint32_t equal_symbol,
                            uint32_t modulo_symbol,
                            size_t max_degree,
                            size_t max_modulus = k_poly_fit_default_max_modulus);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    std::optional<poly_polynomial> find_class_polynomial(const std::vector<poly_sample>& members) const;

    IFindSamples& find_samples_;
    IInterpolate& interpolate_;
    IEvaluatePolynomial& evaluate_polynomial_;
    IRender& render_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    uint32_t variable_;
    pw_symbols symbols_;
    uint32_t equal_symbol_;
    uint32_t modulo_symbol_;
    size_t max_degree_;
    size_t max_modulus_;
};

#define POLY_RESIDUE_HYPOTHESES_TEMPLATE                                                                       \
    template<typename IFindSamples, typename IInterpolate, typename IEvaluatePolynomial, typename IRender, \
             typename IMakeLiteral, typename IMakeFunctor>
#define POLY_RESIDUE_HYPOTHESES \
    poly_residue_hypotheses<IFindSamples, IInterpolate, IEvaluatePolynomial, IRender, IMakeLiteral, IMakeFunctor>

POLY_RESIDUE_HYPOTHESES_TEMPLATE POLY_RESIDUE_HYPOTHESES::poly_residue_hypotheses(
    IFindSamples& find_samples,
    IInterpolate& interpolate,
    IEvaluatePolynomial& evaluate_polynomial,
    IRender& render,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    uint32_t variable,
    pw_symbols symbols,
    uint32_t equal_symbol,
    uint32_t modulo_symbol,
    size_t max_degree,
    size_t max_modulus)
    : find_samples_(find_samples)
    , interpolate_(interpolate)
    , evaluate_polynomial_(evaluate_polynomial)
    , render_(render)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , variable_(variable)
    , symbols_(symbols)
    , equal_symbol_(equal_symbol)
    , modulo_symbol_(modulo_symbol)
    , max_degree_(max_degree)
    , max_modulus_(max_modulus) {}

POLY_RESIDUE_HYPOTHESES_TEMPLATE void POLY_RESIDUE_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    const std::vector<poly_sample> samples = find_samples_.find_samples(points);
    if (samples.size() < 4 || samples.front().inputs.size() != 1)
        return;
    for (size_t modulus = 2; modulus <= max_modulus_; ++modulus) {
        std::vector<std::vector<poly_sample>> classes(modulus);
        for (const poly_sample& sample : samples) {
            const integer remainder = sample.inputs[0] % integer(modulus);
            classes[(remainder < 0 ? integer(remainder + modulus) : remainder).template convert_to<size_t>()]
                .push_back(sample);
        }
        std::vector<std::optional<poly_polynomial>> polynomials(modulus);
        bool fits = true;
        for (size_t residue = 0; residue < modulus && fits; ++residue) {
            if (classes[residue].empty())
                continue;
            polynomials[residue] = find_class_polynomial(classes[residue]);
            fits = polynomials[residue].has_value();
        }
        if (!fits)
            continue;
        // the otherwise: the polynomial most classes share
        size_t common = modulus;
        size_t common_count = 0;
        for (size_t residue = 0; residue < modulus; ++residue) {
            if (!polynomials[residue].has_value())
                continue;
            size_t count = 0;
            for (const std::optional<poly_polynomial>& other : polynomials)
                count += other == polynomials[residue] ? 1 : 0;
            if (count <= common_count)
                continue;
            common = residue;
            common_count = count;
        }
        std::vector<const expr*> branches;
        const expr* variable = make_functor_.make_functor(variable_, {});
        const expr* remainder =
            make_functor_.make_functor(modulo_symbol_, {variable, make_literal_.make_literal(integer(modulus))});
        for (size_t residue = 0; residue < modulus; ++residue) {
            if (!polynomials[residue].has_value() || polynomials[residue] == polynomials[common])
                continue;
            const expr* condition =
                make_functor_.make_functor(equal_symbol_, {remainder, make_literal_.make_literal(integer(residue))});
            branches.push_back(make_functor_.make_functor(
                symbols_.region, {condition, render_.render(polynomials[residue].value())}));
        }
        // one polynomial in every class: not periodic, and no larger modulus will be
        if (branches.empty())
            return;
        branches.push_back(render_.render(polynomials[common].value()));
        out.push_back(make_functor_.make_functor(symbols_.piecewise, branches));
        return;
    }
}

// The lowest-degree polynomial reproducing every sample of a residue class, or nothing.
POLY_RESIDUE_HYPOTHESES_TEMPLATE std::optional<poly_polynomial> POLY_RESIDUE_HYPOTHESES::find_class_polynomial(
    const std::vector<poly_sample>& members) const {
    for (size_t degree = 0; degree <= max_degree_; ++degree) {
        const std::optional<poly_interpolant> interpolant = interpolate_.interpolate(members, degree, 0);
        if (!interpolant.has_value())
            return std::nullopt;
        bool reproduces = true;
        for (const poly_sample& member : members)
            reproduces = reproduces &&
                             evaluate_polynomial_.evaluate(interpolant.value().polynomial, member.inputs) == member.output;
        if (reproduces)
            return interpolant.value().polynomial;
    }
    return std::nullopt;
}

#undef POLY_RESIDUE_HYPOTHESES
#undef POLY_RESIDUE_HYPOTHESES_TEMPLATE

#endif
