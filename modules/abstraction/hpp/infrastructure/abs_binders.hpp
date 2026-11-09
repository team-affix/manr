#ifndef ABS_BINDERS_HPP
#define ABS_BINDERS_HPP

#include <cstddef>
#include <optional>
#include <variant>
#include "value_objects/abs_symbols.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"

// Reads a function's binders: `lam(b)` binds one, `lams(k, b)` binds k. `find_body` returns the body under exactly
// `count` binders, however they are written (`lam(lam(b))` or `lams(2, b)`), and `count_binders` how many a binder
// node itself introduces, for walking scopes.
template<typename IFindLiteralValue>
struct abs_binders {
    abs_binders(IFindLiteralValue& find_literal_value, abs_symbols symbols);
    // Nothing when the function is not exactly that many binders deep before its body begins.
    std::optional<const expr*> find_body(const expr* function, size_t count) const;
    // 1 for `lam`, k for `lams(k, b)`, 0 for anything else.
    size_t count_binders(const expr* e) const;
private:
    IFindLiteralValue& find_literal_value_;
    abs_symbols symbols_;
};

#define ABS_BINDERS_TEMPLATE template<typename IFindLiteralValue>
#define ABS_BINDERS          abs_binders<IFindLiteralValue>

ABS_BINDERS_TEMPLATE ABS_BINDERS::abs_binders(IFindLiteralValue& find_literal_value, abs_symbols symbols)
    : find_literal_value_(find_literal_value)
    , symbols_(symbols) {}

ABS_BINDERS_TEMPLATE std::optional<const expr*> ABS_BINDERS::find_body(const expr* function, size_t count) const {
    const expr* body = function;
    size_t remaining = count;
    while (remaining > 0) {
        const size_t binders = count_binders(body);
        if (binders == 0 || binders > remaining)
            return std::nullopt;
        remaining -= binders;
        body = std::get<expr::functor>(body->content).args.back();
    }
    return body;
}

ABS_BINDERS_TEMPLATE size_t ABS_BINDERS::count_binders(const expr* e) const {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr)
        return 0;
    if (f->id == symbols_.lambda && f->args.size() == 1)
        return 1;
    if (f->id != symbols_.lambdas || f->args.size() != 2)
        return 0;
    const std::optional<integer> count = find_literal_value_.find_value(f->args[0]);
    const bool valid = count.has_value() && count.value() >= 1 && count.value() <= 1024;
    return valid ? count.value().template convert_to<size_t>() : 0;
}

#undef ABS_BINDERS
#undef ABS_BINDERS_TEMPLATE

#endif
