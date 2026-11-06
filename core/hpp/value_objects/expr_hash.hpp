#ifndef EXPR_HASH_HPP
#define EXPR_HASH_HPP

#include <cstddef>
#include "expr.hpp"

struct expr_hash {
    size_t operator()(const expr& expression) const noexcept;

private:
    static size_t hash_combine(size_t seed, size_t value) noexcept;
};

#endif
