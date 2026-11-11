#ifndef PROG_HOLE_HPP
#define PROG_HOLE_HPP

#include <cstddef>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/prog_spec.hpp"

// A program with a hole to fill: any of `frames` (programs holding the hole `marker`, maybe more than once: one
// helper used twice) with the hole filled must meet `spec`. The hole's content is enumerated over `scope_envs`, the
// values its variables take (innermost last), and is of the type of `exemplar` (a value of that type), at most
// `max_size` nodes, `max_terms` terms in all.
struct prog_hole {
    std::vector<const expr*> frames;
    const expr* marker;
    std::vector<std::vector<const expr*>> scope_envs;
    const expr* exemplar;
    prog_spec spec;
    size_t max_size;
    size_t max_terms;
    auto operator<=>(const prog_hole&) const = default;
};

#endif
