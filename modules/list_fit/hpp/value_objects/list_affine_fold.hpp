#ifndef LIST_AFFINE_FOLD_HPP
#define LIST_AFFINE_FOLD_HPP

#include <compare>
#include <vector>
#include "value_objects/integer.hpp"

// A right fold over integer lists whose step is affine in the accumulator: `fold(p(h) + multiplier * acc, base, #0)`,
// with p given by its coefficients, lowest degree first.
struct list_affine_fold {
    integer multiplier;
    std::vector<integer> coefficients;
    integer base;
    auto operator<=>(const list_affine_fold&) const = default;
};

#endif
