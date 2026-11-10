#ifndef SEQ_LINEAR_CLOSER_HPP
#define SEQ_LINEAR_CLOSER_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_symbols.hpp"
#include "value_objects/rewrite.hpp"
#include "value_objects/seq_symbols.hpp"

constexpr size_t k_seq_default_max_root_trials = 10000;

// Local rule: closes a linear recurrence with constant coefficients, `rec(lams(k + 1, s), a(0), ..., a(k-1), n)` whose
// step is `c1 * #1 + ... + ck * #k + p(#0)` (p a polynomial in the index, possibly 0), into its closed form
// `A1 * exp(r1, n) + ... + Am * exp(rm, n) + q(n)` (`5 * #1 - 6 * #2` from 2, 5 is `exp(2, n) + exp(3, n)`). It applies
// when the characteristic polynomial `t^k - c1 t^(k-1) - ... - ck` splits into integer roots, each other than 1
// simple (found among the divisors of ck, at most `max_root_trials`), and the coefficients are integers. The powers'
// bases are the roots other than 1; q has degree deg p plus the multiplicity of 1. The coefficients are solved exactly
// from the terms the recurrence gives, on more terms than they need: agreeing on k + deg q + 1 consecutive terms means
// the closed form satisfies the step there, so everywhere. Fires only when the closed form holds a power (so it has
// no value at a negative index, as the recurrence has none) and is smaller than the recurrence. An affine step of
// order one is left to `seq_power_closer`.
template<typename IFindBody, typename IFindPolynomial, typename IFindLiteralValue, typename IMakeLiteral,
         typename IMakeFunctor, typename ISolveLinearSystem, typename IFindCost>
struct seq_linear_closer {
    seq_linear_closer(IFindBody& find_body,
                      IFindPolynomial& find_polynomial,
                      IFindLiteralValue& find_literal_value,
                      IMakeLiteral& make_literal,
                      IMakeFunctor& make_functor,
                      ISolveLinearSystem& solve_linear_system,
                      IFindCost& find_cost,
                      seq_symbols symbols,
                      poly_symbols arithmetic_symbols,
                      size_t max_root_trials = k_seq_default_max_root_trials);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    std::optional<std::vector<integer>> find_roots(const std::vector<integer>& coefficients,
                                                   size_t& multiplicity_of_one);
    const expr* build(const std::vector<integer>& roots, const std::vector<integer>& amplitudes,
                      const std::vector<integer>& polynomial, const expr* index);
    const expr* add_term(const expr* sum, const integer& coefficient, const expr* term);

    IFindBody& find_body_;
    IFindPolynomial& find_polynomial_;
    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    ISolveLinearSystem& solve_linear_system_;
    IFindCost& find_cost_;
    seq_symbols symbols_;
    poly_symbols arithmetic_symbols_;
    size_t max_root_trials_;
};

#define SEQ_LINEAR_CLOSER_TEMPLATE                                                                              \
    template<typename IFindBody, typename IFindPolynomial, typename IFindLiteralValue, typename IMakeLiteral,  \
             typename IMakeFunctor, typename ISolveLinearSystem, typename IFindCost>
#define SEQ_LINEAR_CLOSER                                                                                      \
    seq_linear_closer<IFindBody, IFindPolynomial, IFindLiteralValue, IMakeLiteral, IMakeFunctor,               \
                      ISolveLinearSystem, IFindCost>

SEQ_LINEAR_CLOSER_TEMPLATE SEQ_LINEAR_CLOSER::seq_linear_closer(IFindBody& find_body,
                                                                IFindPolynomial& find_polynomial,
                                                                IFindLiteralValue& find_literal_value,
                                                                IMakeLiteral& make_literal,
                                                                IMakeFunctor& make_functor,
                                                                ISolveLinearSystem& solve_linear_system,
                                                                IFindCost& find_cost,
                                                                seq_symbols symbols,
                                                                poly_symbols arithmetic_symbols,
                                                                size_t max_root_trials)
    : find_body_(find_body)
    , find_polynomial_(find_polynomial)
    , find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , solve_linear_system_(solve_linear_system)
    , find_cost_(find_cost)
    , symbols_(symbols)
    , arithmetic_symbols_(arithmetic_symbols)
    , max_root_trials_(max_root_trials) {}

