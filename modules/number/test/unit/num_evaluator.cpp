#include <optional>
#include <variant>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/num_evaluator.hpp"
#include "value_objects/integer.hpp"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace {

struct MockEvaluateInnerAt {
    MOCK_METHOD(std::optional<const expr*>, evaluate_at, (const expr*, const expr*));
};
struct MockHoldsInnerAt {
    MOCK_METHOD(std::optional<bool>, holds_at, (const expr*, const expr*));
};
struct MockFindLiteralValue {
    MOCK_METHOD(std::optional<integer>, find_value, (const expr*));
};
struct MockMakeLiteral {
    MOCK_METHOD(const expr*, make_literal, (const integer&));
};
struct MockFindNthPrime {
    MOCK_METHOD(std::optional<integer>, find_nth_prime, (const integer&));
};
struct MockFindPrimality {
    MOCK_METHOD(std::optional<bool>, find_primality, (const integer&));
};

constexpr uint32_t k_prime = 30;
constexpr uint32_t k_is_prime = 31;

// literals are nullary functors whose id is their value plus 100
struct NumEvaluatorTest : ::testing::Test {
    NumEvaluatorTest() {
        ON_CALL(find_literal_value, find_value(_)).WillByDefault(Invoke([](const expr* e) -> std::optional<integer> {
            const expr::functor* f = std::get_if<expr::functor>(&e->content);
            if (f == nullptr || !f->args.empty() || f->id < 100)
                return std::nullopt;
            return integer(static_cast<int>(f->id) - 100);
        }));
        ON_CALL(make_literal, make_literal(_)).WillByDefault(Invoke([this](const integer& value) {
            return literal(value.convert_to<int>());
        }));
        ON_CALL(evaluate_inner_at, evaluate_at(variable, input)).WillByDefault(Return(literal(4)));
    }
    const expr* literal(int value) { return pool.make_functor(static_cast<uint32_t>(value + 100), {}); }

    expr_pool pool{};
    ::testing::NiceMock<MockEvaluateInnerAt> evaluate_inner_at;
    ::testing::NiceMock<MockHoldsInnerAt> holds_inner_at;
    ::testing::NiceMock<MockFindLiteralValue> find_literal_value;
    ::testing::NiceMock<MockMakeLiteral> make_literal;
    ::testing::NiceMock<MockFindNthPrime> find_nth_prime;
    ::testing::NiceMock<MockFindPrimality> find_primality;
    num_evaluator<MockEvaluateInnerAt, MockHoldsInnerAt, MockFindLiteralValue, MockMakeLiteral, MockFindNthPrime,
                  MockFindPrimality>
        evaluator{evaluate_inner_at, holds_inner_at, find_literal_value, make_literal, find_nth_prime, find_primality,
                  num_symbols{.prime = k_prime, .is_prime = k_is_prime}};
    const expr* variable = pool.make_functor(1, {});
    const expr* input = pool.make_functor(2, {});
};

}

TEST_F(NumEvaluatorTest, APrimeIsTheNthPrimeOfItsArgumentsValue) {
    EXPECT_CALL(find_nth_prime, find_nth_prime(integer(4))).WillOnce(Return(integer(11)));
    EXPECT_EQ(evaluator.evaluate_at(pool.make_functor(k_prime, {variable}), input),
              std::optional<const expr*>(literal(11)));
}

TEST_F(NumEvaluatorTest, PrimalityIsOneOrZero) {
    EXPECT_CALL(find_primality, find_primality(integer(4))).WillOnce(Return(false));
    EXPECT_EQ(evaluator.evaluate_at(pool.make_functor(k_is_prime, {variable}), input),
              std::optional<const expr*>(literal(0)));
}

TEST_F(NumEvaluatorTest, PrimitivesNest) {
    EXPECT_CALL(find_nth_prime, find_nth_prime(integer(4))).WillOnce(Return(integer(11)));
    EXPECT_CALL(find_primality, find_primality(integer(11))).WillOnce(Return(true));
    const expr* nested = pool.make_functor(k_is_prime, {pool.make_functor(k_prime, {variable})});
    EXPECT_EQ(evaluator.evaluate_at(nested, input), std::optional<const expr*>(literal(1)));
}

TEST_F(NumEvaluatorTest, PrimalityHoldsAsACondition) {
    EXPECT_CALL(find_primality, find_primality(integer(4))).WillOnce(Return(false));
    EXPECT_EQ(evaluator.holds_at(pool.make_functor(k_is_prime, {variable}), input), std::optional<bool>(false));
}

TEST_F(NumEvaluatorTest, EverythingElseGoesToTheInnerDomain) {
    const expr* other = pool.make_functor(9, {variable});
    EXPECT_CALL(evaluate_inner_at, evaluate_at(other, input)).WillOnce(Return(literal(7)));
    EXPECT_CALL(holds_inner_at, holds_at(other, input)).WillOnce(Return(true));
    EXPECT_EQ(evaluator.evaluate_at(other, input), std::optional<const expr*>(literal(7)));
    EXPECT_EQ(evaluator.holds_at(other, input), std::optional<bool>(true));
}

TEST_F(NumEvaluatorTest, NoPrimePastTheTableHasNoValue) {
    EXPECT_CALL(find_nth_prime, find_nth_prime(integer(4))).WillOnce(Return(std::nullopt));
    EXPECT_FALSE(evaluator.evaluate_at(pool.make_functor(k_prime, {variable}), input).has_value());
}
