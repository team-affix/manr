#ifndef POLY_LINEAR_FACTOR_FINDER_HPP
#define POLY_LINEAR_FACTOR_FINDER_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>
#include "infrastructure/poly_arithmetic.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_polynomial.hpp"

constexpr size_t k_poly_default_max_linear_candidates = 64;
constexpr uint64_t k_poly_default_max_root_divisor = 100000;
constexpr size_t k_poly_default_linear_shift_points = 3;

// Candidate linear factors `a1*v1 + ... + ak*vk + d` (d != 0) of a multivariate polynomial, found by restriction.
// If L divides P, setting every variable but v to zero turns L into `a_v * v + d`, which divides the univariate
// restriction P_v; so each root r_v of P_v (rational root theorem) fixes a_v / d = -1/r_v. Likewise, restricted to
// two variables u and w, L becomes `a_u * u + a_w * w + d`, which must divide P restricted to {u, w}.
//
// A factor is grown one variable at a time, backtracking: each variable joins with one of its roots, or stays out.
// Both choices are checked against every variable already in, on P restricted to that pair: a root joins only if
// the pair's `a_u * u + a_w * w + d` exactly divides it, and a variable stays out only if `a_u * u + d` does (as it
// must when w is not in L). Wrong pairings almost never pass, so the search follows the real factors instead of
// trying every combination or every subset of one. Candidates still need the full exact division.
//
// Restriction needs a nonzero constant term: a factor through the origin restricts to `a_v * v`, whose only root is
// zero. A polynomial with no constant term is first moved so a point s with P(s) != 0 becomes the origin
// (v -> v + s_v, done exactly by binomial expansion); its factors are found there and moved back, which keeps them
// linear and changes only their constant terms.
template<typename IFindQuotient>
struct poly_linear_factor_finder {
    poly_linear_factor_finder(IFindQuotient& find_quotient,
                              size_t max_candidates = k_poly_default_max_linear_candidates,
                              uint64_t max_divisor = k_poly_default_max_root_divisor,
                              size_t shift_points = k_poly_default_linear_shift_points,
                              size_t max_shifted_terms = k_poly_default_max_terms);
    std::vector<poly_polynomial> find_linear_factors(const poly_polynomial& polynomial);
private:
    struct rational {
        integer numerator;
        integer denominator;
    };
    struct member {
        uint32_t variable;
        rational root;
    };
    struct search {
        const poly_polynomial& polynomial;
        std::vector<uint32_t> variables;
        std::vector<std::vector<rational>> roots;
        std::map<std::pair<uint32_t, uint32_t>, poly_polynomial> pair_restrictions;
        std::vector<member> members;
        std::set<poly_polynomial> seen;
        std::vector<poly_polynomial> candidates;
    };

    std::vector<poly_polynomial> find_around_origin(const poly_polynomial& polynomial);
    std::optional<poly_polynomial> shift(const poly_polynomial& polynomial, const std::map<uint32_t, integer>& point) const;
    static std::map<uint32_t, integer> make_shift_point(const poly_polynomial& polynomial, size_t index);
    static integer evaluate(const poly_polynomial& polynomial, const std::map<uint32_t, integer>& point);
    static poly_polynomial unshift(const poly_polynomial& linear, const std::map<uint32_t, integer>& point);
    void extend(search& state, size_t next);
    bool fits_pair(search& state, const member& lhs, const member& rhs);
    bool fits_without(search& state, const member& existing, uint32_t absent);
    const poly_polynomial& find_pair_restriction(search& state, uint32_t lhs, uint32_t rhs);
    static poly_polynomial make_linear(const std::vector<member>& members);
    static poly_polynomial restrict_to(const poly_polynomial& polynomial, uint32_t lhs, uint32_t rhs);
    std::vector<rational> find_rational_roots(const std::map<uint32_t, integer>& coefficients) const;
    std::vector<integer> find_divisors(const integer& value) const;

    IFindQuotient& find_quotient_;
    size_t max_candidates_;
    uint64_t max_divisor_;
    size_t shift_points_;
    size_t max_shifted_terms_;
};

