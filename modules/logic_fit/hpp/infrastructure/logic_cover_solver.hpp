#ifndef LOGIC_COVER_SOLVER_HPP
#define LOGIC_COVER_SOLVER_HPP

#include <cstddef>
#include <cstdint>
#include <set>
#include <vector>
#include "value_objects/logic_implicant.hpp"
#include "value_objects/logic_truth_table.hpp"

constexpr size_t k_logic_default_max_cover_width = 8;

// A small sum of products for the rows of a truth table whose output is `output` (Quine-McCluskey): inputs absent from
// the table are free to be either value (don't cares), so a cover may generalize over them. The prime implicants come
// from merging implicants that differ in one variable; the cover takes every essential prime, then the prime covering
// the most rows still uncovered (fewest literals on a tie) until every row is covered. Empty when no row has that
// output or the table is wider than `max_width`.
struct logic_cover_solver {
    logic_cover_solver(size_t max_width);
    std::vector<logic_implicant> find_cover(const logic_truth_table& table, bool output) const;
private:
    std::set<logic_implicant> find_primes(const logic_truth_table& table, bool output) const;

    size_t max_width_;
};

#endif
