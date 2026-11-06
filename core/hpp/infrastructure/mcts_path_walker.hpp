#ifndef MCTS_PATH_WALKER_HPP
#define MCTS_PATH_WALKER_HPP

#include <cstdint>
#include "value_objects/expr.hpp"

// Names an MCTS node by the path of choices from the root, hashed into 64 bits. Two routes to the same expression
// are different nodes, so this is tree MCTS: a node's statistics only ever come from simulations through its own
// prefix. The root is node 0.
struct mcts_path_walker {
    uint64_t walk(const uint64_t& node, const expr* chosen) const;
};

#endif
