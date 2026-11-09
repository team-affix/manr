#ifndef POLY_ITERATION_HYPOTHESES_HPP
#define POLY_ITERATION_HYPOTHESES_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
#include "value_objects/abs_symbols.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_interpolant.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_data_point.hpp"

constexpr size_t k_poly_fit_default_max_iterated_degree = 16;

// A detector that reads data over one input as a polynomial iterated k times, `g(g(...g(x)))`, and proposes it with a
// helper: `let(lam(g(#0)), app(#0, app(#0, ... #1)))`, the body of the model's `lam`: in the `let`'s scope `#0` is the
// helper and `#1` the input (`#0` outside it), and inside the helper's `lam` `#0` is its parameter. For each degree d
// >= 2 and count k >= 2 with d^k at most `max_degree` and below the number of samples, the data is interpolated at
// degree n = d^k; a polynomial P reproducing every sample is then solved for g with g^k = P, exactly: g's leading
// coefficient is the integer root of P's of order (d^k - 1) / (d - 1), and each next coefficient of g, from the top, is
// the one making the next coefficient of g^k match P's (it enters that coefficient linearly), found by trying 0 and 1
// and dividing. The whole g^k is checked against P before anything is proposed. Whether the helper form is smaller than
// P is the search's choice.
template<typename IFindSamples,
         typename IInterpolate,
         typename IEvaluatePolynomial,
         typename IRender,
         typename IMakeIndex,
         typename IMakeFunctor>
struct poly_iteration_hypotheses {
    poly_iteration_hypotheses(IFindSamples& find_samples,
                              IInterpolate& interpolate,
                              IEvaluatePolynomial& evaluate_polynomial,
                              IRender& render,
                              IMakeIndex& make_index,
                              IMakeFunctor& make_functor,
                              abs_symbols symbols,
                              size_t max_degree = k_poly_fit_default_max_iterated_degree);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    using dense_t = std::vector<integer>;  // coefficients, constant first

    std::optional<dense_t> find_root(const dense_t& target, size_t degree, size_t count) const;
    static dense_t iterate(const dense_t& g, size_t count);
    static dense_t multiply(const dense_t& left, const dense_t& right);
    static std::optional<integer> find_integer_root(const integer& value, size_t order);

    IFindSamples& find_samples_;
    IInterpolate& interpolate_;
    IEvaluatePolynomial& evaluate_polynomial_;
    IRender& render_;
    IMakeIndex& make_index_;
    IMakeFunctor& make_functor_;
    abs_symbols symbols_;
    size_t max_degree_;
};

#define POLY_ITERATION_HYPOTHESES_TEMPLATE                                                                     \
    template<typename IFindSamples, typename IInterpolate, typename IEvaluatePolynomial, typename IRender, \
             typename IMakeIndex, typename IMakeFunctor>
#define POLY_ITERATION_HYPOTHESES \
    poly_iteration_hypotheses<IFindSamples, IInterpolate, IEvaluatePolynomial, IRender, IMakeIndex, IMakeFunctor>

POLY_ITERATION_HYPOTHESES_TEMPLATE POLY_ITERATION_HYPOTHESES::poly_iteration_hypotheses(
    IFindSamples& find_samples,
    IInterpolate& interpolate,
    IEvaluatePolynomial& evaluate_polynomial,
    IRender& render,
    IMakeIndex& make_index,
    IMakeFunctor& make_functor,
    abs_symbols symbols,
    size_t max_degree)
    : find_samples_(find_samples)
    , interpolate_(interpolate)
    , evaluate_polynomial_(evaluate_polynomial)
    , render_(render)
    , make_index_(make_index)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , max_degree_(max_degree) {}

