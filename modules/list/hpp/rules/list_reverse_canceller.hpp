#ifndef LIST_REVERSE_CANCELLER_HPP
#define LIST_REVERSE_CANCELLER_HPP

#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/rewrite.hpp"

// Local rule: cancels a reverse against a reverse below it, through element-wise steps that keep order and so commute
// with it: `reverse(reverse(xs))` to `xs`, and `reverse(map(f, filter(p, reverse(xs))))` to `map(f, filter(p, xs))`.
template<typename IMakeFunctor>
struct list_reverse_canceller {
    list_reverse_canceller(IMakeFunctor& make_functor, list_symbols symbols);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    IMakeFunctor& make_functor_;
    list_symbols symbols_;
};

#define LIST_REVERSE_CANCELLER_TEMPLATE template<typename IMakeFunctor>
#define LIST_REVERSE_CANCELLER          list_reverse_canceller<IMakeFunctor>

LIST_REVERSE_CANCELLER_TEMPLATE LIST_REVERSE_CANCELLER::list_reverse_canceller(IMakeFunctor& make_functor,
                                                                               list_symbols symbols)
    : make_functor_(make_functor)
    , symbols_(symbols) {}

LIST_REVERSE_CANCELLER_TEMPLATE void LIST_REVERSE_CANCELLER::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* top = std::get_if<expr::functor>(&e->content);
    if (top == nullptr || top->id != symbols_.reverse || top->args.size() != 1)
        return;
    // the element-wise steps under the outer reverse, outermost first
    std::vector<const expr::functor*> steps;
    const expr::functor* below = std::get_if<expr::functor>(&top->args[0]->content);
    while (below != nullptr && (below->id == symbols_.map || below->id == symbols_.filter) && below->args.size() == 2) {
        steps.push_back(below);
        below = std::get_if<expr::functor>(&below->args[1]->content);
    }
    const bool reversed = below != nullptr && below->id == symbols_.reverse && below->args.size() == 1;
    if (!reversed)
        return;
    out.push_back([this, steps, list = below->args[0]]() {
        const expr* rebuilt = list;
        for (auto step = steps.rbegin(); step != steps.rend(); ++step)
            rebuilt = make_functor_.make_functor((*step)->id, {(*step)->args[0], rebuilt});
        return rebuilt;
    });
}

#undef LIST_REVERSE_CANCELLER
#undef LIST_REVERSE_CANCELLER_TEMPLATE

#endif
