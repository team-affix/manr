#include "infrastructure/list_affine_fold_solver.hpp"
#include <algorithm>
#include <set>
#include <utility>
#include "value_objects/rational.hpp"

std::optional<list_affine_fold> list_affine_fold_solver::solve(const std::vector<std::vector<integer>>& lists,
                                                               const std::vector<integer>& outputs,
                                                               const integer& multiplier,
                                                               size_t max_degree,
                                                               size_t start) const {
    std::set<integer> elements;
    std::set<size_t> lengths;
    for (const std::vector<integer>& list : lists) {
        elements.insert(list.begin(), list.end());
        lengths.insert(list.size());
    }
    if (elements.empty())
        return std::nullopt;
    const size_t degree = std::min(max_degree, elements.size() - 1);
    const size_t lowest = lengths.size() > 1 ? 0 : 1;
    // the unknowns: the base, then p's coefficients from degree `lowest` to `degree`
    const size_t unknowns = 1 + degree + 1 - lowest;
    const bool checkable = unknowns + 1 <= lists.size();
    if (!checkable)
        return std::nullopt;
    // each basis row is the coefficients then the output, with a 1 at its pivot and 0 at every other pivot
    std::vector<std::vector<rational>> basis;
    std::vector<size_t> pivots;
    for (size_t offset = 0; offset < lists.size() && basis.size() < unknowns; ++offset) {
        const size_t index = (start + offset) % lists.size();
        std::vector<integer> sums(degree + 1, 0);
        integer weight = 1;
        for (const integer& element : lists[index]) {
            integer power = 1;
            for (size_t exponent = 0; exponent <= degree; ++exponent) {
                sums[exponent] += weight * power;
                power *= element;
            }
            weight *= multiplier;
        }
        std::vector<rational> row;
        row.reserve(unknowns + 1);
        row.push_back(rational(weight));
        for (size_t exponent = lowest; exponent <= degree; ++exponent)
            row.push_back(rational(sums[exponent]));
        row.push_back(rational(outputs[index]));
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
        for (rational& value : row)
            value /= lead;
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
    std::vector<integer> values(unknowns, 0);
    for (size_t member = 0; member < basis.size(); ++member) {
        const rational& value = basis[member][unknowns];
        if (boost::multiprecision::denominator(value) != 1)
            return std::nullopt;
        values[pivots[member]] = boost::multiprecision::numerator(value);
    }
    list_affine_fold fold{.multiplier = multiplier, .coefficients = std::vector<integer>(degree + 1, 0), .base = values[0]};
    for (size_t exponent = lowest; exponent <= degree; ++exponent)
        fold.coefficients[exponent] = values[1 + exponent - lowest];
    return fold;
}

std::vector<integer> list_affine_fold_solver::accumulate(const list_affine_fold& fold,
                                                         const std::vector<integer>& list) const {
    std::vector<integer> accumulators(list.size() + 1);
    accumulators[list.size()] = fold.base;
    for (size_t index = list.size(); index-- > 0;) {
        integer step = 0;
        for (size_t exponent = fold.coefficients.size(); exponent-- > 0;)
            step = step * list[index] + fold.coefficients[exponent];
        accumulators[index] = step + fold.multiplier * accumulators[index + 1];
    }
    return accumulators;
}
