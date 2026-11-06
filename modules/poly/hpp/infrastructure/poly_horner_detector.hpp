#ifndef POLY_HORNER_DETECTOR_HPP
#define POLY_HORNER_DETECTOR_HPP

#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/poly_polynomial.hpp"

// Detector: proposes the Horner form of a subterm when that is smaller. Dense polynomials in one variable shrink
// from about 4 nodes per term to about 4 nodes per degree with no powers.
template<typename IFindPolynomial, typename IRenderHorner, typename IGetCost>
struct poly_horner_detector {
    poly_horner_detector(IFindPolynomial& find_polynomial, IRenderHorner& render_horner, IGetCost& get_cost);
    void propose(const expr* e, std::vector<const expr*>& out);
private:
    IFindPolynomial& find_polynomial_;
    IRenderHorner& render_horner_;
    IGetCost& get_cost_;
};

#define POLY_HORNER_DETECTOR_TEMPLATE template<typename IFindPolynomial, typename IRenderHorner, typename IGetCost>
#define POLY_HORNER_DETECTOR          poly_horner_detector<IFindPolynomial, IRenderHorner, IGetCost>

POLY_HORNER_DETECTOR_TEMPLATE POLY_HORNER_DETECTOR::poly_horner_detector(
    IFindPolynomial& find_polynomial, IRenderHorner& render_horner, IGetCost& get_cost)
    : find_polynomial_(find_polynomial)
    , render_horner_(render_horner)
    , get_cost_(get_cost) {}

POLY_HORNER_DETECTOR_TEMPLATE void POLY_HORNER_DETECTOR::propose(const expr* e, std::vector<const expr*>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.empty())
        return;
    const std::optional<poly_polynomial> polynomial = find_polynomial_.find_polynomial(e);
    if (!polynomial.has_value())
        return;
    const expr* horner = render_horner_.render_horner(polynomial.value());
    if (get_cost_.cost(horner) >= get_cost_.cost(e))
        return;
    out.push_back(horner);
}

#undef POLY_HORNER_DETECTOR
#undef POLY_HORNER_DETECTOR_TEMPLATE

#endif
