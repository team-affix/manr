#ifndef POLY_HORNER_RENDERER_HPP
#define POLY_HORNER_RENDERER_HPP

#include <cstdint>
#include <algorithm>
#include <map>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_symbols.hpp"

// Renders a normal form in (multivariate) Horner form: pick the variable of highest degree, write the polynomial
// as a polynomial in it whose coefficients are polynomials in the rest, nest the multiplications, and recurse into
// the coefficients. `x^5 + 2x^4 + 3x^3` becomes `((x + 2) * x + 3) * x^3`.
template<typename IMakeFunctor, typename IMakeLiteral, typename IRender>
struct poly_horner_renderer {
    poly_horner_renderer(IMakeFunctor& make_functor, IMakeLiteral& make_literal, IRender& render, poly_symbols symbols);
    const expr* render_horner(const poly_polynomial& polynomial);
private:
    const expr* make_power(uint32_t variable, uint32_t exponent);

    IMakeFunctor& make_functor_;
    IMakeLiteral& make_literal_;
    IRender& render_;
    poly_symbols symbols_;
};

#define POLY_HORNER_RENDERER_TEMPLATE template<typename IMakeFunctor, typename IMakeLiteral, typename IRender>
#define POLY_HORNER_RENDERER          poly_horner_renderer<IMakeFunctor, IMakeLiteral, IRender>

POLY_HORNER_RENDERER_TEMPLATE POLY_HORNER_RENDERER::poly_horner_renderer(
    IMakeFunctor& make_functor, IMakeLiteral& make_literal, IRender& render, poly_symbols symbols)
    : make_functor_(make_functor)
    , make_literal_(make_literal)
    , render_(render)
    , symbols_(symbols) {}

POLY_HORNER_RENDERER_TEMPLATE const expr* POLY_HORNER_RENDERER::render_horner(const poly_polynomial& polynomial) {
    std::map<uint32_t, uint32_t> degrees;
    for (const auto& [monomial, coefficient] : polynomial.terms)
        for (const poly_power& power : monomial.powers)
            degrees[power.variable] = std::max(degrees[power.variable], power.exponent);
    if (polynomial.terms.size() < 2 || degrees.empty())
        return render_.render(polynomial);
    uint32_t variable = degrees.begin()->first;
    for (const auto& [candidate, degree] : degrees)
        if (degree > degrees[variable])
            variable = candidate;
    // exponent of `variable` -> its coefficient, a polynomial in the other variables
    std::map<uint32_t, poly_polynomial> coefficients;
    for (const auto& [monomial, coefficient] : polynomial.terms) {
        uint32_t exponent = 0;
        poly_monomial rest;
        for (const poly_power& power : monomial.powers) {
            if (power.variable == variable) { exponent = power.exponent; continue; }
            rest.powers.push_back(power);
        }
        coefficients[exponent].terms.emplace(rest, coefficient);
    }
    // `nested` stays null while the running coefficient is exactly 1, so a unit coefficient never renders `1 * x`.
    const expr* nested = nullptr;
    bool started = false;
    uint32_t previous = 0;
    for (auto level = coefficients.rbegin(); level != coefficients.rend(); ++level) {
        const auto& [exponent, coefficient] = *level;
        if (started) {
            const expr* step = make_power(variable, previous - exponent);
            nested = nested == nullptr ? step : make_functor_.make_functor(symbols_.mul, {nested, step});
        }
        previous = exponent;
        const bool constant = coefficient.terms.size() == 1 && coefficient.terms.begin()->first.powers.empty();
        const integer value = constant ? coefficient.terms.begin()->second : integer(0);
        if (!started) {
            started = true;
            nested = constant && value == 1 ? nullptr : render_horner(coefficient);
            continue;
        }
        if (constant && value < 0) {
            nested = make_functor_.make_functor(symbols_.sub, {nested, make_literal_.make_literal(integer(-value))});
            continue;
        }
        nested = make_functor_.make_functor(symbols_.add, {nested, render_horner(coefficient)});
    }
    if (previous == 0)
        return nested;
    const expr* step = make_power(variable, previous);
    return nested == nullptr ? step : make_functor_.make_functor(symbols_.mul, {nested, step});
}

POLY_HORNER_RENDERER_TEMPLATE const expr* POLY_HORNER_RENDERER::make_power(uint32_t variable, uint32_t exponent) {
    const expr* base = make_functor_.make_functor(variable, {});
    if (exponent == 1)
        return base;
    return make_functor_.make_functor(symbols_.pow, {base, make_literal_.make_literal(exponent)});
}

#undef POLY_HORNER_RENDERER
#undef POLY_HORNER_RENDERER_TEMPLATE

#endif
