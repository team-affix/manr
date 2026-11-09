#ifndef ABS_BINDER_MERGER_HPP
#define ABS_BINDER_MERGER_HPP

#include <cstddef>
#include <variant>
#include <vector>
#include "value_objects/abs_symbols.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/rewrite.hpp"

// Local rule: merges a chain of binders into one, `lam(lam(lam(b)))` into `lams(3, b)` and `lam(lams(2, b))` into
// `lams(3, b)`, when that is smaller: a `lam` costs one node, a `lams` two (itself and its count), so it fires on three
// `lam`s or more, or on any chain holding a `lams` and another binder. The meaning is unchanged: `lams(k, b)` is k
// nested lambdas.
template<typename ICountBinders, typename IMakeLiteral, typename IMakeFunctor>
struct abs_binder_merger {
    abs_binder_merger(ICountBinders& count_binders,
                      IMakeLiteral& make_literal,
                      IMakeFunctor& make_functor,
                      abs_symbols symbols);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    ICountBinders& count_binders_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    abs_symbols symbols_;
};

#define ABS_BINDER_MERGER_TEMPLATE template<typename ICountBinders, typename IMakeLiteral, typename IMakeFunctor>
#define ABS_BINDER_MERGER          abs_binder_merger<ICountBinders, IMakeLiteral, IMakeFunctor>

ABS_BINDER_MERGER_TEMPLATE ABS_BINDER_MERGER::abs_binder_merger(ICountBinders& count_binders,
                                                                 IMakeLiteral& make_literal,
                                                                 IMakeFunctor& make_functor,
                                                                 abs_symbols symbols)
    : count_binders_(count_binders)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols) {}

ABS_BINDER_MERGER_TEMPLATE void ABS_BINDER_MERGER::detect(const expr* e, std::vector<rewrite>& out) {
    size_t binders = 0;
    size_t cost = 0;
    size_t nodes = 0;
    const expr* body = e;
    for (size_t count = count_binders_.count_binders(body); count > 0; count = count_binders_.count_binders(body)) {
        const expr::functor& f = std::get<expr::functor>(body->content);
        binders += count;
        cost += f.id == symbols_.lambda ? 1 : 2;
        ++nodes;
        body = f.args.back();
    }
    const bool smaller = nodes >= 2 && cost > 2;
    if (!smaller)
        return;
    out.push_back([this, binders, body]() {
        return make_functor_.make_functor(symbols_.lambdas, {make_literal_.make_literal(integer(binders)), body});
    });
}

#undef ABS_BINDER_MERGER
#undef ABS_BINDER_MERGER_TEMPLATE

#endif
