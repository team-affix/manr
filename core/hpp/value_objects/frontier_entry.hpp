#ifndef FRONTIER_ENTRY_HPP
#define FRONTIER_ENTRY_HPP

#include <compare>
#include <cstddef>
#include <cstdint>
#include "value_objects/expr.hpp"

// Ordered by cost, then by enqueue order, so equal-cost expressions are expanded first in, first out.
struct frontier_entry {
    size_t cost;
    uint64_t order;
    const expr* expression;
    auto operator<=>(const frontier_entry&) const = default;
};

#endif
