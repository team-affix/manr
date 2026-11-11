#ifndef LIST_MAP_FUSER_HPP
#define LIST_MAP_FUSER_HPP

#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/rewrite.hpp"

// Local rule: fuses a map over a map, `map(lam(f), map(lam(g), xs))` to `map(lam(f'), xs)`, where f' is f with g for
// its element `#0` (capture-free: g is shifted under the binders inside f, and the fused lambda sits where f's did, so
// no other index moves). Fires only when the fused map is smaller, which it is whenever f uses its element at most
// once; the polynomial rules then simplify the fused body.
template<typename ISubstituteIndex, typename IFindCost, typename IMakeFunctor>
struct list_map_fuser {
    list_map_fuser(ISubstituteIndex& substitute_index,
                   IFindCost& find_cost,
                   IMakeFunctor& make_functor,
                   list_symbols symbols);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    const expr* find_body(const expr* function) const;

    ISubstituteIndex& substitute_index_;
    IFindCost& find_cost_;
    IMakeFunctor& make_functor_;
    list_symbols symbols_;
};

#define LIST_MAP_FUSER_TEMPLATE template<typename ISubstituteIndex, typename IFindCost, typename IMakeFunctor>
#define LIST_MAP_FUSER          list_map_fuser<ISubstituteIndex, IFindCost, IMakeFunctor>

LIST_MAP_FUSER_TEMPLATE LIST_MAP_FUSER::list_map_fuser(ISubstituteIndex& substitute_index,
                                                       IFindCost& find_cost,
                                                       IMakeFunctor& make_functor,
                                                       list_symbols symbols)
    : substitute_index_(substitute_index)
    , find_cost_(find_cost)
    , make_functor_(make_functor)
    , symbols_(symbols) {}

LIST_MAP_FUSER_TEMPLATE void LIST_MAP_FUSER::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* outer = std::get_if<expr::functor>(&e->content);
    if (outer == nullptr || outer->id != symbols_.map || outer->args.size() != 2)
        return;
    const expr::functor* inner = std::get_if<expr::functor>(&outer->args[1]->content);
    if (inner == nullptr || inner->id != symbols_.map || inner->args.size() != 2)
        return;
    const expr* outer_body = find_body(outer->args[0]);
    const expr* inner_body = find_body(inner->args[0]);
    if (outer_body == nullptr || inner_body == nullptr)
        return;
    const expr* fused_body = substitute_index_.substitute(outer_body, 0, inner_body);
    const expr* fused = make_functor_.make_functor(
        symbols_.map, {make_functor_.make_functor(symbols_.lambda, {fused_body}), inner->args[1]});
    if (find_cost_.cost(fused) >= find_cost_.cost(e))
        return;
    out.push_back(ready(fused));
}

// The body of a one-binder lambda, or nullptr.
LIST_MAP_FUSER_TEMPLATE const expr* LIST_MAP_FUSER::find_body(const expr* function) const {
    const expr::functor* f = std::get_if<expr::functor>(&function->content);
    const bool lambda = f != nullptr && f->id == symbols_.lambda && f->args.size() == 1;
    return lambda ? f->args[0] : nullptr;
}

#undef LIST_MAP_FUSER
#undef LIST_MAP_FUSER_TEMPLATE

#endif
