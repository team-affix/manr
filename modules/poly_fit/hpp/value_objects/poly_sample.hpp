#ifndef POLY_SAMPLE_HPP
#define POLY_SAMPLE_HPP

#include <compare>
#include <vector>
#include "value_objects/integer.hpp"

// One observation: the value of every input variable (in the solver's variable order) and the output.
struct poly_sample {
    std::vector<integer> inputs;
    integer output;
    auto operator<=>(const poly_sample&) const = default;
};

#endif
