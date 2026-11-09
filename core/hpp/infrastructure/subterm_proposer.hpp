#ifndef SUBTERM_PROPOSER_HPP
#define SUBTERM_PROPOSER_HPP

#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/rewrite.hpp"

// Lifts a local rule set (rules that only look at the expression they are handed) to every position of an expression.
// Each rewrite detected at a subterm is wrapped so that, when invoked, it performs the local rewrite and rebuilds the
// path from that subterm back up to the expression it was handed, through the pool. Nothing is rebuilt until a rewrite
// is taken.
template<typename IDetectLocal, typename IMakeFunctor>
struct subterm_proposer {
    subterm_proposer(IDetectLocal& detect_local, IMakeFunctor& make_functor);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    IDetectLocal& detect_local_;
    IMakeFunctor& make_functor_;
};

#define SUBTERM_PROPOSER_TEMPLATE template<typename IDetectLocal, typename IMakeFunctor>
#define SUBTERM_PROPOSER          subterm_proposer<IDetectLocal, IMakeFunctor>

SUBTERM_PROPOSER_TEMPLATE SUBTERM_PROPOSER::subterm_proposer(IDetectLocal& detect_local, IMakeFunctor& make_functor)
    : detect_local_(detect_local)
    , make_functor_(make_functor) {}

SUBTERM_PROPOSER_TEMPLATE void SUBTERM_PROPOSER::detect(const expr* e, std::vector<rewrite>& out) {
    detect_local_.detect(e, out);
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr)
        return;
    for (size_t i = 0; i < f->args.size(); ++i) {
        std::vector<rewrite> arg_rewrites;
        detect(f->args[i], arg_rewrites);
        for (rewrite& arg_rewrite : arg_rewrites) {
            out.push_back([this, f, i, inner = std::move(arg_rewrite)]() -> const expr* {
                std::vector<const expr*> args = f->args;
                args[i] = inner();
                return make_functor_.make_functor(f->id, args);
            });
        }
    }
}

#undef SUBTERM_PROPOSER
#undef SUBTERM_PROPOSER_TEMPLATE

#endif
