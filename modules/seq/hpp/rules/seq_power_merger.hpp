#ifndef SEQ_POWER_MERGER_HPP
#define SEQ_POWER_MERGER_HPP

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_symbols.hpp"
#include "value_objects/rewrite.hpp"
#include "value_objects/seq_symbols.hpp"

constexpr uint32_t k_seq_default_max_raised_exponent = 64;

// Local rule: the laws of powers with a symbolic exponent, each merging two powers into one:
//
// - one exponent: `exp(a, n) * exp(b, n)` is `exp(a * b, n)` (a literal product for literal bases: `exp(2, n) *
//   exp(3, n)` is `exp(6, n)`);
// - one base: `exp(a, m) * exp(a, n)` is `exp(a, m + n)`;
// - a literal power: `exp(a, n)^k` is `exp(a^k, n)` for a literal base and k at most `max_raised_exponent`
//   (`exp(2, n)^2` is `exp(4, n)`, so `2^n * 2^n - 4^n` cancels).
//
// Fires only when smaller.
template<typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor, typename IFindCost>
struct seq_power_merger {
    seq_power_merger(IFindLiteralValue& find_literal_value,
                     IMakeLiteral& make_literal,
                     IMakeFunctor& make_functor,
                     IFindCost& find_cost,
                     seq_symbols symbols,
                     poly_symbols arithmetic_symbols,
                     uint32_t max_raised_exponent = k_seq_default_max_raised_exponent);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    const expr::functor* find_power(const expr* e) const;
    const expr* multiply_bases(const expr* lhs, const expr* rhs);

    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    IFindCost& find_cost_;
    seq_symbols symbols_;
    poly_symbols arithmetic_symbols_;
    uint32_t max_raised_exponent_;
};

#define SEQ_POWER_MERGER_TEMPLATE \
    template<typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor, typename IFindCost>
#define SEQ_POWER_MERGER seq_power_merger<IFindLiteralValue, IMakeLiteral, IMakeFunctor, IFindCost>

SEQ_POWER_MERGER_TEMPLATE SEQ_POWER_MERGER::seq_power_merger(IFindLiteralValue& find_literal_value,
                                                             IMakeLiteral& make_literal,
                                                             IMakeFunctor& make_functor,
                                                             IFindCost& find_cost,
                                                             seq_symbols symbols,
                                                             poly_symbols arithmetic_symbols,
                                                             uint32_t max_raised_exponent)
    : find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , find_cost_(find_cost)
    , symbols_(symbols)
    , arithmetic_symbols_(arithmetic_symbols)
    , max_raised_exponent_(max_raised_exponent) {}

SEQ_POWER_MERGER_TEMPLATE void SEQ_POWER_MERGER::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.size() != 2)
        return;
    const expr::functor* left = find_power(f->args[0]);
    if (left == nullptr)
        return;
    const expr* merged = nullptr;
    const expr::functor* right = f->id == arithmetic_symbols_.mul ? find_power(f->args[1]) : nullptr;
    if (right != nullptr && left->args[1] == right->args[1])
        merged = make_functor_.make_functor(symbols_.power,
                                            {multiply_bases(left->args[0], right->args[0]), left->args[1]});
    if (right != nullptr && merged == nullptr && left->args[0] == right->args[0])
        merged = make_functor_.make_functor(
            symbols_.power,
            {left->args[0], make_functor_.make_functor(arithmetic_symbols_.add, {left->args[1], right->args[1]})});
    const bool raised = f->id == arithmetic_symbols_.pow;
    const std::optional<integer> base = raised ? find_literal_value_.find_value(left->args[0]) : std::nullopt;
    const std::optional<integer> exponent = raised ? find_literal_value_.find_value(f->args[1]) : std::nullopt;
    const bool literal_power = base.has_value() && exponent.has_value() && exponent.value() >= 0
                               && exponent.value() <= max_raised_exponent_;
    if (literal_power)
        merged = make_functor_.make_functor(
            symbols_.power,
            {make_literal_.make_literal(boost::multiprecision::pow(base.value(),
                                                                   static_cast<unsigned>(exponent.value()))),
             left->args[1]});
    if (merged == nullptr || find_cost_.cost(merged) >= find_cost_.cost(e))
        return;
    out.push_back(ready(merged));
}

// e as `exp(a, n)`, or nullptr
SEQ_POWER_MERGER_TEMPLATE const expr::functor* SEQ_POWER_MERGER::find_power(const expr* e) const {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    const bool power = f != nullptr && f->id == symbols_.power && f->args.size() == 2;
    return power ? f : nullptr;
}

// a * b, a literal when both are
SEQ_POWER_MERGER_TEMPLATE const expr* SEQ_POWER_MERGER::multiply_bases(const expr* lhs, const expr* rhs) {
    const std::optional<integer> left = find_literal_value_.find_value(lhs);
    const std::optional<integer> right = find_literal_value_.find_value(rhs);
    if (left.has_value() && right.has_value())
        return make_literal_.make_literal(left.value() * right.value());
    return make_functor_.make_functor(arithmetic_symbols_.mul, {lhs, rhs});
}

#undef SEQ_POWER_MERGER
#undef SEQ_POWER_MERGER_TEMPLATE

#endif
