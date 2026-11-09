#ifndef POLY_INTERPOLATION_HYPOTHESES_HPP
#define POLY_INTERPOLATION_HYPOTHESES_HPP

#include <algorithm>
#include <cstddef>
#include <optional>
#include <set>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/poly_interpolant.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_data_point.hpp"

constexpr size_t k_poly_fit_default_max_degree = 6;
constexpr size_t k_poly_fit_default_max_starts = 8;

// A detector that reads the data and proposes one polynomial through all of it, for any number of inputs.
// For each total degree (lowest first) it interpolates from several starting indices, spread evenly over the data so
// a fit can begin past outliers, even a long run of them. An interpolant is proposed when it reproduces at least one
// point beyond the unknowns that determined it (otherwise it explains nothing). Stops at the first degree whose
// interpolant reproduces every point: a higher degree would find the same polynomial.
//
// Proposes the expanded normal form; the polynomial rules compress it afterwards (Horner, factoring, powers).
template<typename IFindSamples, typename IInterpolate, typename IEvaluatePolynomial, typename IRender>
struct poly_interpolation_hypotheses {
    poly_interpolation_hypotheses(IFindSamples& find_samples,
                                  IInterpolate& interpolate,
                                  IEvaluatePolynomial& evaluate_polynomial,
                                  IRender& render,
                                  size_t max_degree = k_poly_fit_default_max_degree,
                                  size_t max_starts = k_poly_fit_default_max_starts);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    IFindSamples& find_samples_;
    IInterpolate& interpolate_;
    IEvaluatePolynomial& evaluate_polynomial_;
    IRender& render_;
    size_t max_degree_;
    size_t max_starts_;
};

#define POLY_INTERPOLATION_HYPOTHESES_TEMPLATE \
    template<typename IFindSamples, typename IInterpolate, typename IEvaluatePolynomial, typename IRender>
#define POLY_INTERPOLATION_HYPOTHESES \
    poly_interpolation_hypotheses<IFindSamples, IInterpolate, IEvaluatePolynomial, IRender>

POLY_INTERPOLATION_HYPOTHESES_TEMPLATE POLY_INTERPOLATION_HYPOTHESES::poly_interpolation_hypotheses(
    IFindSamples& find_samples,
    IInterpolate& interpolate,
    IEvaluatePolynomial& evaluate_polynomial,
    IRender& render,
    size_t max_degree,
    size_t max_starts)
    : find_samples_(find_samples)
    , interpolate_(interpolate)
    , evaluate_polynomial_(evaluate_polynomial)
    , render_(render)
    , max_degree_(max_degree)
    , max_starts_(max_starts) {}

POLY_INTERPOLATION_HYPOTHESES_TEMPLATE void POLY_INTERPOLATION_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    const std::vector<poly_sample> samples = find_samples_.find_samples(points);
    const size_t starts = std::min(max_starts_, samples.size());
    std::set<poly_polynomial> seen;
    for (size_t degree = 0; degree <= max_degree_; ++degree) {
        for (size_t attempt = 0; attempt < starts; ++attempt) {
            const size_t start = attempt * samples.size() / starts;
            const std::optional<poly_interpolant> interpolant = interpolate_.interpolate(samples, degree, start);
            if (!interpolant.has_value())
                continue;
            const poly_polynomial& polynomial = interpolant.value().polynomial;
            size_t reproduced = 0;
            for (const poly_sample& sample : samples)
                reproduced += evaluate_polynomial_.evaluate(polynomial, sample.inputs) == sample.output ? 1 : 0;
            if (reproduced <= interpolant.value().unknowns)
                continue;
            if (!seen.insert(polynomial).second)
                continue;
            out.push_back(render_.render(polynomial));
            if (reproduced == samples.size())
                return;
        }
    }
}

#undef POLY_INTERPOLATION_HYPOTHESES
#undef POLY_INTERPOLATION_HYPOTHESES_TEMPLATE

#endif
