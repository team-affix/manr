#include "infrastructure/expr_pool.hpp"

const expr* expr_pool::make_functor(uint32_t id, const std::vector<const expr*>& args) {
    return intern(expr{expr::functor{id, args}});
}

const expr* expr_pool::make_var(uint32_t index) {
    return intern(expr{expr::var{index}});
}

size_t expr_pool::size() const {
    return exprs_.size();
}

const expr* expr_pool::intern(expr&& e) {
    return &*exprs_.insert(std::move(e)).first;
}
