#ifndef EXPR_SIZE_HPP
#define EXPR_SIZE_HPP

#include <cstddef>
#include <unordered_map>
#include "value_objects/expr.hpp"

// Descriptive complexity of an expression: its node count, counting a shared subterm once per occurrence.
// Memoized per interned address.
struct expr_size {
    size_t cost(const expr* e);
private:
    std::unordered_map<const expr*, size_t> costs_;
};

#endif
