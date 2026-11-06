#ifndef POLY_LOCAL_RULES_HPP
#define POLY_LOCAL_RULES_HPP

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_symbols.hpp"

// The low-level rules of a commutative ring, each looking only at the node it is handed and its children:
//   fold         two literal operands (and pow of literals) -> literal
//   identity     e+0, 0+e, e-0, e*1, 1*e, e^1 -> e;  e*0, 0*e, e-e -> 0;  e^0 -> 1
//   power        e*e -> e^2;  e^a*e -> e^(a+1);  e*e^a -> e^(a+1);  e^a*e^b -> e^(a+b);  (e^a)^b -> e^(ab)
//   like terms   e+e -> 2e;  c*e+e, e+c*e -> (c+1)e;  c*e-e -> (c-1)e
//   factor       a*b +- a*c, b*a +- c*a, a*b +- c*a, b*a +- a*c -> a*(b +- c)       (shrinks)
//   distribute   a*(b +- c) -> a*b +- a*c;  (b +- c)*a -> b*a +- c*a                (grows)
//   expand       e^n -> e * e^(n-1)                                                (grows)
//   commute      a+b -> b+a;  a*b -> b*a
//   associate    (a+b)+c <-> a+(b+c);  (a*b)*c <-> a*(b*c);  a+(b-c) <-> (a+b)-c;
//                a-(b+c) <-> (a-b)-c;  a-(b-c) -> (a-b)+c
// Together these can reach any equal polynomial, given enough search; detectors exist so search rarely has to.
template<typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
struct poly_local_rules {
    poly_local_rules(IFindLiteralValue& find_literal_value,
                     IMakeLiteral& make_literal,
                     IMakeFunctor& make_functor,
                     poly_symbols symbols);
    void propose(const expr* e, std::vector<const expr*>& out);
private:
    const expr* make(uint32_t id, const expr* lhs, const expr* rhs);
    const expr* find_operand(const expr* e, uint32_t id, size_t index) const;
    std::optional<integer> find_exponent(const expr* e) const;
    void propose_factor(uint32_t sum, const expr* lhs, const expr* rhs, std::vector<const expr*>& out);
    static bool holds(const std::optional<integer>& value, int expected);

    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    poly_symbols symbols_;
};

#define POLY_LOCAL_RULES_TEMPLATE template<typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
#define POLY_LOCAL_RULES          poly_local_rules<IFindLiteralValue, IMakeLiteral, IMakeFunctor>

POLY_LOCAL_RULES_TEMPLATE POLY_LOCAL_RULES::poly_local_rules(
    IFindLiteralValue& find_literal_value, IMakeLiteral& make_literal, IMakeFunctor& make_functor, poly_symbols symbols)
    : find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols) {}