POLY_ITERATION_HYPOTHESES_TEMPLATE void POLY_ITERATION_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    const std::vector<poly_sample> samples = find_samples_.find_samples(points);
    if (samples.size() < 6 || samples.front().inputs.size() != 1)
        return;
    for (size_t degree = 2; degree * degree <= max_degree_; ++degree)
        for (size_t count = 2, total = degree * degree; total <= max_degree_ && total + 2 <= samples.size();
             ++count, total *= degree) {
            const std::optional<poly_interpolant> interpolant = interpolate_.interpolate(samples, total, 0);
            if (!interpolant.has_value())
                continue;
            bool reproduces = true;
            for (const poly_sample& sample : samples)
                reproduces = reproduces
                             && evaluate_polynomial_.evaluate(interpolant.value().polynomial, sample.inputs)
                                    == sample.output;
            if (!reproduces)
                continue;
            dense_t target(total + 1, integer(0));
            bool exact_degree = false;
            for (const auto& [monomial, coefficient] : interpolant.value().polynomial.terms) {
                const size_t power = monomial.powers.empty() ? 0 : monomial.powers.front().exponent;
                target[power] = coefficient;
                exact_degree = exact_degree || power == total;
            }
            if (!exact_degree)
                continue;
            const std::optional<dense_t> root = find_root(target, degree, count);
            if (!root.has_value())
                continue;
            poly_polynomial body;
            const uint32_t parameter = std::get<expr::functor>(make_index_.make_index(0)->content).id;
            for (size_t power = 0; power < root.value().size(); ++power) {
                if (root.value()[power] == 0)
                    continue;
                poly_monomial monomial;
                if (power > 0)
                    monomial.powers.push_back(
                        poly_power{.variable = parameter, .exponent = static_cast<uint32_t>(power)});
                body.terms.emplace(monomial, root.value()[power]);
            }
            const expr* helper = make_index_.make_index(0);
            // the input, one binder further out than the helper
            const expr* calls = make_index_.make_index(1);
            for (size_t call = 0; call < count; ++call)
                calls = make_functor_.make_functor(symbols_.application, {helper, calls});
            const expr* function = make_functor_.make_functor(symbols_.lambda, {render_.render(body)});
            out.push_back(make_functor_.make_functor(symbols_.binding, {function, calls}));
        }
}

// g of the given degree with g iterated `count` times equal to the target, or nothing.
POLY_ITERATION_HYPOTHESES_TEMPLATE std::optional<typename POLY_ITERATION_HYPOTHESES::dense_t>
POLY_ITERATION_HYPOTHESES::find_root(const dense_t& target, size_t degree, size_t count) const {
    const size_t total = target.size() - 1;
    // g^k's leading coefficient is g's to the power 1 + d + ... + d^(k-1)
    size_t order = 0;
    for (size_t layer = 0, power = 1; layer < count; ++layer, power *= degree)
        order += power;
    const std::optional<integer> magnitude = find_integer_root(abs(target.back()), order);
    if (!magnitude.has_value())
        return std::nullopt;
    for (const integer& lead : {magnitude.value(), integer(-magnitude.value())}) {
        dense_t g(degree + 1, integer(0));
        g[degree] = lead;
        bool solved = true;
        for (size_t below = 1; below <= degree && solved; ++below) {
            g[degree - below] = 0;
            const integer at_zero = iterate(g, count)[total - below];
            g[degree - below] = 1;
            const integer slope = iterate(g, count)[total - below] - at_zero;
            const integer gap = target[total - below] - at_zero;
            solved = slope != 0 && gap % slope == 0;
            g[degree - below] = solved ? integer(gap / slope) : integer(0);
        }
        if (solved && iterate(g, count) == target)
            return g;
    }
    return std::nullopt;
}

// g composed with itself `count` times.
POLY_ITERATION_HYPOTHESES_TEMPLATE typename POLY_ITERATION_HYPOTHESES::dense_t POLY_ITERATION_HYPOTHESES::iterate(
    const dense_t& g, size_t count) {
    dense_t result = g;
    for (size_t layer = 1; layer < count; ++layer) {
        // g(result), by Horner
        dense_t composed{g.back()};
        for (size_t power = g.size() - 1; power-- > 0;) {
            composed = multiply(composed, result);
            composed[0] += g[power];
        }
        result = composed;
    }
    return result;
}

POLY_ITERATION_HYPOTHESES_TEMPLATE typename POLY_ITERATION_HYPOTHESES::dense_t POLY_ITERATION_HYPOTHESES::multiply(
    const dense_t& left, const dense_t& right) {
    dense_t product(left.size() + right.size() - 1, integer(0));
    for (size_t first = 0; first < left.size(); ++first)
        for (size_t second = 0; second < right.size(); ++second)
            product[first + second] += left[first] * right[second];
    return product;
}

// The non-negative integer r with r^order = value, or nothing.
POLY_ITERATION_HYPOTHESES_TEMPLATE std::optional<integer> POLY_ITERATION_HYPOTHESES::find_integer_root(
    const integer& value, size_t order) {
    const double estimate = std::pow(value.template convert_to<double>(), 1.0 / static_cast<double>(order));
    const integer near = integer(static_cast<long long>(std::llround(estimate)));
    for (const integer& candidate : {integer(near - 1), near, integer(near + 1)})
        if (candidate >= 0 && boost::multiprecision::pow(candidate, static_cast<unsigned>(order)) == value)
            return candidate;
    return std::nullopt;
}

#undef POLY_ITERATION_HYPOTHESES
#undef POLY_ITERATION_HYPOTHESES_TEMPLATE

#endif
