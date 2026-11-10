#ifndef LIST_FACTOR_FOLD_SOLVER_HPP
#define LIST_FACTOR_FOLD_SOLVER_HPP

#include <optional>
#include <vector>
#include "value_objects/integer.hpp"
#include "value_objects/list_factor_fold.hpp"

// Solves for a right fold over integer lists whose step multiplies the accumulator by an unknown factor of the head,
// `g(h) * acc`, from whole lists: on a list x1..xn its value is `b * g(x1) * ... * g(xn)`. The unknowns are b and g at
// each element value. Values that occur only in lists whose output is 0 get the factor 0. On the other lists the
// output's magnitude is a product, so `log|output|` is linear in `log|g(v)|` and `log|b|`: least squares pins those,
// rounded to integers. The sign of the output is the parity of the negative factors, linear mod 2: elimination over
// GF(2) pins the signs, with any it leaves free taken positive. Wrong labels can spoil the rounding or the signs, so the
// caller checks the result exactly against every list.
struct list_factor_fold_solver {
    // Nothing when every output is 0, the lists with a nonzero output are no more than the unknowns, the logarithms
    // do not pin every unknown, or a magnitude rounds to 0 or past 2^62.
    std::optional<list_factor_fold> solve(const std::vector<std::vector<integer>>& lists,
                                          const std::vector<integer>& outputs) const;
    // The fold's value on the list.
    integer multiply(const list_factor_fold& fold, const std::vector<integer>& list) const;
};

#endif
