#ifndef POLY_SPLIT_HYPOTHESES_HPP
#define POLY_SPLIT_HYPOTHESES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_interpolant.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_data_point.hpp"
#include "value_objects/pw_symbols.hpp"

constexpr size_t k_poly_fit_default_max_piece_degree = 6;
constexpr size_t k_poly_fit_default_max_piece_starts = 8;

// A detector that reads the data and proposes a piecewise polynomial: different polynomials on intervals of one
// variable, `pw(when(lt(x, k1), P1), ..., when(lt(x, kn), Pn), Q)`. For each variable, the data is ordered by it and
// cut greedily into runs: from the first point not yet explained, the lowest-degree interpolant that runs furthest
// (points sharing the variable's value are one step, and one step it misses is tolerated if the next fits) and
// reproduces more points than its unknowns is the next piece. Interpolants are taken from each of the next few
// points, so an outlier among the first points of a piece does not hide it; a point no interpolant explains is skipped. Greedy is
// the fewest pieces, since any sub-run of a run is a run. Each bound is where the next piece starts. Proposed when
// there are two pieces or more (one is the interpolation detector's job); points left unexplained stay as branches.
//
// Proposes expanded normal forms; the polynomial rules compress each piece afterwards.
template<typename IFindSamples,
         typename IInterpolate,
         typename IEvaluatePolynomial,
         typename IRender,
         typename IMakeLiteral,
         typename IMakeFunctor>
struct poly_split_hypotheses {
    poly_split_hypotheses(IFindSamples& find_samples,
                          IInterpolate& interpolate,
                          IEvaluatePolynomial& evaluate_polynomial,
                          IRender& render,
                          IMakeLiteral& make_literal,
                          IMakeFunctor& make_functor,
                          std::vector<uint32_t> variables,
                          pw_symbols symbols,
                          uint32_t less_symbol,
                          size_t max_degree = k_poly_fit_default_max_piece_degree,
                          size_t max_starts = k_poly_fit_default_max_piece_starts);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    // samples [first, end) of the ordered data, all but the skipped ones reproduced by `polynomial`
    struct piece {
        size_t first;
        size_t end;
        poly_polynomial polynomial;
    };
    // where a run stopped, and how many points it reproduced
    struct run {
        size_t end;
        size_t reproduced;
    };

    std::vector<piece> find_pieces(const std::vector<poly_sample>& ordered, size_t position) const;
    run find_run(const std::vector<poly_sample>& ordered, size_t position, size_t first,
                 const poly_polynomial& polynomial) const;
    static size_t find_group_end(const std::vector<poly_sample>& ordered, size_t position, size_t index);

    IFindSamples& find_samples_;
    IInterpolate& interpolate_;
    IEvaluatePolynomial& evaluate_polynomial_;
    IRender& render_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    std::vector<uint32_t> variables_;
    pw_symbols symbols_;
    uint32_t less_symbol_;
    size_t max_degree_;
    size_t max_starts_;
};

#define POLY_SPLIT_HYPOTHESES_TEMPLATE                                                                         \
    template<typename IFindSamples, typename IInterpolate, typename IEvaluatePolynomial, typename IRender, \
             typename IMakeLiteral, typename IMakeFunctor>
#define POLY_SPLIT_HYPOTHESES \
    poly_split_hypotheses<IFindSamples, IInterpolate, IEvaluatePolynomial, IRender, IMakeLiteral, IMakeFunctor>

POLY_SPLIT_HYPOTHESES_TEMPLATE POLY_SPLIT_HYPOTHESES::poly_split_hypotheses(
    IFindSamples& find_samples,
    IInterpolate& interpolate,
    IEvaluatePolynomial& evaluate_polynomial,
    IRender& render,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    std::vector<uint32_t> variables,
    pw_symbols symbols,
    uint32_t less_symbol,
    size_t max_degree,
    size_t max_starts)
    : find_samples_(find_samples)
    , interpolate_(interpolate)
    , evaluate_polynomial_(evaluate_polynomial)
    , render_(render)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , variables_(std::move(variables))
    , symbols_(symbols)
    , less_symbol_(less_symbol)
    , max_degree_(max_degree)
    , max_starts_(max_starts) {}