POLY_LOCAL_RULES_TEMPLATE void POLY_LOCAL_RULES::propose(const expr* e, std::vector<const expr*>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.size() != 2)
        return;
    const uint32_t id = f->id;
    const expr* lhs = f->args[0];
    const expr* rhs = f->args[1];
    const bool add = id == symbols_.add;
    const bool sub = id == symbols_.sub;
    const bool mul = id == symbols_.mul;
    const bool pow = id == symbols_.pow;
    if (!add && !sub && !mul && !pow)
        return;
    const std::optional<integer> lhs_value = find_literal_value_.find_value(lhs);
    const std::optional<integer> rhs_value = find_literal_value_.find_value(rhs);

    // fold
    if (lhs_value.has_value() && rhs_value.has_value()) {
        const integer& l = lhs_value.value();
        const integer& r = rhs_value.value();
        // A power folds only for exponents the rest of the vocabulary accepts (0..64).
        const bool foldable = !pow || (r >= 0 && r <= 64);
        if (add) out.push_back(make_literal_.make_literal(integer(l + r)));
        if (sub) out.push_back(make_literal_.make_literal(integer(l - r)));
        if (mul) out.push_back(make_literal_.make_literal(integer(l * r)));
        if (pow && foldable) out.push_back(make_literal_.make_literal(integer(boost::multiprecision::pow(l, r.convert_to<uint32_t>()))));
        return;
    }

    // identity
    if ((add || sub) && holds(rhs_value, 0)) out.push_back(lhs);
    if (add && holds(lhs_value, 0)) out.push_back(rhs);
    if (sub && lhs == rhs) out.push_back(make_literal_.make_literal(0));
    if (mul && holds(rhs_value, 1)) out.push_back(lhs);
    if (mul && holds(lhs_value, 1)) out.push_back(rhs);
    if (mul && (holds(lhs_value, 0) || holds(rhs_value, 0))) out.push_back(make_literal_.make_literal(0));
    if (pow && holds(rhs_value, 1)) out.push_back(lhs);
    if (pow && holds(rhs_value, 0)) out.push_back(make_literal_.make_literal(1));

    // power
    const std::optional<integer> lhs_exponent = find_exponent(lhs);
    const std::optional<integer> rhs_exponent = find_exponent(rhs);
    const expr* lhs_base = find_operand(lhs, symbols_.pow, 0);
    const expr* rhs_base = find_operand(rhs, symbols_.pow, 0);
    if (mul && lhs == rhs)
        out.push_back(make(symbols_.pow, lhs, make_literal_.make_literal(2)));
    if (mul && lhs_base != nullptr && lhs_base == rhs && lhs_exponent.has_value())
        out.push_back(make(symbols_.pow, rhs, make_literal_.make_literal(integer(lhs_exponent.value() + 1))));
    if (mul && rhs_base != nullptr && rhs_base == lhs && rhs_exponent.has_value())
        out.push_back(make(symbols_.pow, lhs, make_literal_.make_literal(integer(rhs_exponent.value() + 1))));
    if (mul && lhs_base != nullptr && lhs_base == rhs_base && lhs_exponent.has_value() && rhs_exponent.has_value())
        out.push_back(make(symbols_.pow, lhs_base, make_literal_.make_literal(integer(lhs_exponent.value() + rhs_exponent.value()))));
    if (pow && lhs_base != nullptr && lhs_exponent.has_value() && rhs_value.has_value())
        out.push_back(make(symbols_.pow, lhs_base, make_literal_.make_literal(integer(lhs_exponent.value() * rhs_value.value()))));

    // like terms
    const std::optional<integer> lhs_scale = find_operand(lhs, symbols_.mul, 0) != nullptr
        ? find_literal_value_.find_value(find_operand(lhs, symbols_.mul, 0)) : std::nullopt;
    const std::optional<integer> rhs_scale = find_operand(rhs, symbols_.mul, 0) != nullptr
        ? find_literal_value_.find_value(find_operand(rhs, symbols_.mul, 0)) : std::nullopt;
    if (add && lhs == rhs)
        out.push_back(make(symbols_.mul, make_literal_.make_literal(2), lhs));
    if ((add || sub) && lhs_scale.has_value() && find_operand(lhs, symbols_.mul, 1) == rhs)
        out.push_back(make(symbols_.mul, make_literal_.make_literal(integer(lhs_scale.value() + (add ? 1 : -1))), rhs));
    if (add && rhs_scale.has_value() && find_operand(rhs, symbols_.mul, 1) == lhs)
        out.push_back(make(symbols_.mul, make_literal_.make_literal(integer(rhs_scale.value() + 1)), lhs));

    // factor
    if (add || sub)
        propose_factor(id, lhs, rhs, out);

    // distribute
    const bool rhs_sum = find_operand(rhs, symbols_.add, 0) != nullptr || find_operand(rhs, symbols_.sub, 0) != nullptr;
    const bool lhs_sum = find_operand(lhs, symbols_.add, 0) != nullptr || find_operand(lhs, symbols_.sub, 0) != nullptr;
    if (mul && rhs_sum) {
        const expr::functor& inner = std::get<expr::functor>(rhs->content);
        out.push_back(make(inner.id, make(symbols_.mul, lhs, inner.args[0]), make(symbols_.mul, lhs, inner.args[1])));
    }
    if (mul && lhs_sum) {
        const expr::functor& inner = std::get<expr::functor>(lhs->content);
        out.push_back(make(inner.id, make(symbols_.mul, inner.args[0], rhs), make(symbols_.mul, inner.args[1], rhs)));
    }

    // expand
    if (pow && rhs_value.has_value() && rhs_value.value() >= 2) {
        const expr* rest = rhs_value.value() == 2 ? lhs : make(symbols_.pow, lhs, make_literal_.make_literal(integer(rhs_value.value() - 1)));
        out.push_back(make(symbols_.mul, lhs, rest));
    }

    // commute
    if (add || mul)
        out.push_back(make(id, rhs, lhs));

    // associate
    for (const uint32_t op : {symbols_.add, symbols_.mul}) {
        if (id != op) continue;
        if (find_operand(lhs, op, 0) != nullptr)
            out.push_back(make(op, find_operand(lhs, op, 0), make(op, find_operand(lhs, op, 1), rhs)));
        if (find_operand(rhs, op, 0) != nullptr)
            out.push_back(make(op, make(op, lhs, find_operand(rhs, op, 0)), find_operand(rhs, op, 1)));
    }
    if (add && find_operand(rhs, symbols_.sub, 0) != nullptr)
        out.push_back(make(symbols_.sub, make(symbols_.add, lhs, find_operand(rhs, symbols_.sub, 0)), find_operand(rhs, symbols_.sub, 1)));
    if (sub && find_operand(lhs, symbols_.add, 0) != nullptr)
        out.push_back(make(symbols_.add, find_operand(lhs, symbols_.add, 0), make(symbols_.sub, find_operand(lhs, symbols_.add, 1), rhs)));
    if (sub && find_operand(rhs, symbols_.add, 0) != nullptr)
        out.push_back(make(symbols_.sub, make(symbols_.sub, lhs, find_operand(rhs, symbols_.add, 0)), find_operand(rhs, symbols_.add, 1)));
    if (sub && find_operand(lhs, symbols_.sub, 0) != nullptr)
        out.push_back(make(symbols_.sub, find_operand(lhs, symbols_.sub, 0), make(symbols_.add, find_operand(lhs, symbols_.sub, 1), rhs)));
    if (sub && find_operand(rhs, symbols_.sub, 0) != nullptr)
        out.push_back(make(symbols_.add, make(symbols_.sub, lhs, find_operand(rhs, symbols_.sub, 0)), find_operand(rhs, symbols_.sub, 1)));
}

