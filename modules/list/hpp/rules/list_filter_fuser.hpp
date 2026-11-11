#ifndef LIST_FILTER_FUSER_HPP
#define LIST_FILTER_FUSER_HPP

#include <cstdint>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/rewrite.hpp"

// Local rule: fuses a filter over a filter, `filter(lam(p), filter(lam(q), xs))` to `filter(lam(and(q, p)), xs)`, or to
// `filter(lam(p), xs)` when the conditions are the same. Both conditions read the element as `#0` under one binder,
// so the bodies combine unchanged. Always smaller: one filter and one lambda become one `and`.
template<typename IMakeFunctor>
struct list_filter_fuser {
    list_filter_fuser(IMakeFunctor& make_functor, list_symbols symbols, uint32_t conjunction_symbol);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    const expr* find_body(const expr* function) const;

    IMakeFunctor& make_functor_;
    list_symbols symbols_;
    uint32_t conjunction_symbol_;
};

#define LIST_FILTER_FUSER_TEMPLATE template<typename IMakeFunctor>
#define LIST_FILTER_FUSER          list_filter_fuser<IMakeFunctor>

LIST_FILTER_FUSER_TEMPLATE LIST_FILTER_FUSER::list_filter_fuser(IMakeFunctor& make_functor,
                                                                list_symbols symbols,
                                                                uint32_t conjunction_symbol)
    : make_functor_(make_functor)
    , symbols_(symbols)
    , conjunction_symbol_(conjunction_symbol) {}

LIST_FILTER_FUSER_TEMPLATE void LIST_FILTER_FUSER::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* outer = std::get_if<expr::functor>(&e->content);
    if (outer == nullptr || outer->id != symbols_.filter || outer->args.size() != 2)
        return;
    const expr::functor* inner = std::get_if<expr::functor>(&outer->args[1]->content);
    if (inner == nullptr || inner->id != symbols_.filter || inner->args.size() != 2)
        return;
    const expr* outer_body = find_body(outer->args[0]);
    const expr* inner_body = find_body(inner->args[0]);
    if (outer_body == nullptr || inner_body == nullptr)
        return;
    out.push_back([this, outer_body, inner_body, list = inner->args[1]]() {
        const expr* body = outer_body == inner_body
                               ? outer_body
                               : make_functor_.make_functor(conjunction_symbol_, {inner_body, outer_body});
        return make_functor_.make_functor(symbols_.filter,
                                          {make_functor_.make_functor(symbols_.lambda, {body}), list});
    });
}

// The body of a one-binder lambda, or nullptr.
LIST_FILTER_FUSER_TEMPLATE const expr* LIST_FILTER_FUSER::find_body(const expr* function) const {
    const expr::functor* f = std::get_if<expr::functor>(&function->content);
    const bool lambda = f != nullptr && f->id == symbols_.lambda && f->args.size() == 1;
    return lambda ? f->args[0] : nullptr;
}

#undef LIST_FILTER_FUSER
#undef LIST_FILTER_FUSER_TEMPLATE

#endif
