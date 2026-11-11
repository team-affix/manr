#ifndef RAT_QUOTIENT_CANCELLER_HPP
#define RAT_QUOTIENT_CANCELLER_HPP

#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/rat_symbols.hpp"
#include "value_objects/rewrite.hpp"

// Local rule: cancels the common factor of a quotient of polynomials, `div(n, d)`: when d divides n, the quotient
// itself (`(x^2 - 1) / (x - 1)` is `x + 1`); otherwise n and d each divided by their greatest
// common divisor (`(x^2 - 1) / (x^2 + 2 * x + 1)` is `(x - 1) / (x + 1)`). The cancelled form also has a value where
// the factor was 0, its continuous extension, as computer algebra does; where d was 0 the quotient had none. Fires
// only when the result is smaller. Nothing for a divisor that is 0 or a quotient that is not of polynomials.
template<typename IFindPolynomial, typename IFindQuotient, typename IFindGcd, typename IRender, typename IMakeFunctor,
         typename IFindCost>
struct rat_quotient_canceller {
    rat_quotient_canceller(IFindPolynomial& find_polynomial,
                           IFindQuotient& find_quotient,
                           IFindGcd& find_gcd,
                           IRender& render,
                           IMakeFunctor& make_functor,
                           IFindCost& find_cost,
                           rat_symbols symbols);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    IFindPolynomial& find_polynomial_;
    IFindQuotient& find_quotient_;
    IFindGcd& find_gcd_;
    IRender& render_;
    IMakeFunctor& make_functor_;
    IFindCost& find_cost_;
    rat_symbols symbols_;
};

#define RAT_QUOTIENT_CANCELLER_TEMPLATE                                                                         \
    template<typename IFindPolynomial, typename IFindQuotient, typename IFindGcd, typename IRender,            \
             typename IMakeFunctor, typename IFindCost>
#define RAT_QUOTIENT_CANCELLER                                                                                 \
    rat_quotient_canceller<IFindPolynomial, IFindQuotient, IFindGcd, IRender, IMakeFunctor, IFindCost>

RAT_QUOTIENT_CANCELLER_TEMPLATE RAT_QUOTIENT_CANCELLER::rat_quotient_canceller(IFindPolynomial& find_polynomial,
                                                                               IFindQuotient& find_quotient,
                                                                               IFindGcd& find_gcd,
                                                                               IRender& render,
                                                                               IMakeFunctor& make_functor,
                                                                               IFindCost& find_cost,
                                                                               rat_symbols symbols)
    : find_polynomial_(find_polynomial)
    , find_quotient_(find_quotient)
    , find_gcd_(find_gcd)
    , render_(render)
    , make_functor_(make_functor)
    , find_cost_(find_cost)
    , symbols_(symbols) {}

RAT_QUOTIENT_CANCELLER_TEMPLATE void RAT_QUOTIENT_CANCELLER::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->id != symbols_.quotient || f->args.size() != 2)
        return;
    const std::optional<poly_polynomial> numerator = find_polynomial_.find_polynomial(f->args[0]);
    if (!numerator.has_value())
        return;
    const std::optional<poly_polynomial> denominator = find_polynomial_.find_polynomial(f->args[1]);
    if (!denominator.has_value() || denominator.value().terms.empty())
        return;
    const expr* cancelled = nullptr;
    const std::optional<poly_polynomial> exact = find_quotient_.find_quotient(numerator.value(), denominator.value());
    if (exact.has_value())
        cancelled = render_.render(exact.value());
    const std::optional<poly_polynomial> common =
        exact.has_value() ? std::nullopt : find_gcd_.find_gcd(numerator.value(), denominator.value());
    const bool shared = common.has_value() && !common.value().terms.rbegin()->first.powers.empty();
    if (shared) {
        const std::optional<poly_polynomial> top = find_quotient_.find_quotient(numerator.value(), common.value());
        const std::optional<poly_polynomial> bottom = find_quotient_.find_quotient(denominator.value(), common.value());
        if (top.has_value() && bottom.has_value())
            cancelled = make_functor_.make_functor(symbols_.quotient,
                                                   {render_.render(top.value()), render_.render(bottom.value())});
    }
    if (cancelled == nullptr || find_cost_.cost(cancelled) >= find_cost_.cost(e))
        return;
    out.push_back([cancelled]() { return cancelled; });
}

#undef RAT_QUOTIENT_CANCELLER
#undef RAT_QUOTIENT_CANCELLER_TEMPLATE

#endif