POLY_LOCAL_RULES_TEMPLATE const expr* POLY_LOCAL_RULES::make(uint32_t id, const expr* lhs, const expr* rhs) {
    return make_functor_.make_functor(id, {lhs, rhs});
}

// The operand at `index` when e is a binary `id` node, else null.
POLY_LOCAL_RULES_TEMPLATE const expr* POLY_LOCAL_RULES::find_operand(const expr* e, uint32_t id, size_t index) const {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->id != id || f->args.size() != 2)
        return nullptr;
    return f->args[index];
}

POLY_LOCAL_RULES_TEMPLATE std::optional<integer> POLY_LOCAL_RULES::find_exponent(const expr* e) const {
    const expr* exponent = find_operand(e, symbols_.pow, 1);
    if (exponent == nullptr)
        return std::nullopt;
    return find_literal_value_.find_value(exponent);
}

POLY_LOCAL_RULES_TEMPLATE bool POLY_LOCAL_RULES::holds(const std::optional<integer>& value, int expected) {
    return value.has_value() && value.value() == expected;
}

POLY_LOCAL_RULES_TEMPLATE void POLY_LOCAL_RULES::propose_factor(
    uint32_t sum, const expr* lhs, const expr* rhs, std::vector<const expr*>& out) {
    const expr* a0 = find_operand(lhs, symbols_.mul, 0);
    const expr* a1 = find_operand(lhs, symbols_.mul, 1);
    const expr* b0 = find_operand(rhs, symbols_.mul, 0);
    const expr* b1 = find_operand(rhs, symbols_.mul, 1);
    if (a0 == nullptr || b0 == nullptr)
        return;
    if (a0 == b0) out.push_back(make(symbols_.mul, a0, make(sum, a1, b1)));
    if (a1 == b1) out.push_back(make(symbols_.mul, make(sum, a0, b0), a1));
    if (a0 == b1) out.push_back(make(symbols_.mul, a0, make(sum, a1, b0)));
    if (a1 == b0) out.push_back(make(symbols_.mul, a1, make(sum, a0, b1)));
}

#undef POLY_LOCAL_RULES
#undef POLY_LOCAL_RULES_TEMPLATE

#endif
