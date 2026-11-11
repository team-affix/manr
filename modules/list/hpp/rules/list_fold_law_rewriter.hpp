#ifndef LIST_FOLD_LAW_REWRITER_HPP
#define LIST_FOLD_LAW_REWRITER_HPP

#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/rewrite.hpp"

// Local rule: the laws of folds that rebuild a list, and of appending the empty list, each strictly smaller:
//
// - `fold(lam(lam(cons(#0, #1))), b, xs)` is `append(xs, b)` (folding with cons copies the list onto the base);
// - `fold(lam(lam(append(#1, cons(#0, nil)))), b, xs)` is `append(reverse(xs), b)` (appending each head at the end
//   reverses);
// - `append(xs, nil)` and `append(nil, xs)` are `xs`.
//
// So the long forms of the identity and of reverse reduce to `xs` and `reverse(xs)`.
template<typename IMakeFunctor>
struct list_fold_law_rewriter {
    list_fold_law_rewriter(IMakeFunctor& make_functor, list_symbols symbols);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    const expr* find_step(const expr* function) const;
    bool is_symbol(const expr* e, uint32_t symbol) const;

    IMakeFunctor& make_functor_;
    list_symbols symbols_;
};

#define LIST_FOLD_LAW_REWRITER_TEMPLATE template<typename IMakeFunctor>
#define LIST_FOLD_LAW_REWRITER          list_fold_law_rewriter<IMakeFunctor>

LIST_FOLD_LAW_REWRITER_TEMPLATE LIST_FOLD_LAW_REWRITER::list_fold_law_rewriter(IMakeFunctor& make_functor,
                                                                               list_symbols symbols)
    : make_functor_(make_functor)
    , symbols_(symbols) {}

LIST_FOLD_LAW_REWRITER_TEMPLATE void LIST_FOLD_LAW_REWRITER::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr)
        return;
    if (f->id == symbols_.append && f->args.size() == 2) {
        if (is_symbol(f->args[1], symbols_.nil))
            out.push_back(ready(f->args[0]));
        if (is_symbol(f->args[0], symbols_.nil))
            out.push_back(ready(f->args[1]));
        return;
    }
    if (f->id != symbols_.fold || f->args.size() != 3)
        return;
    const expr* step = find_step(f->args[0]);
    const expr::functor* body = step == nullptr ? nullptr : std::get_if<expr::functor>(&step->content);
    if (body == nullptr || body->args.size() != 2)
        return;
    const expr* base = f->args[1];
    const expr* list = f->args[2];
    const bool copying = body->id == symbols_.cons && is_symbol(body->args[0], symbols_.head)
                         && is_symbol(body->args[1], symbols_.accumulator);
    if (copying) {
        out.push_back([this, list, base]() { return make_functor_.make_functor(symbols_.append, {list, base}); });
        return;
    }
    const expr::functor* last = std::get_if<expr::functor>(&body->args[1]->content);
    const bool snoc = body->id == symbols_.append && is_symbol(body->args[0], symbols_.accumulator) && last != nullptr
                      && last->id == symbols_.cons && last->args.size() == 2 && is_symbol(last->args[0], symbols_.head)
                      && is_symbol(last->args[1], symbols_.nil);
    if (!snoc)
        return;
    out.push_back([this, list, base]() {
        return make_functor_.make_functor(symbols_.append,
                                          {make_functor_.make_functor(symbols_.reverse, {list}), base});
    });
}

// the step body of `lam(lam(s))`, or nullptr
LIST_FOLD_LAW_REWRITER_TEMPLATE const expr* LIST_FOLD_LAW_REWRITER::find_step(const expr* function) const {
    const expr* body = function;
    for (int binder = 0; binder < 2; ++binder) {
        const expr::functor* f = std::get_if<expr::functor>(&body->content);
        if (f == nullptr || f->id != symbols_.lambda || f->args.size() != 1)
            return nullptr;
        body = f->args[0];
    }
    return body;
}

LIST_FOLD_LAW_REWRITER_TEMPLATE bool LIST_FOLD_LAW_REWRITER::is_symbol(const expr* e, uint32_t symbol) const {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    return f != nullptr && f->id == symbol && f->args.empty();
}

#undef LIST_FOLD_LAW_REWRITER
#undef LIST_FOLD_LAW_REWRITER_TEMPLATE

#endif
