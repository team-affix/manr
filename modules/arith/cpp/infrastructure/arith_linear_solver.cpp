#include "infrastructure/arith_linear_solver.hpp"
#include <utility>
#include "value_objects/rational.hpp"

std::optional<std::vector<integer>> arith_linear_solver::solve(const std::vector<std::vector<integer>>& rows,
                                                               size_t unknowns) const {
    // each basis row has a 1 at its pivot and 0 at every other pivot
    std::vector<std::vector<rational>> basis;
    std::vector<size_t> pivots;
    for (const std::vector<integer>& equation : rows) {
        std::vector<rational> row(equation.begin(), equation.end());
        for (size_t member = 0; member < basis.size(); ++member) {
            const rational scale = row[pivots[member]];
            if (scale == 0)
                continue;
            for (size_t column = 0; column <= unknowns; ++column)
                row[column] -= scale * basis[member][column];
        }
        size_t pivot = 0;
        while (pivot < unknowns && row[pivot] == 0)
            ++pivot;
        if (pivot == unknowns) {
            const bool inconsistent = row[unknowns] != 0;
            if (inconsistent)
                return std::nullopt;
            continue;
        }
        const rational lead = row[pivot];
        for (rational& entry : row)
            entry /= lead;
        for (std::vector<rational>& member : basis) {
            const rational scale = member[pivot];
            if (scale == 0)
                continue;
            for (size_t column = 0; column <= unknowns; ++column)
                member[column] -= scale * row[column];
        }
        basis.push_back(std::move(row));
        pivots.push_back(pivot);
    }
    // free unknowns are 0, so each pinned one is its row's right-hand side
    std::vector<integer> solution(unknowns, integer(0));
    for (size_t member = 0; member < basis.size(); ++member) {
        const rational& value = basis[member][unknowns];
        if (boost::multiprecision::denominator(value) != 1)
            return std::nullopt;
        solution[pivots[member]] = boost::multiprecision::numerator(value);
    }
    return solution;
}
