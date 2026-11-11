#ifndef SEQ_POWER_CLOSER_HPP
#define SEQ_POWER_CLOSER_HPP

#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_symbols.hpp"
#include "value_objects/rewrite.hpp"
#include "value_objects/seq_symbols.hpp"

// Local rule: closes an affine recurrence of order one, `rec(lam(lam(s)), c, n)` (or `lams(2, s)`) whose step body s is
// `r * #1 + d` (r not 0 or 1; `#1` is the previous term), into its closed form `a * exp(r, n) + b`, with b = -d / (r -
// 1) the fixed point and a = c - b; it fires only when b is an integer. The form drops what is trivial: `exp(r, n)`
// when a is 1, no `+ b` when b is 0, just b when a is 0. Both sides have no value at a negative index, and the result
// is always smaller: the step alone is at least `lam(lam(mul(r, #1)))`, plus `add(.., d)` when d is not 0. A ratio
// built from holes and literals only, `h * #1`, closes the same way to `c * exp(h, n)` (`rec(lam(lam(?0 * #1)), 3, n)`
// is `3 * exp(?0, n)`), leaving the hole for a binding.
template<typename IFindBody, typename IFindPolynomial, typename IFindLiteralValue, typename IMakeLiteral,
         typename IMakeFunctor>
struct seq_power_closer {
    seq_power_closer(IFindBody& find_body,
                     IFindPolynomial& find_polynomial,
                     IFindLiteralValue& find_literal_value,
                     IMakeLiteral& make_literal,
                     IMakeFunctor& make_functor,
                     seq_symbols symbols,
                     poly_symbols arithmetic_symbols);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    bool holds_holes_only(const expr* e, bool& holed) const;

    IFindBody& find_body_;
    IFindPolynomial& find_polynomial_;
    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    seq_symbols symbols_;
    poly_symbols arithmetic_symbols_;
};

#define SEQ_POWER_CLOSER_TEMPLATE                                                                      \
    template<typename IFindBody, typename IFindPolynomial, typename IFindLiteralValue, typename IMakeLiteral, \
             typename IMakeFunctor>
#define SEQ_POWER_CLOSER seq_power_closer<IFindBody, IFindPolynomial, IFindLiteralValue, IMakeLiteral, IMakeFunctor>

SEQ_POWER_CLOSER_TEMPLATE SEQ_POWER_CLOSER::seq_power_closer(
    IFindBody& find_body,
    IFindPolynomial& find_polynomial,
    IFindLiteralValue& find_literal_value,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    seq_symbols symbols,
    poly_symbols arithmetic_symbols)
    : find_body_(find_body)
    , find_polynomial_(find_polynomial)
    , find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , arithmetic_symbols_(arithmetic_symbols) {}

SEQ_POWER_CLOSER_TEMPLATE void SEQ_POWER_CLOSER::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->id != symbols_.recurrence || f->args.size() != 3)
        return;
    const std::optional<integer> start = find_literal_value_.find_value(f->args[1]);
    if (!start.has_value())
        return;
    const std::optional<const expr*> body = find_body_.find_body(f->args[0], 2);
    if (!body.has_value())
        return;
    const expr* index = f->args[2];
    // a ratio holding holes, `h * #1` (no offset): `c * exp(h, n)`
    const expr::functor* product = std::get_if<expr::functor>(&body.value()->content);
    const bool multiplied = product != nullptr && product->id == arithmetic_symbols_.mul && product->args.size() == 2;
    for (size_t side = 0; multiplied && side < 2; ++side) {
        const expr::functor* previous = std::get_if<expr::functor>(&product->args[side]->content);
        const bool previous_term =
            previous != nullptr && previous->id == symbols_.previous[0] && previous->args.empty();
        bool holed = false;
        const bool free = previous_term && holds_holes_only(product->args[1 - side], holed);
        if (!free || !holed)
            continue;
        const integer scale = start.value();
        const expr* ratio = product->args[1 - side];
        out.push_back([this, scale, ratio, index]() {
            if (scale == 0)
                return make_literal_.make_literal(integer(0));
            const expr* power = make_functor_.make_functor(symbols_.power, {ratio, index});
            const expr* scaled = make_literal_.make_literal(scale);
            return scale == 1 ? power : make_functor_.make_functor(arithmetic_symbols_.mul, {scaled, power});
        });
        return;
    }
    const auto step = find_polynomial_.find_polynomial(body.value());
    if (!step.has_value() || step.value().terms.empty() || step.value().terms.size() > 2)
        return;
    integer ratio = 0;
    integer offset = 0;
    for (const auto& [monomial, coefficient] : step.value().terms) {
        const bool previous_term = monomial.powers.size() == 1 && monomial.powers[0].variable == symbols_.previous[0]
                                   && monomial.powers[0].exponent == 1;
        if (monomial.powers.empty())
            offset = coefficient;
        else if (previous_term)
            ratio = coefficient;
        else
            return;
    }
    if (ratio == 0 || ratio == 1)
        return;
    const bool integral = offset % (ratio - 1) == 0;
    if (!integral)
        return;
    const integer fixed_point = -offset / (ratio - 1);
    const integer scale = start.value() - fixed_point;
    out.push_back([this, ratio, fixed_point, scale, index]() {
        if (scale == 0)
            return make_literal_.make_literal(fixed_point);
        const expr* power = make_functor_.make_functor(symbols_.power, {make_literal_.make_literal(ratio), index});
        const expr* scaled = scale == 1 ? power
                                        : make_functor_.make_functor(arithmetic_symbols_.mul,
                                                                     {make_literal_.make_literal(scale), power});
        if (fixed_point == 0)
            return scaled;
        if (fixed_point < 0)
            return make_functor_.make_functor(arithmetic_symbols_.sub,
                                              {scaled, make_literal_.make_literal(integer(-fixed_point))});
        return make_functor_.make_functor(arithmetic_symbols_.add, {scaled, make_literal_.make_literal(fixed_point)});
    });
}

// Whether e is built from holes and literals only (no index), setting `holed` when it holds a hole.
SEQ_POWER_CLOSER_TEMPLATE bool SEQ_POWER_CLOSER::holds_holes_only(const expr* e, bool& holed) const {
    if (std::holds_alternative<expr::var>(e->content)) {
        holed = true;
        return true;
    }
    const expr::functor& f = std::get<expr::functor>(e->content);
    if (f.args.empty())
        return find_literal_value_.find_value(e).has_value();
    bool only = true;
    for (const expr* arg : f.args)
        only = only && holds_holes_only(arg, holed);
    return only;
}

#undef SEQ_POWER_CLOSER
#undef SEQ_POWER_CLOSER_TEMPLATE

#endif