SEQ_LINEAR_CLOSER_TEMPLATE void SEQ_LINEAR_CLOSER::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->id != symbols_.recurrence || f->args.size() < 3)
        return;
    const size_t order = f->args.size() - 2;
    if (order > symbols_.previous.size())
        return;
    std::vector<integer> terms;
    for (size_t base = 1; base <= order; ++base) {
        const std::optional<integer> value = find_literal_value_.find_value(f->args[base]);
        if (!value.has_value())
            return;
        terms.push_back(value.value());
    }
    const std::optional<const expr*> body = find_body_.find_body(f->args[0], order + 1);
    if (!body.has_value())
        return;
    const auto step = find_polynomial_.find_polynomial(body.value());
    if (!step.has_value())
        return;
    // the step as `c1 * #1 + ... + ck * #k + p(#0)`
    std::vector<integer> coefficients(order, integer(0));
    std::vector<integer> inhomogeneous;
    for (const auto& [monomial, coefficient] : step.value().terms) {
        if (monomial.powers.empty()) {
            inhomogeneous.resize(std::max<size_t>(inhomogeneous.size(), 1), integer(0));
            inhomogeneous[0] = coefficient;
            continue;
        }
        if (monomial.powers.size() != 1)
            return;
        const uint32_t variable = monomial.powers[0].variable;
        const uint32_t exponent = monomial.powers[0].exponent;
        if (variable == symbols_.index) {
            inhomogeneous.resize(std::max<size_t>(inhomogeneous.size(), exponent + 1), integer(0));
            inhomogeneous[exponent] = coefficient;
            continue;
        }
        bool linear = false;
        for (size_t back = 0; back < order && !linear; ++back) {
            if (variable != symbols_.previous[back] || exponent != 1)
                continue;
            coefficients[back] = coefficient;
            linear = true;
        }
        if (!linear)
            return;
    }
    const bool affine_order_one = order == 1 && inhomogeneous.size() <= 1;
    if (affine_order_one || coefficients.back() == 0)
        return;
    size_t multiplicity_of_one = 0;
    const std::optional<std::vector<integer>> roots = find_roots(coefficients, multiplicity_of_one);
    if (!roots.has_value() || roots.value().empty())
        return;
    // the basis: a power per root other than 1, then the index's powers up to the degree of q
    const size_t degree_count = inhomogeneous.size() + multiplicity_of_one;
    const size_t unknowns = roots.value().size() + degree_count;
    const size_t equations = unknowns + order + degree_count + 1;
    while (terms.size() < equations) {
        const integer index = static_cast<long>(terms.size());
        integer next = 0;
        for (size_t back = 0; back < order; ++back)
            next += coefficients[back] * terms[terms.size() - 1 - back];
        integer power = 1;
        for (const integer& coefficient : inhomogeneous) {
            next += coefficient * power;
            power *= index;
        }
        terms.push_back(next);
    }
    std::vector<std::vector<integer>> rows;
    for (size_t index = 0; index < equations; ++index) {
        std::vector<integer> row;
        for (const integer& root : roots.value())
            row.push_back(boost::multiprecision::pow(root, static_cast<unsigned>(index)));
        integer power = 1;
        for (size_t degree = 0; degree < degree_count; ++degree) {
            row.push_back(power);
            power *= static_cast<long>(index);
        }
        row.push_back(terms[index]);
        rows.push_back(row);
    }
    const std::optional<std::vector<integer>> solution = solve_linear_system_.solve(rows, unknowns);
    if (!solution.has_value())
        return;
    const std::vector<integer> amplitudes(solution.value().begin(), solution.value().begin() + roots.value().size());
    const std::vector<integer> polynomial(solution.value().begin() + roots.value().size(), solution.value().end());
    bool powered = false;
    for (const integer& amplitude : amplitudes)
        powered = powered || amplitude != 0;
    if (!powered)
        return;
    const expr* closed = build(roots.value(), amplitudes, polynomial, f->args.back());
    if (find_cost_.cost(closed) >= find_cost_.cost(e))
        return;
    out.push_back([closed]() { return closed; });
}

