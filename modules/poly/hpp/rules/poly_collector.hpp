#ifndef POLY_COLLECTOR_HPP
#define POLY_COLLECTOR_HPP

#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/rewrite.hpp"

// Detector: expand-and-collect in one step. Detection decides by exact render cost whether the subterm's expanded
// normal form is smaller; the rewrite only renders it. Finds cancellations hidden behind expansions:
// `(x+1)^2 - (x-1)^2` -> `4 * x`.
template<typename IFindPolynomial, typename IRender, typename IFindRenderCost, typename IGetCost>
struct poly_collector {
    poly_collector(IFindPolynomial& find_polynomial, IRender& render, IFindRenderCost& find_render_cost,
                   IGetCost& get_cost);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    IFindPolynomial& find_polynomial_;
    IRender& render_;
    IFindRenderCost& find_render_cost_;
    IGetCost& get_cost_;
};

#define POLY_COLLECTOR_TEMPLATE \
    template<typename IFindPolynomial, typename IRender, typename IFindRenderCost, typename IGetCost>
#define POLY_COLLECTOR poly_collector<IFindPolynomial, IRender, IFindRenderCost, IGetCost>

POLY_COLLECTOR_TEMPLATE POLY_COLLECTOR::poly_collector(
    IFindPolynomial& find_polynomial, IRender& render, IFindRenderCost& find_render_cost, IGetCost& get_cost)
    : find_polynomial_(find_polynomial)
    , render_(render)
    , find_render_cost_(find_render_cost)
    , get_cost_(get_cost) {}

POLY_COLLECTOR_TEMPLATE void POLY_COLLECTOR::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.empty())
        return;
    const std::optional<poly_polynomial> polynomial = find_polynomial_.find_polynomial(e);
    if (!polynomial.has_value())
        return;
    if (find_render_cost_.find_cost(polynomial.value()) >= get_cost_.cost(e))
        return;
    out.push_back([this, collect = polynomial.value()]() { return render_.render(collect); });
}

#undef POLY_COLLECTOR
#undef POLY_COLLECTOR_TEMPLATE

#endif
