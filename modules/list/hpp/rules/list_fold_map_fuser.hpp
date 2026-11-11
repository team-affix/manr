#ifndef LIST_FOLD_MAP_FUSER_HPP
#define LIST_FOLD_MAP_FUSER_HPP

#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/rewrite.hpp"

// Local rule: fuses a fold over a map, `fold(lam(lam(s)), b, map(lam(f), xs))` to `fold(lam(lam(s')), b, xs)`, where
// s' is s with f for its head `#0`. f is moved from under one binder to under the step's two, so its indices past its
// own parameter move out by one (`shift`), then substituted capture-free (`substitute`). Fires only when smaller (a
// step that ignores its head drops the map whole: the length of a map is the length).
template<typename IShiftIndices, typename ISubstituteIndex, typename IFindCost, typename IMakeFunctor>
struct list_fold_map_fuser {
    list_fold_map_fuser(IShiftIndices& shift_indices,
                        ISubstituteIndex& substitute_index,
                        IFindCost& find_cost,
                        IMakeFunctor& make_functor,
                        list_symbols symbols);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    const expr* find_body(const expr* function, int binders) const;

    IShiftIndices& shift_indices_;
    ISubstituteIndex& substitute_index_;
    IFindCost& find_cost_;
    IMakeFunctor& make_functor_;
    list_symbols symbols_;
};

#define LIST_FOLD_MAP_FUSER_TEMPLATE \
    template<typename IShiftIndices, typename ISubstituteIndex, typename IFindCost, typename IMakeFunctor>
#define LIST_FOLD_MAP_FUSER list_fold_map_fuser<IShiftIndices, ISubstituteIndex, IFindCost, IMakeFunctor>

LIST_FOLD_MAP_FUSER_TEMPLATE LIST_FOLD_MAP_FUSER::list_fold_map_fuser(IShiftIndices& shift_indices,
                                                                      ISubstituteIndex& substitute_index,
                                                                      IFindCost& find_cost,
                                                                      IMakeFunctor& make_functor,
                                                                      list_symbols symbols)
    : shift_indices_(shift_indices)
    , substitute_index_(substitute_index)
    , find_cost_(find_cost)
    , make_functor_(make_functor)
    , symbols_(symbols) {}

LIST_FOLD_MAP_FUSER_TEMPLATE void LIST_FOLD_MAP_FUSER::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* fold = std::get_if<expr::functor>(&e->content);
    if (fold == nullptr || fold->id != symbols_.fold || fold->args.size() != 3)
        return;
    const expr::functor* map = std::get_if<expr::functor>(&fold->args[2]->content);
    if (map == nullptr || map->id != symbols_.map || map->args.size() != 2)
        return;
    const expr* step = find_body(fold->args[0], 2);
    const expr* element = find_body(map->args[0], 1);
    if (step == nullptr || element == nullptr)
        return;
    const expr* moved = shift_indices_.shift(element, 1, 1);
    const expr* fused_step = substitute_index_.substitute(step, 0, moved);
    const expr* function = make_functor_.make_functor(
        symbols_.lambda, {make_functor_.make_functor(symbols_.lambda, {fused_step})});
    const expr* fused = make_functor_.make_functor(symbols_.fold, {function, fold->args[1], map->args[1]});
    if (find_cost_.cost(fused) >= find_cost_.cost(e))
        return;
    out.push_back(ready(fused));
}

// the body under exactly `binders` nested `lam`s, or nullptr
LIST_FOLD_MAP_FUSER_TEMPLATE const expr* LIST_FOLD_MAP_FUSER::find_body(const expr* function, int binders) const {
    const expr* body = function;
    for (int binder = 0; binder < binders; ++binder) {
        const expr::functor* f = std::get_if<expr::functor>(&body->content);
        if (f == nullptr || f->id != symbols_.lambda || f->args.size() != 1)
            return nullptr;
        body = f->args[0];
    }
    return body;
}

#undef LIST_FOLD_MAP_FUSER
#undef LIST_FOLD_MAP_FUSER_TEMPLATE

#endif
