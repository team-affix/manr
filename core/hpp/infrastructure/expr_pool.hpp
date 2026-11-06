#ifndef EXPR_POOL_HPP
#define EXPR_POOL_HPP

#include <unordered_set>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/expr_hash.hpp"

// Hash-conses expressions: structurally equal expressions share one address, so pointer equality is
// structural equality and a search can detect a revisited expression in O(1).
struct expr_pool {
    const expr* make_functor(uint32_t id, const std::vector<const expr*>& args);
    const expr* make_var(uint32_t index);
    size_t size() const;
private:
    const expr* intern(expr&&);
    std::unordered_set<expr, expr_hash> exprs_;
};

#endif
