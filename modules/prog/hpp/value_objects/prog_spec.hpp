#ifndef PROG_SPEC_HPP
#define PROG_SPEC_HPP

#include <cstdint>
#include <vector>
#include "value_objects/expr.hpp"

// how a program's value at an environment must relate to the wanted output
constexpr uint8_t k_prog_relation_equal = 0;          // the same value
constexpr uint8_t k_prog_relation_permutation = 1;    // the same elements in any order (under a `sort`)
constexpr uint8_t k_prog_relation_set = 2;            // the same distinct elements (under `sort(unique(..))`)
constexpr uint8_t k_prog_relation_truth = 3;          // nonzero exactly where the wanted value is (a predicate)
constexpr uint8_t k_prog_relation_supersequence = 4;  // holds the wanted list as a subsequence (a filter's source)

// What a program must do: at each environment (the values of the variables in scope, innermost last, `#0`), a value
// related to the wanted output by `relation`. A data set is one (its inputs as environments); so is each subproblem a
// decomposition derives (a map's body: the environment and one element, wanted its image).
struct prog_spec {
    std::vector<std::vector<const expr*>> envs;
    std::vector<const expr*> outputs;
    uint8_t relation;
    auto operator<=>(const prog_spec&) const = default;
};

#endif
