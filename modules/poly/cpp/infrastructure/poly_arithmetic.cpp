#include "infrastructure/poly_arithmetic.hpp"
#include <set>
#include <utility>
#include <vector>
#include "value_objects/rational.hpp"
#include "debug_assert.hpp"

namespace {

// the exponent of `variable` in a polynomial: its largest power there
uint32_t find_degree(const poly_polynomial& polynomial, uint32_t variable) {
    uint32_t degree = 0;
    for (const auto& [monomial, coefficient] : polynomial.terms)
        for (const poly_power& power : monomial.powers)
            if (power.variable == variable && power.exponent > degree)
                degree = power.exponent;
    return degree;
}

// the coefficients of the powers of `variable`, lowest first, each a polynomial in the other variables
std::vector<poly_polynomial> split_by_degree(const poly_polynomial& polynomial, uint32_t variable) {
    std::vector<poly_polynomial> coefficients(find_degree(polynomial, variable) + 1);
    for (const auto& [monomial, coefficient] : polynomial.terms) {
        poly_monomial rest;
        uint32_t degree = 0;
        for (const poly_power& power : monomial.powers) {
            if (power.variable == variable) {
                degree = power.exponent;
                continue;
            }
            rest.powers.push_back(power);
        }
        coefficients[degree].terms.emplace(rest, coefficient);
    }
    return coefficients;
}

// divided by the gcd of its coefficients, with a positive leading coefficient
poly_polynomial make_primitive(const poly_polynomial& polynomial) {
    integer common = 0;
    for (const auto& [monomial, coefficient] : polynomial.terms)
        common = boost::multiprecision::gcd(common, coefficient);
    if (common == 0)
        return polynomial;
    if (polynomial.terms.rbegin()->second < 0)
        common = -common;
    poly_polynomial primitive;
    for (const auto& [monomial, coefficient] : polynomial.terms)
        primitive.terms.emplace(monomial, integer(coefficient / common));
    return primitive;
}

poly_monomial multiply_monomials(const poly_monomial& lhs, const poly_monomial& rhs) {
    poly_monomial product;
    size_t l = 0;
    size_t r = 0;
    while (l < lhs.powers.size() || r < rhs.powers.size()) {
        const bool take_lhs = r == rhs.powers.size()
            || (l < lhs.powers.size() && lhs.powers[l].variable < rhs.powers[r].variable);
        const bool take_rhs = l == lhs.powers.size()
            || (r < rhs.powers.size() && rhs.powers[r].variable < lhs.powers[l].variable);
        if (take_lhs) { product.powers.push_back(lhs.powers[l++]); continue; }
        if (take_rhs) { product.powers.push_back(rhs.powers[r++]); continue; }
        product.powers.push_back(poly_power{
            .variable = lhs.powers[l].variable,
            .exponent = lhs.powers[l].exponent + rhs.powers[r].exponent});
        ++l;
        ++r;
    }
    return product;
}

std::optional<poly_monomial> divide_monomials(const poly_monomial& dividend, const poly_monomial& divisor) {
    poly_monomial quotient;
    size_t d = 0;
    for (const poly_power& power : dividend.powers) {
        if (d < divisor.powers.size() && divisor.powers[d].variable < power.variable)
            return std::nullopt;
        const bool shared = d < divisor.powers.size() && divisor.powers[d].variable == power.variable;
        if (!shared) { quotient.powers.push_back(power); continue; }
        if (divisor.powers[d].exponent > power.exponent)
            return std::nullopt;
        if (divisor.powers[d].exponent < power.exponent)
            quotient.powers.push_back(poly_power{
                .variable = power.variable, .exponent = power.exponent - divisor.powers[d].exponent});
        ++d;
    }
    if (d != divisor.powers.size())
        return std::nullopt;
    return quotient;
}

}

poly_arithmetic::poly_arithmetic(size_t max_terms)
    : max_terms_(max_terms) {}

std::optional<poly_polynomial> poly_arithmetic::add(const poly_polynomial& lhs, const poly_polynomial& rhs) const {
    return combine(lhs, rhs, 1);
}

std::optional<poly_polynomial> poly_arithmetic::subtract(const poly_polynomial& lhs, const poly_polynomial& rhs) const {
    return combine(lhs, rhs, -1);
}

std::optional<poly_polynomial> poly_arithmetic::multiply(const poly_polynomial& lhs, const poly_polynomial& rhs) const {
    poly_polynomial product;
    for (const auto& [lhs_monomial, lhs_coefficient] : lhs.terms) {
        for (const auto& [rhs_monomial, rhs_coefficient] : rhs.terms) {
            product.terms[multiply_monomials(lhs_monomial, rhs_monomial)] += lhs_coefficient * rhs_coefficient;
        }
        if (product.terms.size() > max_terms_ * 4)
            return std::nullopt;
    }
    for (auto term = product.terms.begin(); term != product.terms.end();) {
        if (term->second == 0) { term = product.terms.erase(term); continue; }
        ++term;
    }
    if (product.terms.size() > max_terms_)
        return std::nullopt;
    return product;
}

