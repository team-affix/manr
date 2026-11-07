#include "infrastructure/poly_interpolator.hpp"
#include <algorithm>
#include <utility>
#include "value_objects/rational.hpp"

poly_interpolator::poly_interpolator(std::vector<uint32_t> variables, size_t max_unknowns)
    : variables_(variables)
    , sorted_variables_(std::move(variables))
    , max_unknowns_(max_unknowns) {
    std::sort(sorted_variables_.begin(), sorted_variables_.end());
}

std::optional<poly_interpolant> poly_interpolator::interpolate(
    const std::vector<poly_sample>& samples, size_t degree, size_t start) const {
    const size_t unknowns = count_monomials(degree);
    const bool checkable = unknowns + 1 <= samples.size();
    if (unknowns > max_unknowns_ || !checkable)
        return std::nullopt;
    std::vector<poly_monomial> monomials;
    poly_monomial current;
    extend_monomials(0, degree, current, monomials);
    // each basis row is the coefficients then the output, with a 1 at its pivot and 0 at every other pivot
    std::vector<std::vector<rational>> basis;
    std::vector<size_t> pivots;
    for (size_t offset = 0; offset < samples.size() && basis.size() < unknowns; ++offset) {
        const poly_sample& sample = samples[(start + offset) % samples.size()];
        std::vector<rational> row;
        row.reserve(unknowns + 1);
        for (const poly_monomial& monomial : monomials)
            row.push_back(rational(evaluate_monomial(monomial, sample.inputs)));
        row.push_back(rational(sample.output));
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
    poly_interpolant interpolant{.polynomial = {}, .unknowns = unknowns};
    for (size_t member = 0; member < basis.size(); ++member) {
        const rational& value = basis[member][unknowns];
        if (boost::multiprecision::denominator(value) != 1)
            return std::nullopt;
        if (value == 0)
            continue;
        interpolant.polynomial.terms.emplace(monomials[pivots[member]], boost::multiprecision::numerator(value));
    }
    return interpolant;
}

integer poly_interpolator::evaluate(const poly_polynomial& polynomial, const std::vector<integer>& inputs) const {
    integer sum = 0;
    for (const auto& [monomial, coefficient] : polynomial.terms)
        sum += coefficient * evaluate_monomial(monomial, inputs);
    return sum;
}

// The number of monomials in variables_ of total degree <= degree: C(variables + degree, degree), without building
// them.
size_t poly_interpolator::count_monomials(size_t degree) const {
    size_t count = 1;
    for (size_t step = 1; step <= degree; ++step) {
        count = count * (variables_.size() + step) / step;
        if (count > max_unknowns_)
            return count;
    }
    return count;
}

// Appends every monomial in sorted_variables_[index..] of total degree <= remaining, times `current`.
void poly_interpolator::extend_monomials(
    size_t index, size_t remaining, poly_monomial& current, std::vector<poly_monomial>& out) const {
    if (index == sorted_variables_.size()) {
        out.push_back(current);
        return;
    }
    extend_monomials(index + 1, remaining, current, out);
    for (uint32_t exponent = 1; exponent <= remaining; ++exponent) {
        current.powers.push_back(poly_power{.variable = sorted_variables_[index], .exponent = exponent});
        extend_monomials(index + 1, remaining - exponent, current, out);
        current.powers.pop_back();
    }
}

// The monomial's value with the i-th variable bound to inputs[i].
integer poly_interpolator::evaluate_monomial(const poly_monomial& monomial, const std::vector<integer>& inputs) const {
    integer value = 1;
    for (const poly_power& power : monomial.powers) {
        const size_t position = std::find(variables_.begin(), variables_.end(), power.variable) - variables_.begin();
        value *= boost::multiprecision::pow(inputs[position], power.exponent);
    }
    return value;
}
