#ifndef PW_COVERAGE_HPP
#define PW_COVERAGE_HPP

#include <cstdint>
#include <map>
#include <vector>
#include "value_objects/expr.hpp"

// What a model leaves of a piecewise's branches: the branches it does not reproduce, and the one binding of holes
// under which it reproduces the rest. A rewrite that drops the reproduced branches applies the binding to what it
// keeps, since a hole is one value everywhere.
struct pw_coverage {
    std::vector<const expr*> uncovered;
    std::map<uint32_t, const expr*> bindings;
    auto operator<=>(const pw_coverage&) const = default;
};

#endif
