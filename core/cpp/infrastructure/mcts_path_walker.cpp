#include "infrastructure/mcts_path_walker.hpp"

// splitmix64 over the parent's name mixed with the chosen expression's address.
uint64_t mcts_path_walker::walk(const uint64_t& node, const expr* chosen) const {
    uint64_t mixed = node ^ (reinterpret_cast<uintptr_t>(chosen) + 0x9e3779b97f4a7c15ull + (node << 6) + (node >> 2));
    mixed = (mixed ^ (mixed >> 30)) * 0xbf58476d1ce4e5b9ull;
    mixed = (mixed ^ (mixed >> 27)) * 0x94d049bb133111ebull;
    return mixed ^ (mixed >> 31);
}
