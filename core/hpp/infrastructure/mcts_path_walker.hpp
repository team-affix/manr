#ifndef MCTS_PATH_WALKER_HPP
#define MCTS_PATH_WALKER_HPP

#include <cstddef>
#include <cstdint>

// Names an MCTS node by the path of choices from the root, hashed into 64 bits. A choice is the index of a detected
// rewrite; detection order is deterministic, so the same index from the same node is the same rewrite. Two routes
// to the same expression are different nodes, so this is tree MCTS. The root is node 0.
struct mcts_path_walker {
    uint64_t walk(const uint64_t& node, const size_t& choice) const;
};

#endif
