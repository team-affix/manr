#ifndef ABS_REDUCER_HPP
#define ABS_REDUCER_HPP

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>
#include "value_objects/abs_symbols.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"

constexpr size_t k_abs_default_max_reductions = 10000;

// Reduces an expression to one without helpers: every `let(v, b)` and every `app(lam(b), a)` is beta-reduced, b with
// its `#0` replaced by the argument (`app(lams(k, b), a)` takes the outermost of its k binders: `#(k-1)`, leaving
// `lams(k-1, ..)`),
// by capture-free de Bruijn substitution (an index that points past a binder is
// shifted as the term moves). Everything else is kept, and reduced inside, so the result is an ordinary expression of
// the domain, ready for its evaluator. An `app` whose function is not a lambda stays. Nothing when a reduction does not
// finish within `max_reductions` beta steps (recursion may not terminate). Results are memoized per expression.
template<typename IFindIndex, typename IMakeIndex, typename ICountBinders, typename IMakeLiteral, typename IMakeFunctor>
struct abs_reducer {
    abs_reducer(IFindIndex& find_index,
                IMakeIndex& make_index,
                ICountBinders& count_binders,
                IMakeLiteral& make_literal,
                IMakeFunctor& make_functor,
                abs_symbols symbols,
                size_t max_reductions = k_abs_default_max_reductions);
    std::optional<const expr*> reduce(const expr* e);
private:
    std::optional<const expr*> reduce_within(const expr* e, size_t& budget);
    std::optional<const expr*> apply(const expr* body, const expr* argument, size_t& budget);
    const expr* shift(const expr* e, int amount, size_t cutoff);
    const expr* substitute(const expr* e, size_t index, const expr* value);
    size_t count_bound(const expr::functor& f, const expr* e, size_t position) const;

    IFindIndex& find_index_;
    IMakeIndex& make_index_;
    ICountBinders& count_binders_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    abs_symbols symbols_;
    size_t max_reductions_;
    std::unordered_map<const expr*, const expr*> reduced_;
};

#define ABS_REDUCER_TEMPLATE                                                                         \
    template<typename IFindIndex, typename IMakeIndex, typename ICountBinders, typename IMakeLiteral, \
             typename IMakeFunctor>
#define ABS_REDUCER abs_reducer<IFindIndex, IMakeIndex, ICountBinders, IMakeLiteral, IMakeFunctor>

ABS_REDUCER_TEMPLATE ABS_REDUCER::abs_reducer(IFindIndex& find_index,
                                               IMakeIndex& make_index,
                                               ICountBinders& count_binders,
                                               IMakeLiteral& make_literal,
                                               IMakeFunctor& make_functor,
                                               abs_symbols symbols,
                                               size_t max_reductions)
    : find_index_(find_index)
    , make_index_(make_index)
    , count_binders_(count_binders)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , max_reductions_(max_reductions)
    , reduced_() {}

ABS_REDUCER_TEMPLATE std::optional<const expr*> ABS_REDUCER::reduce(const expr* e) {
    const auto known = reduced_.find(e);
    if (known != reduced_.end())
        return known->second;
    size_t budget = max_reductions_;
    const std::optional<const expr*> result = reduce_within(e, budget);
    if (result.has_value())
        reduced_.emplace(e, result.value());
    return result;
}

ABS_REDUCER_TEMPLATE std::optional<const expr*> ABS_REDUCER::reduce_within(const expr* e, size_t& budget) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.empty())
        return e;
    const bool binding = f->id == symbols_.binding && f->args.size() == 2;
    if (binding)
        return apply(f->args[1], f->args[0], budget);
    std::vector<const expr*> args;
    args.reserve(f->args.size());
    for (const expr* arg : f->args) {
        const std::optional<const expr*> reduced = reduce_within(arg, budget);
        if (!reduced.has_value())
            return std::nullopt;
        args.push_back(reduced.value());
    }
    const bool application = f->id == symbols_.application && args.size() == 2;
    const size_t binders = application ? count_binders_.count_binders(args[0]) : 0;
    if (binders == 0)
        return make_functor_.make_functor(f->id, args);
    // the outermost binder takes the argument; a `lams` keeps the rest
    const expr* body = std::get<expr::functor>(args[0]->content).args.back();
    const expr* rest = binders == 1   ? body
                       : binders == 2 ? make_functor_.make_functor(symbols_.lambda, {body})
                                      : make_functor_.make_functor(
                                            symbols_.lambdas, {make_literal_.make_literal(integer(binders - 1)), body});
    return apply(rest, args[1], budget);
}

// The lambda body with the argument for `#0`, reduced: shift(substitute(body, 0, shift(argument, 1, 0)), -1, 0).
ABS_REDUCER_TEMPLATE std::optional<const expr*> ABS_REDUCER::apply(
    const expr* body, const expr* argument, size_t& budget) {
    if (budget == 0)
        return std::nullopt;
    --budget;
    const expr* substituted = shift(substitute(body, 0, shift(argument, 1, 0)), -1, 0);
    return reduce_within(substituted, budget);
}

// Every index of e at or past `cutoff` binders out moved by `amount`. `lam` binds one more inside its body, and `let`
// one more inside its second argument.
ABS_REDUCER_TEMPLATE const expr* ABS_REDUCER::shift(const expr* e, int amount, size_t cutoff) {
    const std::optional<size_t> index = find_index_.find_index(e);
    if (index.has_value())
        return index.value() < cutoff ? e : make_index_.make_index(index.value() + amount);
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.empty())
        return e;
    std::vector<const expr*> args;
    args.reserve(f->args.size());
    for (size_t position = 0; position < f->args.size(); ++position)
        args.push_back(shift(f->args[position], amount, cutoff + count_bound(*f, e, position)));
    return make_functor_.make_functor(f->id, args);
}

// e with `value` for the index `index`, shifting `value` by one under each binder it enters.
ABS_REDUCER_TEMPLATE const expr* ABS_REDUCER::substitute(const expr* e, size_t index, const expr* value) {
    const std::optional<size_t> found = find_index_.find_index(e);
    if (found.has_value())
        return found.value() == index ? value : e;
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.empty())
        return e;
    std::vector<const expr*> args;
    args.reserve(f->args.size());
    for (size_t position = 0; position < f->args.size(); ++position) {
        const size_t bound = count_bound(*f, e, position);
        const expr* shifted = bound == 0 ? value : shift(value, static_cast<int>(bound), 0);
        args.push_back(substitute(f->args[position], index + bound, shifted));
    }
    return make_functor_.make_functor(f->id, args);
}

// How many binders the argument at `position` sits under, introduced by e itself: a `lam`'s body 1, a `lams(k, b)`'s
// body k, a `let`'s second argument 1, anything else 0.
ABS_REDUCER_TEMPLATE size_t ABS_REDUCER::count_bound(const expr::functor& f, const expr* e, size_t position) const {
    const bool body = position + 1 == f.args.size();
    if (f.id == symbols_.binding)
        return position == 1 ? 1 : 0;
    return body ? count_binders_.count_binders(e) : 0;
}

#undef ABS_REDUCER
#undef ABS_REDUCER_TEMPLATE

#endif
