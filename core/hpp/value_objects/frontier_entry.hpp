#ifndef FRONTIER_ENTRY_HPP
#define FRONTIER_ENTRY_HPP

#include <compare>
#include <cstddef>
#include <cstdint>

// The key of a reached expression on a best-first frontier: its cost, then the order it was reached in, so equally
// cheap expressions are expanded first in, first out.
struct frontier_entry {
    size_t cost;
    uint64_t order;
    auto operator<=>(const frontier_entry&) const = default;
};

#endif
