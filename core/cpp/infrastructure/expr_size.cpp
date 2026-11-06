#include "infrastructure/expr_size.hpp"
#include <variant>

size_t expr_size::cost(const expr* e) {
    const auto found = costs_.find(e);
    if (found != costs_.end())
        return found->second;
    size_t total = 1;
    if (const expr::functor* f = std::get_if<expr::functor>(&e->content))
        for (const expr* arg : f->args)
            total += cost(arg);
    costs_.emplace(e, total);
    return total;
}
