#ifndef LIST_VALUES_HPP
#define LIST_VALUES_HPP

#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/list_symbols.hpp"

// Interns list values as `cons(e1, cons(e2, ... nil))` and reads them back. Any expression that is not such a chain
// has no elements.
template<typename IMakeFunctor>
struct list_values {
    list_values(IMakeFunctor& make_functor, list_symbols symbols);
    const expr* make_list(const std::vector<const expr*>& elements);
    std::optional<std::vector<const expr*>> find_elements(const expr* e) const;
private:
    IMakeFunctor& make_functor_;
    list_symbols symbols_;
};

#define LIST_VALUES_TEMPLATE template<typename IMakeFunctor>
#define LIST_VALUES          list_values<IMakeFunctor>

LIST_VALUES_TEMPLATE LIST_VALUES::list_values(IMakeFunctor& make_functor, list_symbols symbols)
    : make_functor_(make_functor)
    , symbols_(symbols) {}

LIST_VALUES_TEMPLATE const expr* LIST_VALUES::make_list(const std::vector<const expr*>& elements) {
    const expr* list = make_functor_.make_functor(symbols_.nil, {});
    for (auto element = elements.rbegin(); element != elements.rend(); ++element)
        list = make_functor_.make_functor(symbols_.cons, {*element, list});
    return list;
}

LIST_VALUES_TEMPLATE std::optional<std::vector<const expr*>> LIST_VALUES::find_elements(const expr* e) const {
    std::vector<const expr*> elements;
    const expr* rest = e;
    while (true) {
        const expr::functor* f = std::get_if<expr::functor>(&rest->content);
        if (f == nullptr)
            return std::nullopt;
        const bool nil = f->id == symbols_.nil && f->args.empty();
        if (nil)
            return elements;
        const bool cons = f->id == symbols_.cons && f->args.size() == 2;
        if (!cons)
            return std::nullopt;
        elements.push_back(f->args[0]);
        rest = f->args[1];
    }
}

#undef LIST_VALUES
#undef LIST_VALUES_TEMPLATE

#endif
