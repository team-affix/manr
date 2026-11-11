#ifndef LIST_NIL_BINDER_HPP
#define LIST_NIL_BINDER_HPP

#include <cstdint>
#include <map>
#include <set>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/rewrite.hpp"

// Root rule (it binds holes: apply it through `hole_scope_proposer`): binds a hole that stands where a list goes (an
// argument of `append`, the tail of a `cons`, the base of a fold that rebuilds a list) to the empty list `nil`, the
// list that leaves the rest unchanged, so the laws of lists can then fire: `fold(lam(lam(cons(#0, #1))), ?0, xs)`
// with `?0 = nil` is `xs` (#73), and a snoc step `append(#1, cons(#0, ?0))` with `?0 = nil` folds to `reverse` (#72).
// Proposes one binding per such hole, at the same size (`?0` and `nil` are one node each).
template<typename IMakeFunctor>
struct list_nil_binder {
    list_nil_binder(IMakeFunctor& make_functor, list_symbols symbols);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    void collect_list_holes(const expr* e, std::set<uint32_t>& holes) const;
    const expr* substitute(const expr* e, uint32_t hole, const expr* value);

    IMakeFunctor& make_functor_;
    list_symbols symbols_;
};

#define LIST_NIL_BINDER_TEMPLATE template<typename IMakeFunctor>
#define LIST_NIL_BINDER          list_nil_binder<IMakeFunctor>

LIST_NIL_BINDER_TEMPLATE LIST_NIL_BINDER::list_nil_binder(IMakeFunctor& make_functor, list_symbols symbols)
    : make_functor_(make_functor)
    , symbols_(symbols) {}

LIST_NIL_BINDER_TEMPLATE void LIST_NIL_BINDER::detect(const expr* e, std::vector<rewrite>& out) {
    std::set<uint32_t> holes;
    collect_list_holes(e, holes);
    for (const uint32_t hole : holes) {
        out.push_back([this, e, hole]() {
            return substitute(e, hole, make_functor_.make_functor(symbols_.nil, {}));
        });
    }
}

// the holes standing directly where a list goes: in `append`, as a `cons` tail, or as a fold's base
LIST_NIL_BINDER_TEMPLATE void LIST_NIL_BINDER::collect_list_holes(const expr* e, std::set<uint32_t>& holes) const {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr)
        return;
    std::vector<size_t> positions;
    if (f->id == symbols_.append && f->args.size() == 2)
        positions = {0, 1};
    if (f->id == symbols_.cons && f->args.size() == 2)
        positions = {1};
    if (f->id == symbols_.fold && f->args.size() == 3)
        positions = {1};
    for (const size_t position : positions)
        if (const expr::var* hole = std::get_if<expr::var>(&f->args[position]->content))
            holes.insert(hole->index);
    for (const expr* arg : f->args)
        collect_list_holes(arg, holes);
}

LIST_NIL_BINDER_TEMPLATE const expr* LIST_NIL_BINDER::substitute(const expr* e, uint32_t hole, const expr* value) {
    if (const expr::var* var = std::get_if<expr::var>(&e->content))
        return var->index == hole ? value : e;
    const expr::functor& f = std::get<expr::functor>(e->content);
    if (f.args.empty())
        return e;
    std::vector<const expr*> args;
    args.reserve(f.args.size());
    for (const expr* arg : f.args)
        args.push_back(substitute(arg, hole, value));
    return make_functor_.make_functor(f.id, args);
}

#undef LIST_NIL_BINDER
#undef LIST_NIL_BINDER_TEMPLATE

#endif