std::optional<poly_polynomial> poly_arithmetic::power(const poly_polynomial& base, uint32_t exponent) const {
    poly_polynomial result;
    result.terms.emplace(poly_monomial{}, 1);
    for (uint32_t step = 0; step < exponent; ++step) {
        const std::optional<poly_polynomial> next = multiply(result, base);
        if (!next.has_value())
            return std::nullopt;
        result = next.value();
    }
    return result;
}

std::optional<poly_polynomial> poly_arithmetic::find_quotient(
    const poly_polynomial& dividend, const poly_polynomial& divisor) const {
    if (divisor.terms.empty())
        return std::nullopt;
    const auto& [lead_monomial, lead_coefficient] = *divisor.terms.rbegin();
    poly_polynomial quotient;
    poly_polynomial remainder = dividend;
    // Each step removes the remainder's leading term, so the loop is bounded; the cap only guards blow-up.
    for (size_t step = 0; !remainder.terms.empty(); ++step) {
        if (step > max_terms_)
            return std::nullopt;
        const poly_monomial monomial = remainder.terms.rbegin()->first;
        const integer coefficient = remainder.terms.rbegin()->second;
        if (coefficient % lead_coefficient != 0)
            return std::nullopt;
        const std::optional<poly_monomial> factor = divide_monomials(monomial, lead_monomial);
        if (!factor.has_value())
            return std::nullopt;
        poly_polynomial step_term;
        step_term.terms.emplace(factor.value(), integer(coefficient / lead_coefficient));
        const std::optional<poly_polynomial> removed = multiply(step_term, divisor);
        if (!removed.has_value())
            return std::nullopt;
        const std::optional<poly_polynomial> next = subtract(remainder, removed.value());
        if (!next.has_value())
            return std::nullopt;
        remainder = next.value();
        quotient.terms.emplace(factor.value(), integer(coefficient / lead_coefficient));
    }
    return quotient;
}

std::optional<poly_polynomial> poly_arithmetic::combine(
    const poly_polynomial& lhs, const poly_polynomial& rhs, int sign) const {
    poly_polynomial result = lhs;
    for (const auto& [monomial, coefficient] : rhs.terms) {
        integer& slot = result.terms[monomial];
        if (sign < 0) slot -= coefficient;
        else slot += coefficient;
        if (slot == 0)
            result.terms.erase(monomial);
    }
    if (result.terms.size() > max_terms_)
        return std::nullopt;
    return result;
}

std::optional<poly_polynomial> poly_arithmetic::find_gcd(const poly_polynomial& lhs, const poly_polynomial& rhs) const {
    if (lhs.terms.empty() || rhs.terms.empty())
        return std::nullopt;
    std::set<uint32_t> variables;
    for (const poly_polynomial* side : {&lhs, &rhs})
        for (const auto& [monomial, coefficient] : side->terms)
            for (const poly_power& power : monomial.powers)
                variables.insert(power.variable);
    if (variables.size() <= 1)
        return find_univariate_gcd(lhs, rhs);
    // gcd = gcd(contents) * gcd(primitive parts), both over the main variable
    const uint32_t main = *variables.rbegin();
    const std::optional<poly_polynomial> lhs_content = find_content(lhs, main);
    const std::optional<poly_polynomial> rhs_content = find_content(rhs, main);
    if (!lhs_content.has_value() || !rhs_content.has_value())
        return std::nullopt;
    const std::optional<poly_polynomial> common_content = find_gcd(lhs_content.value(), rhs_content.value());
    std::optional<poly_polynomial> a = find_quotient(lhs, lhs_content.value());
    std::optional<poly_polynomial> b = find_quotient(rhs, rhs_content.value());
    if (!common_content.has_value() || !a.has_value() || !b.has_value())
        return std::nullopt;
    if (find_degree(a.value(), main) < find_degree(b.value(), main))
        std::swap(a, b);
    // Euclid on primitive parts: a, b <- b, pp(prem(a, b)) until b is free of the main variable
    while (find_degree(b.value(), main) > 0) {
        const std::optional<poly_polynomial> remainder = find_pseudo_remainder(a.value(), b.value(), main);
        if (!remainder.has_value())
            return std::nullopt;
        a = b;
        if (remainder.value().terms.empty())
            break;
        const std::optional<poly_polynomial> content = find_content(remainder.value(), main);
        if (!content.has_value())
            return std::nullopt;
        b = find_quotient(remainder.value(), content.value());
        if (!b.has_value())
            return std::nullopt;
    }
    const bool coprime = find_degree(b.value(), main) == 0 && !(a == b);
    const poly_polynomial unit = poly_polynomial{.terms = {{poly_monomial{}, integer(1)}}};
    const std::optional<poly_polynomial> product = multiply(common_content.value(), coprime ? unit : a.value());
    if (!product.has_value())
        return std::nullopt;
    return make_primitive(product.value());
}