#define POLY_LINEAR_FACTOR_FINDER_TEMPLATE template<typename IFindQuotient>
#define POLY_LINEAR_FACTOR_FINDER          poly_linear_factor_finder<IFindQuotient>

POLY_LINEAR_FACTOR_FINDER_TEMPLATE POLY_LINEAR_FACTOR_FINDER::poly_linear_factor_finder(
    IFindQuotient& find_quotient, size_t max_candidates, uint64_t max_divisor, size_t shift_points,
    size_t max_shifted_terms)
    : find_quotient_(find_quotient)
    , max_candidates_(max_candidates)
    , max_divisor_(max_divisor)
    , shift_points_(shift_points)
    , max_shifted_terms_(max_shifted_terms) {}

POLY_LINEAR_FACTOR_FINDER_TEMPLATE std::vector<poly_polynomial> POLY_LINEAR_FACTOR_FINDER::find_linear_factors(
    const poly_polynomial& polynomial) {
    if (polynomial.terms.contains(poly_monomial{}))
        return find_around_origin(polynomial);
    for (size_t index = 0; index < shift_points_; ++index) {
        const std::map<uint32_t, integer> point = make_shift_point(polynomial, index);
        if (evaluate(polynomial, point) == 0)
            continue;
        const std::optional<poly_polynomial> shifted = shift(polynomial, point);
        if (!shifted.has_value())
            return {};
        std::vector<poly_polynomial> candidates;
        for (const poly_polynomial& candidate : find_around_origin(shifted.value()))
            candidates.push_back(unshift(candidate, point));
        return candidates;
    }
    return {};
}

POLY_LINEAR_FACTOR_FINDER_TEMPLATE std::vector<poly_polynomial> POLY_LINEAR_FACTOR_FINDER::find_around_origin(
    const poly_polynomial& polynomial) {
    const auto constant = polynomial.terms.find(poly_monomial{});
    // variable -> its restriction, as exponent -> coefficient
    std::map<uint32_t, std::map<uint32_t, integer>> restrictions;
    for (const auto& [monomial, coefficient] : polynomial.terms) {
        if (monomial.powers.size() != 1)
            continue;
        restrictions[monomial.powers.front().variable][monomial.powers.front().exponent] = coefficient;
    }
    search state{.polynomial = polynomial, .variables = {}, .roots = {}, .pair_restrictions = {}, .members = {},
                 .seen = {}, .candidates = {}};
    for (auto& [variable, restriction] : restrictions) {
        restriction[0] = constant->second;
        state.variables.push_back(variable);
        state.roots.push_back(find_rational_roots(restriction));
    }
    extend(state, 0);
    return state.candidates;
}

// P(v + s): each term c * prod v^e becomes c * prod sum_k C(e, k) v^k s^(e - k). Nothing if it grows past
// `max_shifted_terms`.
POLY_LINEAR_FACTOR_FINDER_TEMPLATE std::optional<poly_polynomial> POLY_LINEAR_FACTOR_FINDER::shift(
    const poly_polynomial& polynomial, const std::map<uint32_t, integer>& point) const {
    poly_polynomial shifted;
    for (const auto& [monomial, coefficient] : polynomial.terms) {
        // the term's expansion so far, as monomial -> coefficient
        std::map<poly_monomial, integer, poly_monomial_order> partial{{poly_monomial{}, coefficient}};
        for (const poly_power& power : monomial.powers) {
            std::map<poly_monomial, integer, poly_monomial_order> next;
            const integer& offset = point.at(power.variable);
            integer binomial = 1;
            for (uint32_t k = 0; k <= power.exponent; ++k) {
                const integer factor = binomial * boost::multiprecision::pow(offset, power.exponent - k);
                for (const auto& [existing, value] : partial) {
                    poly_monomial grown = existing;
                    if (k > 0)
                        grown.powers.push_back(poly_power{.variable = power.variable, .exponent = k});
                    next[grown] += value * factor;
                }
                binomial = binomial * (power.exponent - k) / (k + 1);
            }
            partial = std::move(next);
        }
        for (const auto& [term, value] : partial) {
            integer& total = shifted.terms[term];
            total += value;
            if (total == 0)
                shifted.terms.erase(term);
        }
        if (shifted.terms.size() > max_shifted_terms_)
            return std::nullopt;
    }
    return shifted;
}

