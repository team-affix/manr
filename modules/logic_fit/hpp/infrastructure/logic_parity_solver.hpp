#ifndef LOGIC_PARITY_SOLVER_HPP
#define LOGIC_PARITY_SOLVER_HPP

#include <optional>
#include "value_objects/logic_parity.hpp"
#include "value_objects/logic_truth_table.hpp"

// Solves a truth table as an affine function over GF(2), `c0 xor c1 x1 xor ... xor ck xk`: every row is one linear
// equation mod 2, solved exactly by elimination on bit sets. Nothing when the rows are inconsistent (the table is not
// a parity) or the table is wider than 63 variables. Variables the rows leave undetermined are left out.
struct logic_parity_solver {
    std::optional<logic_parity> solve(const logic_truth_table& table) const;
};

#endif
