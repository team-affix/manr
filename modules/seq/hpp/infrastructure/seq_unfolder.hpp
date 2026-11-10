#ifndef SEQ_UNFOLDER_HPP
#define SEQ_UNFOLDER_HPP

#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/rewrite.hpp"
#include "value_objects/seq_symbols.hpp"

// Local rule: the infinite list of a recurrence's terms, `tabulate(lam(rec(f, b0, ..., b(k-1), #0)))`, is
// `unfold(f, b0, ..., b(k-1))`, which drops the function around the recurrence and its index: always smaller.
template<typename IFindBody, typename IMakeFunctor>
struct seq_unfolder {
    seq_unfolder(IFindBody& find_body, IMakeFunctor& make_functor, seq_symbols symbols);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    IFindBody& find_body_;
    IMakeFunctor& make_functor_;
    seq_symbols symbols_;
};

#define SEQ_UNFOLDER_TEMPLATE template<typename IFindBody, typename IMakeFunctor>
#define SEQ_UNFOLDER          seq_unfolder<IFindBody, IMakeFunctor>

SEQ_UNFOLDER_TEMPLATE SEQ_UNFOLDER::seq_unfolder(IFindBody& find_body, IMakeFunctor& make_functor,
                                                 seq_symbols symbols)
    : find_body_(find_body)
    , make_functor_(make_functor)
    , symbols_(symbols) {}

SEQ_UNFOLDER_TEMPLATE void SEQ_UNFOLDER::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->id != symbols_.tabulation || f->args.size() != 1)
        return;
    const std::optional<const expr*> body = find_body_.find_body(f->args[0], 1);
    if (!body.has_value())
        return;
    const expr::functor* recurrence = std::get_if<expr::functor>(&body.value()->content);
    if (recurrence == nullptr || recurrence->id != symbols_.recurrence || recurrence->args.size() < 3)
        return;
    const expr::functor* index = std::get_if<expr::functor>(&recurrence->args.back()->content);
    const bool at_index = index != nullptr && index->id == symbols_.index && index->args.empty();
    if (!at_index)
        return;
    const std::vector<const expr*> parts(recurrence->args.begin(), recurrence->args.end() - 1);
    out.push_back([this, parts]() { return make_functor_.make_functor(symbols_.unfolding, parts); });
}

#undef SEQ_UNFOLDER
#undef SEQ_UNFOLDER_TEMPLATE

#endif
