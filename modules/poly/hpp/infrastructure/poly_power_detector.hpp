#ifndef POLY_POWER_DETECTOR_HPP
#define POLY_POWER_DETECTOR_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_root.hpp"
#include "value_objects/poly_symbols.hpp"
#include "value_objects/rewrite.hpp"

constexpr uint32_t k_poly_default_max_power_degree = 8;

// Detector: recognizes a subterm as a power of something plus a leftover, `q^n + r`, for every n up to the
// polynomial's total degree (capped at `max_degree`). Exact powers (r = 0) are the binomial and multinomial theorems
// run backwards; with r != 0 it is completing the square, cube, ....
// Detection decides: for each degree with a possible starting term (a monomial that is an n-th power), it extracts
// the roots and keeps the smallest q^n + r, by exact render cost, if smaller than the subterm. The rewrite only
// builds it. q is rendered expanded; other rules may factor it.
template<typename IFindPolynomial, typename IFindRoots, typename IRender, typename IFindRenderCost,
         typename IMakeFunctor, typename IMakeLiteral, typename IGetCost>
struct poly_power_detector {
    poly_power_detector(IFindPolynomial& find_polynomial,
                        IFindRoots& find_roots,
                        IRender& render,
                        IFindRenderCost& find_render_cost,
                        IMakeFunctor& make_functor,
                        IMakeLiteral& make_literal,
                        IGetCost& get_cost,
                        poly_symbols symbols,
                        uint32_t max_degree = k_poly_default_max_power_degree);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    std::optional<poly_root> find_smallest(const poly_polynomial& polynomial, uint32_t degree, size_t below);
    const expr* build(const poly_root& root);
    static poly_polynomial find_magnitude(const poly_polynomial& remainder);

    IFindPolynomial& find_polynomial_;
    IFindRoots& find_roots_;
    IRender& render_;
    IFindRenderCost& find_render_cost_;
    IMakeFunctor& make_functor_;
    IMakeLiteral& make_literal_;
    IGetCost& get_cost_;
    poly_symbols symbols_;
    uint32_t max_degree_;
};

#define POLY_POWER_DETECTOR_TEMPLATE                                                                            \
    template<typename IFindPolynomial, typename IFindRoots, typename IRender, typename IFindRenderCost, \
             typename IMakeFunctor, typename IMakeLiteral, typename IGetCost>
#define POLY_POWER_DETECTOR                                                                                   \
    poly_power_detector<IFindPolynomial, IFindRoots, IRender, IFindRenderCost, IMakeFunctor, IMakeLiteral, \
                        IGetCost>

POLY_POWER_DETECTOR_TEMPLATE POLY_POWER_DETECTOR::poly_power_detector(
    IFindPolynomial& find_polynomial,
    IFindRoots& find_roots,
    IRender& render,
    IFindRenderCost& find_render_cost,
    IMakeFunctor& make_functor,
    IMakeLiteral& make_literal,
    IGetCost& get_cost,
    poly_symbols symbols,
    uint32_t max_degree)
    : find_polynomial_(find_polynomial)
    , find_roots_(find_roots)
    , render_(render)
    , find_render_cost_(find_render_cost)
    , make_functor_(make_functor)
    , make_literal_(make_literal)
    , get_cost_(get_cost)
    , symbols_(symbols)
    , max_degree_(max_degree) {}

POLY_POWER_DETECTOR_TEMPLATE void POLY_POWER_DETECTOR::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.empty())
        return;
    const std::optional<poly_polynomial> polynomial = find_polynomial_.find_polynomial(e);
    if (!polynomial.has_value() || polynomial.value().terms.size() < 2)
        return;
    uint32_t total_degree = 0;
    for (const auto& [monomial, coefficient] : polynomial.value().terms) {
        uint32_t degree = 0;
        for (const poly_power& power : monomial.powers)
            degree += power.exponent;
        total_degree = std::max(total_degree, degree);
    }
    const size_t cost = get_cost_.cost(e);
    for (uint32_t degree = 2; degree <= std::min(total_degree, max_degree_); ++degree) {
        // Necessary: q^n needs a starting term whose monomial is an n-th power.
        bool has_start = false;
        for (const auto& [monomial, coefficient] : polynomial.value().terms) {
            bool divisible = !monomial.powers.empty();
            for (const poly_power& power : monomial.powers)
                divisible = divisible && power.exponent % degree == 0;
            has_start = has_start || divisible;
        }
        if (!has_start)
            continue;
        const std::optional<poly_root> smallest = find_smallest(polynomial.value(), degree, cost);
        if (!smallest.has_value())
            continue;
        out.push_back([this, root = smallest.value()]() { return build(root); });
    }
}

// The root of this degree whose q^n + r renders smallest, if that is below `below`.
POLY_POWER_DETECTOR_TEMPLATE std::optional<poly_root> POLY_POWER_DETECTOR::find_smallest(
    const poly_polynomial& polynomial, uint32_t degree, size_t below) {
    std::optional<poly_root> smallest = std::nullopt;
    size_t smallest_cost = below;
    for (const poly_root& root : find_roots_.find_roots(polynomial, degree)) {
        if (root.root.terms.size() < 2 && !root.remainder.terms.empty())
            continue;
        // pow(q, n) is 2 nodes beyond q; a remainder adds an add/sub node and itself.
        size_t cost = find_render_cost_.find_cost(root.root) + 2;
        if (!root.remainder.terms.empty())
            cost += 1 + find_render_cost_.find_cost(find_magnitude(root.remainder));
        if (cost >= smallest_cost)
            continue;
        smallest = root;
        smallest_cost = cost;
    }
    return smallest;
}

POLY_POWER_DETECTOR_TEMPLATE const expr* POLY_POWER_DETECTOR::build(const poly_root& root) {
    const expr* power =
        make_functor_.make_functor(symbols_.pow, {render_.render(root.root), make_literal_.make_literal(root.degree)});
    if (root.remainder.terms.empty())
        return power;
    const bool negative = root.remainder.terms.rbegin()->second < 0;
    return make_functor_.make_functor(
        negative ? symbols_.sub : symbols_.add, {power, render_.render(find_magnitude(root.remainder))});
}

// The remainder with its leading coefficient made positive, so it can follow `+` or `-`.
POLY_POWER_DETECTOR_TEMPLATE poly_polynomial POLY_POWER_DETECTOR::find_magnitude(const poly_polynomial& remainder) {
    const bool negative = remainder.terms.rbegin()->second < 0;
    poly_polynomial magnitude = remainder;
    for (auto& [monomial, coefficient] : magnitude.terms)
        coefficient = negative ? integer(-coefficient) : coefficient;
    return magnitude;
}

#undef POLY_POWER_DETECTOR
#undef POLY_POWER_DETECTOR_TEMPLATE

#endif