// The index-th point to move the origin to: small nonzero values, varied per variable so a factor is unlikely to
// vanish at every one.
POLY_LINEAR_FACTOR_FINDER_TEMPLATE std::map<uint32_t, integer> POLY_LINEAR_FACTOR_FINDER::make_shift_point(
    const poly_polynomial& polynomial, size_t index) {
    std::map<uint32_t, integer> point;
    for (const auto& [monomial, coefficient] : polynomial.terms)
        for (const poly_power& power : monomial.powers)
            point.emplace(power.variable, 0);
    size_t position = 0;
    for (auto& [variable, value] : point) {
        value = 1 + (position * index) % (index + 2);
        if (index % 2 == 1 && position % 2 == 1)
            value = -value;
        ++position;
    }
    return point;
}

POLY_LINEAR_FACTOR_FINDER_TEMPLATE integer POLY_LINEAR_FACTOR_FINDER::evaluate(
    const poly_polynomial& polynomial, const std::map<uint32_t, integer>& point) {
    integer sum = 0;
    for (const auto& [monomial, coefficient] : polynomial.terms) {
        integer term = coefficient;
        for (const poly_power& power : monomial.powers)
            term *= boost::multiprecision::pow(point.at(power.variable), power.exponent);
        sum += term;
    }
    return sum;
}

// L(v - s) for a linear L found around s: the same coefficients, with constant d - sum a_v s_v. Content and sign
// are unchanged (a common divisor of the a_v and either constant divides the other).
POLY_LINEAR_FACTOR_FINDER_TEMPLATE poly_polynomial POLY_LINEAR_FACTOR_FINDER::unshift(
    const poly_polynomial& linear, const std::map<uint32_t, integer>& point) {
    poly_polynomial moved;
    integer constant = 0;
    for (const auto& [monomial, coefficient] : linear.terms) {
        if (monomial.powers.empty()) {
            constant += coefficient;
            continue;
        }
        moved.terms.emplace(monomial, coefficient);
        constant -= coefficient * point.at(monomial.powers.front().variable);
    }
    if (constant != 0)
        moved.terms.emplace(poly_monomial{}, constant);
    return moved;
}

// Decides variables[next..]: each joins with one of its roots (if it fits every member so far) or stays out.
POLY_LINEAR_FACTOR_FINDER_TEMPLATE void POLY_LINEAR_FACTOR_FINDER::extend(search& state, size_t next) {
    if (state.candidates.size() == max_candidates_)
        return;
    if (next == state.variables.size()) {
        if (state.members.empty())
            return;
        poly_polynomial linear = make_linear(state.members);
        if (!state.seen.insert(linear).second)
            return;
        state.candidates.push_back(std::move(linear));
        return;
    }
    for (const rational& root : state.roots[next]) {
        const member joining{.variable = state.variables[next], .root = root};
        bool fits = true;
        for (const member& existing : state.members) {
            fits = fits_pair(state, existing, joining);
            if (!fits)
                break;
        }
        // The first member also checks every variable skipped before it, which had no member to check against.
        for (size_t skipped = 0; fits && state.members.empty() && skipped < next; ++skipped)
            fits = fits_without(state, joining, state.variables[skipped]);
        if (!fits)
            continue;
        state.members.push_back(joining);
        extend(state, next + 1);
        state.members.pop_back();
    }
    for (const member& existing : state.members)
        if (!fits_without(state, existing, state.variables[next]))
            return;
    extend(state, next + 1);
}

// Whether `a_l * l + a_r * r + d` exactly divides the polynomial restricted to {l, r}.
POLY_LINEAR_FACTOR_FINDER_TEMPLATE bool POLY_LINEAR_FACTOR_FINDER::fits_pair(
    search& state, const member& lhs, const member& rhs) {
    const poly_polynomial& restricted = find_pair_restriction(state, lhs.variable, rhs.variable);
    return find_quotient_.find_quotient(restricted, make_linear({lhs, rhs})).has_value();
}

