#include "infrastructure/poly_linear_factor_finder.hpp"
#include <set>

poly_linear_factor_finder::poly_linear_factor_finder(size_t max_candidates)
    : max_candidates_(max_candidates) {}

std::vector<poly_polynomial> poly_linear_factor_finder::find_linear_factors(const poly_polynomial& polynomial) const {
    std::vector<poly_polynomial> candidates;
    const auto constant = polynomial.terms.find(poly_monomial{});
    if (constant == polynomial.terms.end())
        return candidates;
    // variable -> its restriction, as exponent -> coefficient
    std::map<uint32_t, std::map<uint32_t, integer>> restrictions;
    for (const auto& [monomial, coefficient] : polynomial.terms) {
        if (monomial.powers.size() != 1)
            continue;
        restrictions[monomial.powers.front().variable][monomial.powers.front().exponent] = coefficient;
    }
    std::vector<uint32_t> variables;
    std::vector<std::vector<rational>> roots;
    for (auto& [variable, restriction] : restrictions) {
        restriction[0] = constant->second;
        variables.push_back(variable);
        roots.push_back(find_rational_roots(restriction));
    }
    // Odometer over one root (index >= 1) or none (index 0) per variable.
    std::vector<size_t> choice(variables.size(), 0);
    std::set<poly_polynomial> seen;
    while (candidates.size() < max_candidates_) {
        size_t digit = 0;
        while (digit < choice.size() && ++choice[digit] > roots[digit].size()) {
            choice[digit] = 0;
            ++digit;
        }
        if (digit == choice.size())
            break;
        // a_v = -d * q / p for root p/q; the smallest d that makes every a_v an integer is lcm of the |p|.
        integer d = 1;
        for (size_t i = 0; i < variables.size(); ++i)
            if (choice[i] > 0)
                d = boost::multiprecision::lcm(d, integer(boost::multiprecision::abs(roots[i][choice[i] - 1].numerator)));
        poly_polynomial linear;
        integer gcd = d;
        for (size_t i = 0; i < variables.size(); ++i) {
            if (choice[i] == 0)
                continue;
            const rational& root = roots[i][choice[i] - 1];
            const integer a = -d / root.numerator * root.denominator;
            gcd = boost::multiprecision::gcd(gcd, a);
            linear.terms.emplace(poly_monomial{.powers = {poly_power{.variable = variables[i], .exponent = 1}}}, a);
        }
        linear.terms.emplace(poly_monomial{}, d);
        const int sign = linear.terms.rbegin()->second < 0 ? -1 : 1;
        for (auto& [monomial, coefficient] : linear.terms)
            coefficient = coefficient / gcd * sign;
        if (!seen.insert(linear).second)
            continue;
        candidates.push_back(linear);
    }
    return candidates;
}

// Roots p/q in lowest terms of the univariate integer polynomial sum c_k t^k (c_0 != 0): p divides c_0 and q
// divides the leading coefficient. Checked exactly as sum c_k p^k q^(n-k) == 0.
std::vector<poly_linear_factor_finder::rational> poly_linear_factor_finder::find_rational_roots(
    const std::map<uint32_t, integer>& coefficients) {
    std::vector<rational> roots;
    const uint32_t degree = coefficients.rbegin()->first;
    if (degree == 0)
        return roots;
    for (const integer& p_magnitude : find_divisors(coefficients.at(0))) {
        for (const integer& q : find_divisors(coefficients.rbegin()->second)) {
            if (boost::multiprecision::gcd(p_magnitude, q) != 1)
                continue;
            for (const integer& p : {p_magnitude, integer(-p_magnitude)}) {
                integer sum = 0;
                for (const auto& [exponent, coefficient] : coefficients)
                    sum += coefficient * boost::multiprecision::pow(p, exponent)
                         * boost::multiprecision::pow(q, degree - exponent);
                if (sum != 0)
                    continue;
                roots.push_back(rational{.numerator = p, .denominator = q});
            }
        }
    }
    return roots;
}

// Positive divisors of |value| up to 100000, so a huge coefficient cannot stall the search. A root whose numerator
// or denominator is larger is missed.
std::vector<integer> poly_linear_factor_finder::find_divisors(const integer& value) {
    std::vector<integer> divisors;
    const integer magnitude = boost::multiprecision::abs(value);
    for (integer candidate = 1; candidate <= magnitude && candidate <= 100000; ++candidate)
        if (magnitude % candidate == 0)
            divisors.push_back(candidate);
    return divisors;
}
