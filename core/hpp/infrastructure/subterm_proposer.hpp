#ifndef SUBTERM_PROPOSER_HPP
#define SUBTERM_PROPOSER_HPP

#include <variant>
#include <vector>
#include "value_objects/expr.hpp"

// Lifts a local proposer (one that only looks at the expression it is handed) to every position of an
// expression: each local rewrite of a subterm becomes a rewrite of the whole expression, with the path from the
// root to that subterm rebuilt through the pool. One rewrite is applied per proposal.
template<typename IProposeLocal, typename IMakeFunctor>
struct subterm_proposer {
    subterm_proposer(IProposeLocal& propose_local, IMakeFunctor& make_functor);
    void propose(const expr* e, std::vector<const expr*>& out);
private:
    IProposeLocal& propose_local_;
    IMakeFunctor& make_functor_;
};

#define SUBTERM_PROPOSER_TEMPLATE template<typename IProposeLocal, typename IMakeFunctor>
#define SUBTERM_PROPOSER          subterm_proposer<IProposeLocal, IMakeFunctor>

SUBTERM_PROPOSER_TEMPLATE SUBTERM_PROPOSER::subterm_proposer(IProposeLocal& propose_local, IMakeFunctor& make_functor)
    : propose_local_(propose_local)
    , make_functor_(make_functor) {}

SUBTERM_PROPOSER_TEMPLATE void SUBTERM_PROPOSER::propose(const expr* e, std::vector<const expr*>& out) {
    propose_local_.propose(e, out);
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr)
        return;
    for (size_t i = 0; i < f->args.size(); ++i) {
        std::vector<const expr*> arg_rewrites;
        propose(f->args[i], arg_rewrites);
        for (const expr* arg_rewrite : arg_rewrites) {
            std::vector<const expr*> args = f->args;
            args[i] = arg_rewrite;
            out.push_back(make_functor_.make_functor(f->id, args));
        }
    }
}

#undef SUBTERM_PROPOSER
#undef SUBTERM_PROPOSER_TEMPLATE

#endif
