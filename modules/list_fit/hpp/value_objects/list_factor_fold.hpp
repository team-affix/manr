#ifndef LIST_FACTOR_FOLD_HPP
#define LIST_FACTOR_FOLD_HPP

#include <compare>
#include <map>
#include "value_objects/integer.hpp"

// A right fold over integer lists whose step multiplies the accumulator by a factor of the head,
// `fold(g(h) * acc, base, xs)`, with g given by its value at each element value of the data.
struct list_factor_fold {
    std::map<integer, integer> factors;
    integer base;
    auto operator<=>(const list_factor_fold&) const = default;
};

#endif
