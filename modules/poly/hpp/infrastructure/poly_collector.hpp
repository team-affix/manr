#ifndef POLY_COLLECTOR_HPP
#define POLY_COLLECTOR_HPP

#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/poly_polynomial.hpp"

// Detector: expand-and-collect in one step. Proposes the expanded normal form of a subterm when that is smaller,
// which finds cancellations hidden behind expansions: `(x+1)^2 - (x-1)^2` -> `4 * x`.
template<typename IFindPolynomial, typename IRender, typename IGetCost>
struct poly_collector {
    poly_collector(IFindPolynomial& find_polynomial, IRender& render, IGetCost& get_cost);
    void propose(const expr* e, std::vector<const expr*>& out);
private:
    IFindPolynomial& find_polynomial_;
    IRender& render_;
    IGetCost& get_cost_;
};

#define POLY_COLLECTOR_TEMPLATE template<typename IFindPolynomial, typename IRender, typename IGetCost>
#define POLY_COLLECTOR          poly_collector<IFindPolynomial, IRender, IGetCost>

POLY_COLLECTOR_TEMPLATE POLY_COLLECTOR::poly_collector(IFindPolynomial& find_polynomial, IRender& render, IGetCost& get_cost)
    : find_polynomial_(find_polynomial)
    , render_(render)
    , get_cost_(get_cost) {}

POLY_COLLECTOR_TEMPLATE void POLY_COLLECTOR::propose(const expr* e, std::vector<const expr*>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.empty())
        return;
    const std::optional<poly_polynomial> polynomial = find_polynomial_.find_polynomial(e);
    if (!polynomial.has_value())
        return;
    const expr* collected = render_.render(polynomial.value());
    if (get_cost_.cost(collected) >= get_cost_.cost(e))
        return;
    out.push_back(collected);
}

#undef POLY_COLLECTOR
#undef POLY_COLLECTOR_TEMPLATE

#endif
