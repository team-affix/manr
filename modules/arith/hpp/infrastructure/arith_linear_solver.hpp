#ifndef ARITH_LINEAR_SOLVER_HPP
#define ARITH_LINEAR_SOLVER_HPP

#include <cstddef>
#include <optional>
#include <vector>
#include "value_objects/integer.hpp"

// Solves a system of linear equations over the integers by exact elimination over the rationals. Each row holds the
// coefficients of `unknowns` unknowns, then the right-hand side. Unknowns no equation pins are free and set to 0.
// Nothing when the system is inconsistent or the solution is not integral.
struct arith_linear_solver {
    std::optional<std::vector<integer>> solve(const std::vector<std::vector<integer>>& rows, size_t unknowns) const;
};

#endif