// The content over `variable`: the gcd of the coefficients of its powers, each a polynomial in the other variables.
std::optional<poly_polynomial> poly_arithmetic::find_content(const poly_polynomial& polynomial,
                                                             uint32_t variable) const {
    const std::vector<poly_polynomial> coefficients = split_by_degree(polynomial, variable);
    std::optional<poly_polynomial> content;
    for (const poly_polynomial& coefficient : coefficients) {
        if (coefficient.terms.empty())
            continue;
        content = content.has_value() ? find_gcd(content.value(), coefficient) : make_primitive(coefficient);
        if (!content.has_value())
            return std::nullopt;
    }
    return content;
}

// lc(divisor)^k * dividend reduced by divisor over `variable` until its degree there is below the divisor's
std::optional<poly_polynomial> poly_arithmetic::find_pseudo_remainder(const poly_polynomial& dividend,
                                                                      const poly_polynomial& divisor,
                                                                      uint32_t variable) const {
    const uint32_t divisor_degree = find_degree(divisor, variable);
    const poly_polynomial leading = split_by_degree(divisor, variable)[divisor_degree];
    poly_polynomial remainder = dividend;
    while (!remainder.terms.empty() && find_degree(remainder, variable) >= divisor_degree) {
        const uint32_t degree = find_degree(remainder, variable);
        const std::vector<poly_polynomial> coefficients = split_by_degree(remainder, variable);
        poly_polynomial shifted;
        for (const auto& [monomial, coefficient] : coefficients[degree].terms) {
            poly_monomial raised = monomial;
            if (degree > divisor_degree)
                raised = multiply_monomials(monomial, poly_monomial{.powers = {poly_power{
                    .variable = variable, .exponent = degree - divisor_degree}}});
            shifted.terms.emplace(raised, coefficient);
        }
        const std::optional<poly_polynomial> scaled = multiply(leading, remainder);
        const std::optional<poly_polynomial> removed = multiply(shifted, divisor);
        if (!scaled.has_value() || !removed.has_value())
            return std::nullopt;
        const std::optional<poly_polynomial> next = subtract(scaled.value(), removed.value());
        if (!next.has_value())
            return std::nullopt;
        remainder = next.value();
    }
    return remainder;
}

std::optional<poly_polynomial> poly_arithmetic::find_univariate_gcd(const poly_polynomial& lhs,
                                                                    const poly_polynomial& rhs) const {
    // dense coefficients over the rationals, lowest degree first, in the one variable both use
    std::optional<uint32_t> variable;
    std::vector<std::vector<rational>> dense(2);
    const poly_polynomial* sides[2] = {&lhs, &rhs};
    for (size_t side = 0; side < 2; ++side) {
        for (const auto& [monomial, coefficient] : sides[side]->terms) {
            if (monomial.powers.size() > 1)
                return std::nullopt;
            uint32_t degree = 0;
            if (monomial.powers.size() == 1) {
                const bool other = variable.has_value() && variable.value() != monomial.powers[0].variable;
                if (other)
                    return std::nullopt;
                variable = monomial.powers[0].variable;
                degree = monomial.powers[0].exponent;
            }
            if (dense[side].size() <= degree)
                dense[side].resize(degree + 1, rational(0));
            dense[side][degree] = rational(coefficient);
        }
    }
    // Euclid: a, b <- b, a mod b until b is 0
    std::vector<rational> a = dense[0];
    std::vector<rational> b = dense[1];
    while (!b.empty()) {
        while (a.size() >= b.size() && !a.empty()) {
            const rational scale = a.back() / b.back();
            const size_t shift = a.size() - b.size();
            for (size_t degree = 0; degree < b.size(); ++degree)
                a[shift + degree] -= scale * b[degree];
            while (!a.empty() && a.back() == 0)
                a.pop_back();
        }
        std::swap(a, b);
    }
    // the primitive integer multiple, leading coefficient positive
    integer common_denominator = 1;
    for (const rational& coefficient : a)
        common_denominator = boost::multiprecision::lcm(common_denominator,
                                                        boost::multiprecision::denominator(coefficient));
    integer common_factor = 0;
    for (const rational& coefficient : a)
        common_factor = boost::multiprecision::gcd(
            common_factor, boost::multiprecision::numerator(coefficient * rational(common_denominator)));
    if (a.back() < 0)
        common_factor = -common_factor;
    poly_polynomial divisor;
    for (size_t degree = 0; degree < a.size(); ++degree) {
        const rational scaled = a[degree] * rational(common_denominator) / rational(common_factor);
        if (scaled == 0)
            continue;
        poly_monomial monomial;
        if (degree > 0)
            monomial.powers.push_back(poly_power{.variable = variable.value(), .exponent = static_cast<uint32_t>(degree)});
        divisor.terms.emplace(monomial, boost::multiprecision::numerator(scaled));
    }
    return divisor;
}
