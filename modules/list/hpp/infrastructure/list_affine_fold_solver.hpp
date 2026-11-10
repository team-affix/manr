#ifndef LIST_AFFINE_FOLD_SOLVER_HPP
#define LIST_AFFINE_FOLD_SOLVER_HPP

#include <cstddef>
#include <optional>
#include <vector>
#include "value_objects/integer.hpp"
#include "value_objects/list_affine_fold.hpp"

// Solves for a right fold over integer lists whose step is affine in the accumulator, `p(h) + c * acc`, for a given
// multiplier c. On a list x1..xn such a fold is `sum_i c^(i-1) p(x_i) + c^n b`: once c is fixed it is linear in p's
// coefficients and the base b, so the value of every tail drops out and whole lists alone pin the fold, whether or
// not the data holds tails or the empty list. Lists are taken in order from a starting index (wrapping), skipping any
// that add no new constraint (Gauss-Jordan elimination over the rationals), until every unknown is pinned. The degree
// of p is at most one less than the number of distinct elements, and when every list has the same length the base
// absorbs p's constant term, so the unknowns are independent on any data that can pin them.
struct list_affine_fold_solver {
    // Nothing when too few lists to also check one beyond the unknowns, the lists run out first, or a value is a
    // proper fraction.
    std::optional<list_affine_fold> solve(const std::vector<std::vector<integer>>& lists,
                                          const std::vector<integer>& outputs,
                                          const integer& multiplier,
                                          size_t max_degree,
                                          size_t start) const;
    // The fold of every suffix of `list`, longest first: entry i folds list[i..], and the last entry is the base.
    std::vector<integer> accumulate(const list_affine_fold& fold, const std::vector<integer>& list) const;
};

#endif
