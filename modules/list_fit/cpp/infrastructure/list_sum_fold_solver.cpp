#include "infrastructure/list_sum_fold_solver.hpp"
#include <map>
#include <utility>
#include "value_objects/rational.hpp"

std::optional<list_sum_fold> list_sum_fold_solver::solve(const std::vector<std::vector<integer>>& lists,
                                                         const std::vector<integer>& outputs,
                                                         size_t start) const {
    // the unknowns: the base at column 0, then g at each element value
    std::map<integer, size_t> columns;
    for (const std::vector<integer>& list : lists)
        for (const integer& value : list)
            columns.emplace(value, 0);
    size_t unknowns = 1;
    for (auto& [value, column] : columns)
        column = unknowns++;
    if (lists.size() <= unknowns)
        return std::nullopt;
    // each basis row is the coefficients then the output, with a 1 at its pivot and 0 at every other pivot
    std::vector<std::vector<rational>> basis;
    std::vector<size_t> pivots;
    for (size_t offset = 0; offset < lists.size() && basis.size() < unknowns; ++offset) {
        const size_t index = (start + offset) % lists.size();
        std::vector<rational> row(unknowns + 1, rational(0));
        row[0] = 1;
        for (const integer& value : lists[index])
            row[columns.at(value)] += 1;
        row[unknowns] = rational(outputs[index]);
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
        if (pivot == unknowns)
            continue;
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
    if (basis.size() < unknowns)
        return std::nullopt;
    std::vector<integer> solution(unknowns);
    for (size_t member = 0; member < basis.size(); ++member) {
        const rational& value = basis[member][unknowns];
        if (boost::multiprecision::denominator(value) != 1)
            return std::nullopt;
        solution[pivots[member]] = boost::multiprecision::numerator(value);
    }
    list_sum_fold fold{.terms = {}, .base = solution[0]};
    for (const auto& [value, column] : columns)
        fold.terms.emplace(value, solution[column]);
    return fold;
}

integer list_sum_fold_solver::sum(const list_sum_fold& fold, const std::vector<integer>& list) const {
    integer total = fold.base;
    for (const integer& value : list)
        total += fold.terms.at(value);
    return total;
}
