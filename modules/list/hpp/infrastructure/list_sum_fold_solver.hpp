#ifndef LIST_SUM_FOLD_SOLVER_HPP
#define LIST_SUM_FOLD_SOLVER_HPP

#include <cstddef>
#include <optional>
#include <vector>
#include "value_objects/integer.hpp"
#include "value_objects/list_sum_fold.hpp"

// Solves for a right fold over integer lists whose step adds an unknown term of the head to the accumulator,
// `g(h) + acc`, from whole lists: on a list x1..xn its value is `b + g(x1) + ... + g(xn)`, linear in the unknowns b
// and g at each element value, with each value's count in the list as its coefficient. Lists are taken in order from
// `start` (wrapping), each one that adds a new constraint joining an exact elimination over the rationals, until
// every unknown is pinned. A wrong label among those lists spoils the answer, so the caller checks it against every
// list and may try other starts.
struct list_sum_fold_solver {
    // Nothing when the lists run out before every unknown is pinned, or an unknown is a proper fraction.
    std::optional<list_sum_fold> solve(const std::vector<std::vector<integer>>& lists,
                                       const std::vector<integer>& outputs,
                                       size_t start) const;
    // The fold's value on the list.
    integer sum(const list_sum_fold& fold, const std::vector<integer>& list) const;
};

#endif