// Whether `a_e * e + d` exactly divides the polynomial restricted to {e, absent}, as it must if `absent` is not in L.
POLY_LINEAR_FACTOR_FINDER_TEMPLATE bool POLY_LINEAR_FACTOR_FINDER::fits_without(
    search& state, const member& existing, uint32_t absent) {
    const poly_polynomial& restricted = find_pair_restriction(state, existing.variable, absent);
    return find_quotient_.find_quotient(restricted, make_linear({existing})).has_value();
}

POLY_LINEAR_FACTOR_FINDER_TEMPLATE const poly_polynomial& POLY_LINEAR_FACTOR_FINDER::find_pair_restriction(
    search& state, uint32_t lhs, uint32_t rhs) {
    const std::pair<uint32_t, uint32_t> key{lhs, rhs};
    auto restricted = state.pair_restrictions.find(key);
    if (restricted == state.pair_restrictions.end())
        restricted = state.pair_restrictions.emplace(key, restrict_to(state.polynomial, lhs, rhs)).first;
    return restricted->second;
}

// The members' linear form: a_v = -d * q / p for root p/q, with d the lcm of the |p| so every a_v is an integer,
// then divided by its content and signed so the leading coefficient is positive.
POLY_LINEAR_FACTOR_FINDER_TEMPLATE poly_polynomial POLY_LINEAR_FACTOR_FINDER::make_linear(
    const std::vector<member>& members) {
    integer d = 1;
    for (const member& m : members)
        d = boost::multiprecision::lcm(d, integer(boost::multiprecision::abs(m.root.numerator)));
    poly_polynomial linear;
    integer gcd = d;
    for (const member& m : members) {
        const integer a = -d / m.root.numerator * m.root.denominator;
        gcd = boost::multiprecision::gcd(gcd, a);
        linear.terms.emplace(poly_monomial{.powers = {poly_power{.variable = m.variable, .exponent = 1}}}, a);
    }
    linear.terms.emplace(poly_monomial{}, d);
    const int sign = linear.terms.rbegin()->second < 0 ? -1 : 1;
    for (auto& [monomial, coefficient] : linear.terms)
        coefficient = coefficient / gcd * sign;
    return linear;
}

// The terms whose variables are all among {lhs, rhs}: the polynomial with every other variable set to zero.
POLY_LINEAR_FACTOR_FINDER_TEMPLATE poly_polynomial POLY_LINEAR_FACTOR_FINDER::restrict_to(
    const poly_polynomial& polynomial, uint32_t lhs, uint32_t rhs) {
    poly_polynomial restricted;
    for (const auto& [monomial, coefficient] : polynomial.terms) {
        bool inside = true;
        for (const poly_power& power : monomial.powers)
            inside = inside && (power.variable == lhs || power.variable == rhs);
        if (!inside)
            continue;
        restricted.terms.emplace(monomial, coefficient);
    }
    return restricted;
}

// Roots p/q in lowest terms of the univariate integer polynomial sum c_k t^k (c_0 != 0): p divides c_0 and q
// divides the leading coefficient. Checked exactly as sum c_k p^k q^(n-k) == 0.
POLY_LINEAR_FACTOR_FINDER_TEMPLATE std::vector<typename POLY_LINEAR_FACTOR_FINDER::rational>
POLY_LINEAR_FACTOR_FINDER::find_rational_roots(const std::map<uint32_t, integer>& coefficients) const {
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

// Positive divisors of |value| up to `max_divisor`, so a huge coefficient cannot stall the search. A root whose
// numerator or denominator is larger is missed.
POLY_LINEAR_FACTOR_FINDER_TEMPLATE std::vector<integer> POLY_LINEAR_FACTOR_FINDER::find_divisors(
    const integer& value) const {
    std::vector<integer> divisors;
    const integer magnitude = boost::multiprecision::abs(value);
    for (integer candidate = 1; candidate <= magnitude && candidate <= max_divisor_; ++candidate)
        if (magnitude % candidate == 0)
            divisors.push_back(candidate);
    return divisors;
}

#undef POLY_LINEAR_FACTOR_FINDER
#undef POLY_LINEAR_FACTOR_FINDER_TEMPLATE

#endif
