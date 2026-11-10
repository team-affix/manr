#ifndef LIST_SAMPLE_HPP
#define LIST_SAMPLE_HPP

#include <compare>
#include <vector>
#include "value_objects/integer.hpp"

// One observation of a list program: the input list and the output, which is a list when `output_is_list` and
// otherwise the single integer `output[0]`.
struct list_sample {
    std::vector<integer> input;
    std::vector<integer> output;
    bool output_is_list;
    auto operator<=>(const list_sample&) const = default;
};

#endif
