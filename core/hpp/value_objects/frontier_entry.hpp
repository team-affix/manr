#ifndef FRONTIER_ENTRY_HPP
#define FRONTIER_ENTRY_HPP

#include <compare>
#include <cstddef>
#include <cstdint>

// The key of a pending rewrite on a best-first frontier: the cost of the expression it was detected on, then
// detection order, so rewrites of equally cheap expressions are taken first in, first out.
struct frontier_entry {
    size_t cost;
    uint64_t order;
    auto operator<=>(const frontier_entry&) const = default;
};

#endif
