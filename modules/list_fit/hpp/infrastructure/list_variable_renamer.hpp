#ifndef LIST_VARIABLE_RENAMER_HPP
#define LIST_VARIABLE_RENAMER_HPP

#include <cstdint>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"

// Renames a body's variable: every nullary functor `from` becomes `to`. A body fitted in the element variable `e`
// (by the element domain) becomes a function of the head `h` this way, for use inside a fold step.
template<typename IMakeFunctor>
struct list_variable_renamer {
    list_variable_renamer(IMakeFunctor& make_functor);
    const expr* rename(const expr* body, uint32_t from, uint32_t to);
private:
    IMakeFunctor& make_functor_;
};

#define LIST_VARIABLE_RENAMER_TEMPLATE template<typename IMakeFunctor>
#define LIST_VARIABLE_RENAMER list_variable_renamer<IMakeFunctor>

LIST_VARIABLE_RENAMER_TEMPLATE LIST_VARIABLE_RENAMER::list_variable_renamer(IMakeFunctor& make_functor)
    : make_functor_(make_functor) {}

LIST_VARIABLE_RENAMER_TEMPLATE const expr* LIST_VARIABLE_RENAMER::rename(const expr* body, uint32_t from, uint32_t to) {
    const expr::functor* functor = std::get_if<expr::functor>(&body->content);
    if (functor == nullptr)
        return body;
    const bool renamed = functor->id == from && functor->args.empty();
    if (renamed)
        return make_functor_.make_functor(to, {});
    std::vector<const expr*> args;
    args.reserve(functor->args.size());
    for (const expr* arg : functor->args)
        args.push_back(rename(arg, from, to));
    return make_functor_.make_functor(functor->id, args);
}

#undef LIST_VARIABLE_RENAMER
#undef LIST_VARIABLE_RENAMER_TEMPLATE

#endif
