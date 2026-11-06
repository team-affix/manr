#ifndef POLY_POWER_DETECTOR_HPP
#define POLY_POWER_DETECTOR_HPP

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_root.hpp"
#include "value_objects/poly_symbols.hpp"

// Detector: recognizes a subterm as a power of something plus a leftover, `q^n + r`, for every n up to the
// polynomial's total degree (capped at `max_degree`), and proposes it when smaller. Exact powers (r = 0) are the
// binomial and multinomial theorems run backwards; with r != 0 it is completing the square, cube, ....
// q is proposed expanded; other rules may then factor it.
template<typename IFindPolynomial, typename IFindRoots, typename IRender, typename IMakeFunctor, typename IMakeLiteral,
         typename IGetCost>
struct poly_power_detector {
    poly_power_detector(IFindPolynomial& find_polynomial,
                        IFindRoots& find_roots,
                        IRender& render,
                        IMakeFunctor& make_functor,
                        IMakeLiteral& make_literal,
                        IGetCost& get_cost,
                        poly_symbols symbols,
                        uint32_t max_degree);
    void propose(const expr* e, std::vector<const expr*>& out);
private:
    IFindPolynomial& find_polynomial_;
    IFindRoots& find_roots_;
    IRender& render_;
    IMakeFunctor& make_functor_;
    IMakeLiteral& make_literal_;
    IGetCost& get_cost_;
    poly_symbols symbols_;
    uint32_t max_degree_;
};

#define POLY_POWER_DETECTOR_TEMPLATE                                                                        \
    template<typename IFindPolynomial, typename IFindRoots, typename IRender, typename IMakeFunctor, \
             typename IMakeLiteral, typename IGetCost>
#define POLY_POWER_DETECTOR \
    poly_power_detector<IFindPolynomial, IFindRoots, IRender, IMakeFunctor, IMakeLiteral, IGetCost>

POLY_POWER_DETECTOR_TEMPLATE POLY_POWER_DETECTOR::poly_power_detector(
    IFindPolynomial& find_polynomial,
    IFindRoots& find_roots,
    IRender& render,
    IMakeFunctor& make_functor,
    IMakeLiteral& make_literal,
    IGetCost& get_cost,
    poly_symbols symbols,
    uint32_t max_degree)
    : find_polynomial_(find_polynomial)
    , find_roots_(find_roots)
    , render_(render)
    , make_functor_(make_functor)
    , make_literal_(make_literal)
    , get_cost_(get_cost)
    , symbols_(symbols)
    , max_degree_(max_degree) {}

POLY_POWER_DETECTOR_TEMPLATE void POLY_POWER_DETECTOR::propose(const expr* e, std::vector<const expr*>& out) {
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
        for (const poly_root& root : find_roots_.find_roots(polynomial.value(), degree)) {
            if (root.root.terms.size() < 2 && !root.remainder.terms.empty())
                continue;
            const expr* power = make_functor_.make_functor(
                symbols_.pow, {render_.render(root.root), make_literal_.make_literal(degree)});
            const expr* proposal = power;
            if (!root.remainder.terms.empty()) {
                const bool negative = root.remainder.terms.rbegin()->second < 0;
                poly_polynomial magnitude = root.remainder;
                for (auto& [monomial, coefficient] : magnitude.terms)
                    coefficient = negative ? integer(-coefficient) : coefficient;
                proposal = make_functor_.make_functor(
                    negative ? symbols_.sub : symbols_.add, {power, render_.render(magnitude)});
            }
            if (get_cost_.cost(proposal) >= cost)
                continue;
            out.push_back(proposal);
        }
    }
}

#undef POLY_POWER_DETECTOR
#undef POLY_POWER_DETECTOR_TEMPLATE

#endif
