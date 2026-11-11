#include "rules/list_identity_map_remover.hpp"

list_identity_map_remover::list_identity_map_remover(list_symbols symbols)
    : symbols_(symbols) {}

void list_identity_map_remover::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->id != symbols_.map || f->args.size() != 2)
        return;
    const expr::functor* function = std::get_if<expr::functor>(&f->args[0]->content);
    if (function == nullptr || function->id != symbols_.lambda || function->args.size() != 1)
        return;
    const expr::functor* body = std::get_if<expr::functor>(&function->args[0]->content);
    const bool identity = body != nullptr && body->id == symbols_.element && body->args.empty();
    if (!identity)
        return;
    out.push_back(ready(f->args[1]));
}
