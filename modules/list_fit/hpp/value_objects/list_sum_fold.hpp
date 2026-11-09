#ifndef LIST_SUM_FOLD_HPP
#define LIST_SUM_FOLD_HPP

#include <compare>
#include <map>
#include "value_objects/integer.hpp"

// A right fold over integer lists whose step adds a term of the head to the accumulator,
// `fold(lam(lam(g(#0) + #1)), base, xs)`, with g given by its value at each element value of the data.
struct list_sum_fold {
    std::map<integer, integer> terms;
    integer base;
    auto operator<=>(const list_sum_fold&) const = default;
};

#endif
