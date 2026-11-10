#include <optional>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/pw_evaluator.hpp"

using ::testing::Return;
using ::testing::StrictMock;

namespace {

struct MockEvaluateAt {
    MOCK_METHOD(std::optional<const expr*>, evaluate_at, (const expr*, const expr*));
};

struct MockHoldsAt {
    MOCK_METHOD(std::optional<bool>, holds_at, (const expr*, const expr*));
};

struct PwEvaluatorTest : ::testing::Test {
    StrictMock<MockEvaluateAt> evaluate_at;
    StrictMock<MockHoldsAt> holds_at;
    pw_evaluator<MockEvaluateAt, MockHoldsAt> evaluator{
        evaluate_at, holds_at, pw_symbols{.piecewise = 7, .branch = 8, .region = 9}};
    expr_pool pool{};
    const expr* input = pool.make_functor(1, {});
    const expr* other_input = pool.make_functor(2, {});
    const expr* condition = pool.make_functor(3, {});
    const expr* case_value = pool.make_functor(4, {});
    const expr* region_value = pool.make_functor(5, {});
    const expr* otherwise = pool.make_functor(6, {});
    const expr* result = pool.make_functor(10, {});
    const expr* piecewise = pool.make_functor(7, {pool.make_functor(8, {input, case_value}),
                                                  pool.make_functor(9, {condition, region_value}), otherwise});
};

}

TEST_F(PwEvaluatorTest, AnythingButAPiecewiseIsEvaluatedByTheDomain) {
    EXPECT_CALL(evaluate_at, evaluate_at(otherwise, input)).WillOnce(Return(result));
    EXPECT_EQ(evaluator.evaluate_at(otherwise, input), std::optional<const expr*>{result});
}

TEST_F(PwEvaluatorTest, ACaseWhoseKeyIsTheInputGivesTheValue) {
    EXPECT_CALL(evaluate_at, evaluate_at(case_value, input)).WillOnce(Return(result));
    EXPECT_EQ(evaluator.evaluate_at(piecewise, input), std::optional<const expr*>{result});
}

TEST_F(PwEvaluatorTest, ARegionWhoseConditionHoldsGivesTheValue) {
    EXPECT_CALL(holds_at, holds_at(condition, other_input)).WillOnce(Return(true));
    EXPECT_CALL(evaluate_at, evaluate_at(region_value, other_input)).WillOnce(Return(result));
    EXPECT_EQ(evaluator.evaluate_at(piecewise, other_input), std::optional<const expr*>{result});
}

TEST_F(PwEvaluatorTest, WhenNoBranchAppliesTheOtherwiseGivesTheValue) {
    EXPECT_CALL(holds_at, holds_at(condition, other_input)).WillOnce(Return(false));
    EXPECT_CALL(evaluate_at, evaluate_at(otherwise, other_input)).WillOnce(Return(result));
    EXPECT_EQ(evaluator.evaluate_at(piecewise, other_input), std::optional<const expr*>{result});
}

TEST_F(PwEvaluatorTest, AnUndecidedConditionGivesNoValue) {
    EXPECT_CALL(holds_at, holds_at(condition, other_input)).WillOnce(Return(std::nullopt));
    EXPECT_EQ(evaluator.evaluate_at(piecewise, other_input), std::nullopt);
}

TEST_F(PwEvaluatorTest, ANestedPiecewiseIsEvaluatedThrough) {
    const expr* outer = pool.make_functor(7, {pool.make_functor(8, {other_input, case_value}), piecewise});
    EXPECT_CALL(evaluate_at, evaluate_at(case_value, input)).WillOnce(Return(result));
    EXPECT_EQ(evaluator.evaluate_at(outer, input), std::optional<const expr*>{result});
}

TEST_F(PwEvaluatorTest, ACaseLabelTheDomainCannotEvaluateIsItsOwnValue) {
    EXPECT_CALL(evaluate_at, evaluate_at(case_value, input)).WillOnce(Return(std::nullopt));
    EXPECT_EQ(evaluator.evaluate_at(piecewise, input), std::optional<const expr*>{case_value});
}
