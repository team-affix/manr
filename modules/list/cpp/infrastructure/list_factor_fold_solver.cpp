#include "infrastructure/list_factor_fold_solver.hpp"
#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <utility>

std::optional<list_factor_fold> list_factor_fold_solver::solve(const std::vector<std::vector<integer>>& lists,
                                                               const std::vector<integer>& outputs) const {
    // the unknowns: the base at column 0, then g at each value of a list with a nonzero output
    std::map<integer, size_t> columns;
    std::set<integer> zero_values;
    std::vector<size_t> rows;
    for (size_t index = 0; index < lists.size(); ++index) {
        const bool zero = outputs[index] == 0;
        if (!zero)
            rows.push_back(index);
        for (const integer& value : lists[index]) {
            if (zero) {
                zero_values.insert(value);
                continue;
            }
            columns.emplace(value, 0);
        }
    }
    if (rows.empty())
        return std::nullopt;
    size_t unknowns = 1;
    for (auto& [value, column] : columns)
        column = unknowns++;
    const bool checkable = rows.size() > unknowns;
    if (!checkable)
        return std::nullopt;
    // magnitudes: least squares on the logarithms, by the normal equations
    std::vector<std::vector<double>> normal(unknowns, std::vector<double>(unknowns + 1, 0));
    for (const size_t index : rows) {
        const double magnitude = static_cast<double>(abs(outputs[index]));
        if (!std::isfinite(magnitude))
            return std::nullopt;
        std::vector<double> row(unknowns, 0);
        row[0] = 1;
        for (const integer& value : lists[index])
            row[columns.at(value)] += 1;
        for (size_t first = 0; first < unknowns; ++first) {
            for (size_t second = 0; second < unknowns; ++second)
                normal[first][second] += row[first] * row[second];
            normal[first][unknowns] += row[first] * std::log(magnitude);
        }
    }
    for (size_t pivot = 0; pivot < unknowns; ++pivot) {
        size_t best = pivot;
        for (size_t candidate = pivot + 1; candidate < unknowns; ++candidate)
            if (std::fabs(normal[candidate][pivot]) > std::fabs(normal[best][pivot]))
                best = candidate;
        if (std::fabs(normal[best][pivot]) < 1e-9)
            return std::nullopt;
        std::swap(normal[pivot], normal[best]);
        for (size_t other = 0; other < unknowns; ++other) {
            if (other == pivot)
                continue;
            const double scale = normal[other][pivot] / normal[pivot][pivot];
            for (size_t column = pivot; column <= unknowns; ++column)
                normal[other][column] -= scale * normal[pivot][column];
        }
    }
    std::vector<integer> magnitudes(unknowns, 0);
    for (size_t column = 0; column < unknowns; ++column) {
        const double magnitude = std::round(std::exp(normal[column][unknowns] / normal[column][column]));
        const bool representable = magnitude >= 1 && magnitude < 4.6e18;
        if (!representable)
            return std::nullopt;
        magnitudes[column] = static_cast<long long>(magnitude);
    }
    // signs: each output's sign is the parity of its negative factors, solved over GF(2); free signs are positive
    std::vector<std::vector<bool>> parities;
    std::vector<size_t> pivots;
    for (const size_t index : rows) {
        std::vector<bool> row(unknowns + 1, false);
        row[0] = true;
        for (const integer& value : lists[index])
            row[columns.at(value)] = !row[columns.at(value)];
        row[unknowns] = outputs[index] < 0;
        for (size_t member = 0; member < parities.size(); ++member) {
            if (!row[pivots[member]])
                continue;
            for (size_t column = 0; column <= unknowns; ++column)
                row[column] = row[column] != parities[member][column];
        }
        size_t pivot = 0;
        while (pivot < unknowns && !row[pivot])
            ++pivot;
        if (pivot == unknowns)
            continue;
        for (std::vector<bool>& member : parities) {
            if (!member[pivot])
                continue;
            for (size_t column = 0; column <= unknowns; ++column)
                member[column] = member[column] != row[column];
        }
        parities.push_back(std::move(row));
        pivots.push_back(pivot);
    }
    std::vector<bool> negative(unknowns, false);
    for (size_t member = 0; member < parities.size(); ++member)
        negative[pivots[member]] = parities[member][unknowns];
    list_factor_fold fold{.factors = {}, .base = negative[0] ? integer(-magnitudes[0]) : magnitudes[0]};
    for (const auto& [value, column] : columns)
        fold.factors.emplace(value, negative[column] ? integer(-magnitudes[column]) : magnitudes[column]);
    for (const integer& value : zero_values)
        fold.factors.emplace(value, 0);
    return fold;
}

integer list_factor_fold_solver::multiply(const list_factor_fold& fold, const std::vector<integer>& list) const {
    integer product = fold.base;
    for (const integer& value : list)
        product *= fold.factors.at(value);
    return product;
}