POLY_SPLIT_HYPOTHESES_TEMPLATE void POLY_SPLIT_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    const std::vector<poly_sample> samples = find_samples_.find_samples(points);
    for (size_t position = 0; position < variables_.size(); ++position) {
        std::vector<std::pair<integer, size_t>> order;
        order.reserve(samples.size());
        for (size_t index = 0; index < samples.size(); ++index)
            order.emplace_back(samples[index].inputs[position], index);
        std::sort(order.begin(), order.end());
        std::vector<poly_sample> ordered;
        ordered.reserve(samples.size());
        for (const auto& [value, index] : order)
            ordered.push_back(samples[index]);
        const std::vector<piece> pieces = find_pieces(ordered, position);
        if (pieces.size() < 2)
            continue;
        const poly_monomial monomial{.powers = {poly_power{.variable = variables_[position], .exponent = 1}}};
        const expr* variable = render_.render(poly_polynomial{.terms = {{monomial, integer(1)}}});
        std::vector<const expr*> args;
        for (size_t index = 0; index + 1 < pieces.size(); ++index) {
            const integer& bound = ordered[pieces[index + 1].first].inputs[position];
            const expr* condition = make_functor_.make_functor(less_symbol_, {variable, make_literal_.make_literal(bound)});
            args.push_back(make_functor_.make_functor(symbols_.region, {condition, render_.render(pieces[index].polynomial)}));
        }
        args.push_back(render_.render(pieces.back().polynomial));
        out.push_back(make_functor_.make_functor(symbols_.piecewise, args));
    }
}

// The greedy cut of the ordered data into pieces; adjacent pieces with the same polynomial (split only by points it
// does not explain) are one.
POLY_SPLIT_HYPOTHESES_TEMPLATE std::vector<typename POLY_SPLIT_HYPOTHESES::piece> POLY_SPLIT_HYPOTHESES::find_pieces(
    const std::vector<poly_sample>& ordered, size_t position) const {
    std::vector<piece> pieces;
    size_t first = 0;
    while (first < ordered.size()) {
        std::optional<piece> best;
        const size_t starts = std::min(max_starts_, ordered.size() - first);
        for (size_t degree = 0; degree <= max_degree_; ++degree) {
            for (size_t start = first; start < first + starts; ++start) {
                const std::optional<poly_interpolant> interpolant = interpolate_.interpolate(ordered, degree, start);
                if (!interpolant.has_value())
                    continue;
                const run reached = find_run(ordered, position, first, interpolant.value().polynomial);
                if (reached.reproduced <= interpolant.value().unknowns)
                    continue;
                const bool longer = !best.has_value() || reached.end > best.value().end;
                if (longer)
                    best = piece{.first = first, .end = reached.end, .polynomial = interpolant.value().polynomial};
            }
            const bool complete = best.has_value() && best.value().end == ordered.size();
            if (complete)
                break;
        }
        if (!best.has_value()) {
            first = find_group_end(ordered, position, first);
            continue;
        }
        const bool continues = !pieces.empty() && pieces.back().polynomial == best.value().polynomial;
        if (continues)
            pieces.back().end = best.value().end;
        else
            pieces.push_back(best.value());
        first = best.value().end;
    }
    return pieces;
}

// How far `polynomial` runs from `first`, a group of equal values of the variable at a time: a group fits when every
// point in it is reproduced, and the run ends at the second group in a row that does not fit.
POLY_SPLIT_HYPOTHESES_TEMPLATE typename POLY_SPLIT_HYPOTHESES::run POLY_SPLIT_HYPOTHESES::find_run(
    const std::vector<poly_sample>& ordered, size_t position, size_t first, const poly_polynomial& polynomial) const {
    run reached{.end = first, .reproduced = 0};
    bool missed = false;
    size_t index = first;
    while (index < ordered.size()) {
        const size_t group_end = find_group_end(ordered, position, index);
        size_t hits = 0;
        for (size_t member = index; member < group_end; ++member)
            hits += evaluate_polynomial_.evaluate(polynomial, ordered[member].inputs) == ordered[member].output ? 1 : 0;
        const bool fits = hits == group_end - index;
        index = group_end;
        if (fits) {
            reached.end = group_end;
            reached.reproduced += hits;
            missed = false;
            continue;
        }
        if (missed)
            break;
        missed = true;
    }
    return reached;
}

// The index past the last sample sharing the variable's value with ordered[index].
POLY_SPLIT_HYPOTHESES_TEMPLATE size_t POLY_SPLIT_HYPOTHESES::find_group_end(
    const std::vector<poly_sample>& ordered, size_t position, size_t index) {
    size_t end = index + 1;
    while (end < ordered.size() && ordered[end].inputs[position] == ordered[index].inputs[position])
        ++end;
    return end;
}

#undef POLY_SPLIT_HYPOTHESES
#undef POLY_SPLIT_HYPOTHESES_TEMPLATE

#endif
