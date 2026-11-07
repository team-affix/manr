#ifndef POLY_RENDERER_HPP
#define POLY_RENDERER_HPP

#include <cstddef>
#include <cstdint>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_symbols.hpp"

// Renders a normal form as its expanded sum of monomials, leading term first: unit coefficients and exponents are
// left out, `x^n` is `pow(x, n)`, and negative terms after the first are subtracted.
template<typename IMakeFunctor, typename IMakeLiteral>
struct poly_renderer {
    poly_renderer(IMakeFunctor& make_functor, IMakeLiteral& make_literal, poly_symbols symbols);
    const expr* render(const poly_polynomial& polynomial);
    const expr* render_monomial(const poly_monomial& monomial);
    // The node count of what `render` would build, without building it.
    size_t find_cost(const poly_polynomial& polynomial) const;
private:
    const expr* render_term(const poly_monomial& monomial, const integer& coefficient);
    static size_t find_term_cost(const poly_monomial& monomial, bool unit_coefficient);

    IMakeFunctor& make_functor_;
    IMakeLiteral& make_literal_;
    poly_symbols symbols_;
};

#define POLY_RENDERER_TEMPLATE template<typename IMakeFunctor, typename IMakeLiteral>
#define POLY_RENDERER          poly_renderer<IMakeFunctor, IMakeLiteral>

POLY_RENDERER_TEMPLATE POLY_RENDERER::poly_renderer(
    IMakeFunctor& make_functor, IMakeLiteral& make_literal, poly_symbols symbols)
    : make_functor_(make_functor)
    , make_literal_(make_literal)
    , symbols_(symbols) {}

POLY_RENDERER_TEMPLATE const expr* POLY_RENDERER::render(const poly_polynomial& polynomial) {
    if (polynomial.terms.empty())
        return make_literal_.make_literal(0);
    const expr* sum = nullptr;
    for (auto term = polynomial.terms.rbegin(); term != polynomial.terms.rend(); ++term) {
        if (sum == nullptr) {
            sum = render_term(term->first, term->second);
            continue;
        }
        const bool negative = term->second < 0;
        const expr* magnitude = render_term(term->first, negative ? integer(-term->second) : term->second);
        sum = make_functor_.make_functor(negative ? symbols_.sub : symbols_.add, {sum, magnitude});
    }
    return sum;
}

POLY_RENDERER_TEMPLATE const expr* POLY_RENDERER::render_monomial(const poly_monomial& monomial) {
    return render_term(monomial, 1);
}

POLY_RENDERER_TEMPLATE size_t POLY_RENDERER::find_cost(const poly_polynomial& polynomial) const {
    if (polynomial.terms.empty())
        return 1;
    size_t cost = polynomial.terms.size() - 1;
    for (auto term = polynomial.terms.rbegin(); term != polynomial.terms.rend(); ++term) {
        const bool leading = term == polynomial.terms.rbegin();
        const bool unit = leading ? term->second == 1 : boost::multiprecision::abs(term->second) == 1;
        cost += find_term_cost(term->first, unit);
    }
    return cost;
}

// Mirrors `render_term`: a literal unless the coefficient is a bare 1, each factor 1 node (or 3 for `pow`), and one
// `mul` joining each factor after the first.
POLY_RENDERER_TEMPLATE size_t POLY_RENDERER::find_term_cost(const poly_monomial& monomial, bool unit_coefficient) {
    const bool bare = unit_coefficient && !monomial.powers.empty();
    size_t cost = bare ? 0 : 1;
    for (const poly_power& power : monomial.powers)
        cost += power.exponent == 1 ? 1 : 3;
    const size_t factors = monomial.powers.size() + (bare ? 0 : 1);
    return cost + (factors > 0 ? factors - 1 : 0);
}

// A left-nested product, coefficient first: `3 * x * y^2` is `mul(mul(3, x), pow(y, 2))`.
POLY_RENDERER_TEMPLATE const expr* POLY_RENDERER::render_term(const poly_monomial& monomial, const integer& coefficient) {
    const bool bare = coefficient == 1 && !monomial.powers.empty();
    const expr* product = bare ? nullptr : make_literal_.make_literal(coefficient);
    for (const poly_power& power : monomial.powers) {
        const expr* variable = make_functor_.make_functor(power.variable, {});
        const expr* factor = power.exponent == 1
            ? variable
            : make_functor_.make_functor(symbols_.pow, {variable, make_literal_.make_literal(power.exponent)});
        product = product == nullptr ? factor : make_functor_.make_functor(symbols_.mul, {product, factor});
    }
    return product;
}

#undef POLY_RENDERER
#undef POLY_RENDERER_TEMPLATE

#endif