// The roots of `t^k - c1 t^(k-1) - ... - ck` other than 1, each simple, when every root is an integer; nothing else.
SEQ_LINEAR_CLOSER_TEMPLATE std::optional<std::vector<integer>> SEQ_LINEAR_CLOSER::find_roots(
    const std::vector<integer>& coefficients, size_t& multiplicity_of_one) {
    // highest degree first: 1, -c1, ..., -ck
    std::vector<integer> remaining{1};
    for (const integer& coefficient : coefficients)
        remaining.push_back(-coefficient);
    std::vector<integer> roots;
    const integer constant = boost::multiprecision::abs(remaining.back());
    size_t trials = 0;
    for (integer divisor = 1; divisor <= constant && remaining.size() > 1; ++divisor) {
        if (++trials > max_root_trials_)
            return std::nullopt;
        if (constant % divisor != 0)
            continue;
        for (const integer& root : {divisor, integer(-divisor)}) {
            // divide by (t - root) as often as it goes
            while (remaining.size() > 1) {
                std::vector<integer> quotient{remaining[0]};
                for (size_t position = 1; position + 1 < remaining.size(); ++position)
                    quotient.push_back(remaining[position] + root * quotient.back());
                const bool divides = remaining.back() + root * quotient.back() == 0;
                if (!divides)
                    break;
                remaining = quotient;
                if (root == 1) {
                    ++multiplicity_of_one;
                    continue;
                }
                const bool repeated = !roots.empty() && roots.back() == root;
                if (repeated)
                    return std::nullopt;
                roots.push_back(root);
            }
        }
    }
    if (remaining.size() > 1)
        return std::nullopt;
    return roots;
}

SEQ_LINEAR_CLOSER_TEMPLATE const expr* SEQ_LINEAR_CLOSER::build(const std::vector<integer>& roots,
                                                                const std::vector<integer>& amplitudes,
                                                                const std::vector<integer>& polynomial,
                                                                const expr* index) {
    // the powers with positive amplitudes first, so a negative one is subtracted
    const expr* sum = nullptr;
    for (const bool positive : {true, false}) {
        for (size_t position = 0; position < roots.size(); ++position) {
            if ((amplitudes[position] > 0) != positive)
                continue;
            const expr* base = make_literal_.make_literal(roots[position]);
            sum = add_term(sum, amplitudes[position], make_functor_.make_functor(symbols_.power, {base, index}));
        }
    }
    for (size_t degree = polynomial.size(); degree-- > 1;) {
        const expr* power = degree == 1 ? index
                                        : make_functor_.make_functor(
                                              arithmetic_symbols_.pow,
                                              {index, make_literal_.make_literal(integer(static_cast<long>(degree)))});
        sum = add_term(sum, polynomial[degree], power);
    }
    if (!polynomial.empty())
        sum = add_term(sum, polynomial[0], nullptr);
    return sum;
}

// sum + coefficient * term (just the coefficient when term is null), written with `sub` for a negative coefficient
SEQ_LINEAR_CLOSER_TEMPLATE const expr* SEQ_LINEAR_CLOSER::add_term(const expr* sum, const integer& coefficient,
                                                                   const expr* term) {
    if (coefficient == 0)
        return sum;
    const bool negative = coefficient < 0 && sum != nullptr;
    const integer magnitude = negative ? integer(-coefficient) : coefficient;
    const expr* scaled = term == nullptr ? make_literal_.make_literal(magnitude)
                         : magnitude == 1
                             ? term
                             : make_functor_.make_functor(arithmetic_symbols_.mul,
                                                          {make_literal_.make_literal(magnitude), term});
    if (sum == nullptr)
        return scaled;
    return make_functor_.make_functor(negative ? arithmetic_symbols_.sub : arithmetic_symbols_.add, {sum, scaled});
}

#undef SEQ_LINEAR_CLOSER
#undef SEQ_LINEAR_CLOSER_TEMPLATE

#endif
