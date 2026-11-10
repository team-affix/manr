#ifndef NUM_EVALUATOR_HPP
#define NUM_EVALUATOR_HPP

#include <optional>
#include <variant>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/num_symbols.hpp"

// The value of an expression at an input with the number-theory primitives on top of another domain: `prime(a)` and
// `is_prime(a)` take the value of `a` (through this evaluator, so they nest) and give the n-th prime or 1/0, and as a
// condition `is_prime(a)` holds where `a` is prime. Everything else goes to the inner domain, whose own expressions
// cannot hold a primitive (`prime(#0) + 1` has no value). No value past the prime table's range.
template<typename IEvaluateInnerAt,
         typename IHoldsInnerAt,
         typename IFindLiteralValue,
         typename IMakeLiteral,
         typename IFindNthPrime,
         typename IFindPrimality>
struct num_evaluator {
    num_evaluator(IEvaluateInnerAt& evaluate_inner_at,
                  IHoldsInnerAt& holds_inner_at,
                  IFindLiteralValue& find_literal_value,
                  IMakeLiteral& make_literal,
                  IFindNthPrime& find_nth_prime,
                  IFindPrimality& find_primality,
                  num_symbols symbols);
    std::optional<const expr*> evaluate_at(const expr* e, const expr* input);
    std::optional<bool> holds_at(const expr* condition, const expr* input);
private:
    std::optional<integer> find_argument(const expr* e, const expr* input);

    IEvaluateInnerAt& evaluate_inner_at_;
    IHoldsInnerAt& holds_inner_at_;
    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IFindNthPrime& find_nth_prime_;
    IFindPrimality& find_primality_;
    num_symbols symbols_;
};

#define NUM_EVALUATOR_TEMPLATE                                                                              \
    template<typename IEvaluateInnerAt, typename IHoldsInnerAt, typename IFindLiteralValue, typename IMakeLiteral, \
             typename IFindNthPrime, typename IFindPrimality>
#define NUM_EVALUATOR \
    num_evaluator<IEvaluateInnerAt, IHoldsInnerAt, IFindLiteralValue, IMakeLiteral, IFindNthPrime, IFindPrimality>

NUM_EVALUATOR_TEMPLATE NUM_EVALUATOR::num_evaluator(IEvaluateInnerAt& evaluate_inner_at,
                                                     IHoldsInnerAt& holds_inner_at,
                                                     IFindLiteralValue& find_literal_value,
                                                     IMakeLiteral& make_literal,
                                                     IFindNthPrime& find_nth_prime,
                                                     IFindPrimality& find_primality,
                                                     num_symbols symbols)
    : evaluate_inner_at_(evaluate_inner_at)
    , holds_inner_at_(holds_inner_at)
    , find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , find_nth_prime_(find_nth_prime)
    , find_primality_(find_primality)
    , symbols_(symbols) {}

NUM_EVALUATOR_TEMPLATE std::optional<const expr*> NUM_EVALUATOR::evaluate_at(const expr* e, const expr* input) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    const bool primitive = f != nullptr && f->args.size() == 1 && (f->id == symbols_.prime || f->id == symbols_.is_prime);
    if (!primitive)
        return evaluate_inner_at_.evaluate_at(e, input);
    const std::optional<integer> argument = find_argument(f->args[0], input);
    if (!argument.has_value())
        return std::nullopt;
    if (f->id == symbols_.prime) {
        const std::optional<integer> prime = find_nth_prime_.find_nth_prime(argument.value());
        if (!prime.has_value())
            return std::nullopt;
        return make_literal_.make_literal(prime.value());
    }
    const std::optional<bool> primality = find_primality_.find_primality(argument.value());
    if (!primality.has_value())
        return std::nullopt;
    return make_literal_.make_literal(integer(primality.value() ? 1 : 0));
}

NUM_EVALUATOR_TEMPLATE std::optional<bool> NUM_EVALUATOR::holds_at(const expr* condition, const expr* input) {
    const expr::functor* f = std::get_if<expr::functor>(&condition->content);
    const bool primality = f != nullptr && f->args.size() == 1 && f->id == symbols_.is_prime;
    if (!primality)
        return holds_inner_at_.holds_at(condition, input);
    const std::optional<integer> argument = find_argument(f->args[0], input);
    if (!argument.has_value())
        return std::nullopt;
    return find_primality_.find_primality(argument.value());
}

// The integer value of a primitive's argument.
NUM_EVALUATOR_TEMPLATE std::optional<integer> NUM_EVALUATOR::find_argument(const expr* e, const expr* input) {
    const std::optional<const expr*> value = evaluate_at(e, input);
    if (!value.has_value())
        return std::nullopt;
    return find_literal_value_.find_value(value.value());
}

#undef NUM_EVALUATOR
#undef NUM_EVALUATOR_TEMPLATE

#endif
