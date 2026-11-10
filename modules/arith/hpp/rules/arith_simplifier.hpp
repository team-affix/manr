#ifndef ARITH_SIMPLIFIER_HPP
#define ARITH_SIMPLIFIER_HPP

#include <optional>
#include <variant>
#include <vector>
#include "value_objects/arith_symbols.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/rewrite.hpp"

// Local rules of integer arithmetic: constant folding of a binary node over two literals, and the identities
// `e + 0`, `0 + e`, `e - 0`, `e * 1`, `1 * e` -> `e`, and `e * 0`, `0 * e`, `e - e` -> `0`.
template<typename IFindLiteralValue, typename IMakeLiteral>
struct arith_simplifier {
    arith_simplifier(IFindLiteralValue& find_literal_value, IMakeLiteral& make_literal, arith_symbols symbols);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    arith_symbols symbols_;
};

#define ARITH_SIMPLIFIER_TEMPLATE template<typename IFindLiteralValue, typename IMakeLiteral>
#define ARITH_SIMPLIFIER          arith_simplifier<IFindLiteralValue, IMakeLiteral>

ARITH_SIMPLIFIER_TEMPLATE ARITH_SIMPLIFIER::arith_simplifier(
    IFindLiteralValue& find_literal_value, IMakeLiteral& make_literal, arith_symbols symbols)
    : find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , symbols_(symbols) {}

ARITH_SIMPLIFIER_TEMPLATE void ARITH_SIMPLIFIER::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.size() != 2)
        return;
    const bool add = f->id == symbols_.add;
    const bool sub = f->id == symbols_.sub;
    const bool mul = f->id == symbols_.mul;
    if (!add && !sub && !mul)
        return;
    const expr* lhs = f->args[0];
    const expr* rhs = f->args[1];
    const std::optional<integer> lhs_value = find_literal_value_.find_value(lhs);
    const std::optional<integer> rhs_value = find_literal_value_.find_value(rhs);
    const bool lhs_zero = lhs_value.has_value() && lhs_value.value() == 0;
    const bool rhs_zero = rhs_value.has_value() && rhs_value.value() == 0;
    const bool lhs_one = lhs_value.has_value() && lhs_value.value() == 1;
    const bool rhs_one = rhs_value.has_value() && rhs_value.value() == 1;
    if (lhs_value.has_value() && rhs_value.has_value()) {
        if (add) out.push_back(ready(make_literal_.make_literal(integer(lhs_value.value() + rhs_value.value()))));
        if (sub) out.push_back(ready(make_literal_.make_literal(integer(lhs_value.value() - rhs_value.value()))));
        if (mul) out.push_back(ready(make_literal_.make_literal(integer(lhs_value.value() * rhs_value.value()))));
        return;
    }
    if (add && rhs_zero) out.push_back(ready(lhs));
    if (add && lhs_zero) out.push_back(ready(rhs));
    if (sub && rhs_zero) out.push_back(ready(lhs));
    if (sub && lhs == rhs) out.push_back(ready(make_literal_.make_literal(0)));
    if (mul && rhs_one) out.push_back(ready(lhs));
    if (mul && lhs_one) out.push_back(ready(rhs));
    if (mul && (lhs_zero || rhs_zero)) out.push_back(ready(make_literal_.make_literal(0)));
}

#undef ARITH_SIMPLIFIER
#undef ARITH_SIMPLIFIER_TEMPLATE

#endif
