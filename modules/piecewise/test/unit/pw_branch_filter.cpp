#include <cstdint>
#include <map>
#include <optional>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/pw_branch_filter.hpp"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::ReturnArg;

namespace {

struct MockEvaluateAt {
    MOCK_METHOD(std::optional<const expr*>, evaluate_at, (const expr*, const expr*));
};

struct MockMatchOutput {
    MOCK_METHOD(bool, match, (const expr*, const expr*, (std::map<uint32_t, const expr*>&)));
};

struct MockSubstituteHoles {
    MOCK_METHOD(const expr*, substitute_holes, (const expr*, (const std::map<uint32_t, const expr*>&)));
};

struct PwBranchFilterTest : ::testing::Test {
    MockEvaluateAt evaluate_at;
    MockMatchOutput match_output;
    ::testing::NiceMock<MockSubstituteHoles> substitute_holes;
    expr_pool pool{};
    const expr* zero = pool.make_functor(30, {});
    const expr* one = pool.make_functor(31, {});
    pw_branch_filter<MockEvaluateAt, MockMatchOutput, MockSubstituteHoles> filter{
        evaluate_at, match_output, substitute_holes, pw_symbols{.piecewise = 7, .branch = 8, .region = 9}, {zero, one}};

    PwBranchFilterTest() {
        ON_CALL(substitute_holes, substitute_holes(_, _)).WillByDefault(ReturnArg<0>());
    }
    const expr* key0 = pool.make_functor(1, {});
    const expr* key1 = pool.make_functor(2, {});
    const expr* value = pool.make_functor(3, {});
    const expr* other_value = pool.make_functor(4, {});
    const expr* branch0 = pool.make_functor(8, {key0, value});
    const expr* branch1 = pool.make_functor(8, {key1, value});
    const expr* otherwise = pool.make_functor(5, {});
};

}

TEST_F(PwBranchFilterTest, ABranchWhoseOutputMatchesTheOtherwisesValueIsCovered) {
    EXPECT_CALL(evaluate_at, evaluate_at(otherwise, key0)).WillOnce(Return(value));
    EXPECT_CALL(evaluate_at, evaluate_at(otherwise, key1)).WillOnce(Return(other_value));
    EXPECT_CALL(match_output, match(value, value, _)).WillOnce(Return(true));
    EXPECT_CALL(match_output, match(value, other_value, _)).WillOnce(Return(false));
    EXPECT_EQ(filter.find_uncovered({branch0, branch1}, otherwise).uncovered, std::vector<const expr*>{branch1});
}

TEST_F(PwBranchFilterTest, AnOtherwiseWithNoValueCoversNothing) {
    EXPECT_CALL(evaluate_at, evaluate_at(otherwise, key0)).WillOnce(Return(std::nullopt));
    EXPECT_EQ(filter.find_uncovered({branch0}, otherwise).uncovered, std::vector<const expr*>{branch0});
}

TEST_F(PwBranchFilterTest, ARegionBranchIsNeverCovered) {
    const expr* region = pool.make_functor(9, {key0, value});
    EXPECT_EQ(filter.find_uncovered({region}, otherwise).uncovered, std::vector<const expr*>{region});
}

TEST_F(PwBranchFilterTest, EveryBranchIsMatchedUnderOneSharedBinding) {
    const expr* hole = pool.make_var(1);
    std::map<uint32_t, const expr*> seen;
    EXPECT_CALL(evaluate_at, evaluate_at(otherwise, key0)).WillOnce(Return(value));
    EXPECT_CALL(evaluate_at, evaluate_at(otherwise, key1)).WillOnce(Return(value));
    EXPECT_CALL(match_output, match(_, value, _))
        .WillOnce([&](const expr*, const expr*, std::map<uint32_t, const expr*>& bindings) {
            bindings.emplace(1, value);
            return true;
        })
        .WillOnce([&](const expr*, const expr*, std::map<uint32_t, const expr*>& bindings) {
            seen = bindings;
            return true;
        });
    const expr* holed0 = pool.make_functor(8, {key0, hole});
    const expr* holed1 = pool.make_functor(8, {key1, hole});
    const pw_coverage coverage = filter.find_uncovered({holed0, holed1}, otherwise);
    EXPECT_TRUE(coverage.uncovered.empty());
    EXPECT_EQ(seen, (std::map<uint32_t, const expr*>{{1, value}}));
    EXPECT_EQ(coverage.bindings, (std::map<uint32_t, const expr*>{{1, value}}));
}

TEST_F(PwBranchFilterTest, ALabelHoldingSeveralHolesIsMatchedAfterTheOthers) {
    // `?1 + ?2` first, `?2` second: the label with one hole pins its hole before the other is solved for
    const expr* sum = pool.make_functor(20, {pool.make_var(1), pool.make_var(2)});
    const expr* single = pool.make_var(2);
    EXPECT_CALL(evaluate_at, evaluate_at(otherwise, _)).WillRepeatedly(Return(value));
    InSequence order;
    EXPECT_CALL(match_output, match(single, value, _)).WillOnce(Return(true));
    EXPECT_CALL(match_output, match(sum, value, _)).WillOnce(Return(true));
    EXPECT_TRUE(filter.find_uncovered({pool.make_functor(8, {key0, sum}), pool.make_functor(8, {key1, single})},
                                      otherwise).uncovered.empty());
}

TEST_F(PwBranchFilterTest, ALabelHoldingAHoleThatFailsIsRetriedOnce) {
    // the second label's hole may be settled by a binding made after its first try
    const expr* hole = pool.make_var(1);
    const expr* holed = pool.make_functor(8, {key0, hole});
    EXPECT_CALL(evaluate_at, evaluate_at(otherwise, key0)).WillOnce(Return(value));
    EXPECT_CALL(match_output, match(hole, value, _)).WillOnce(Return(false)).WillOnce(Return(true));
    EXPECT_TRUE(filter.find_uncovered({holed}, otherwise).uncovered.empty());
}

TEST_F(PwBranchFilterTest, ALabelWithoutHolesIsMatchedOnce) {
    EXPECT_CALL(evaluate_at, evaluate_at(otherwise, key0)).WillOnce(Return(other_value));
    EXPECT_CALL(match_output, match(value, other_value, _)).WillOnce(Return(false));
    EXPECT_EQ(filter.find_uncovered({branch0}, otherwise).uncovered, std::vector<const expr*>{branch0});
}

TEST_F(PwBranchFilterTest, AKeyWhoseHolesStopTheModelIsTriedAtTheFillers) {
    // the model cannot be evaluated at `k(?1)`; at `k(0)` it cannot either, at `k(1)` it gives the label
    const expr* holed_key = pool.make_functor(1, {pool.make_var(1)});
    const expr* key_at_zero = pool.make_functor(1, {zero});
    const expr* key_at_one = pool.make_functor(1, {one});
    ON_CALL(substitute_holes, substitute_holes(holed_key, std::map<uint32_t, const expr*>{{1, zero}}))
        .WillByDefault(Return(key_at_zero));
    ON_CALL(substitute_holes, substitute_holes(holed_key, std::map<uint32_t, const expr*>{{1, one}}))
        .WillByDefault(Return(key_at_one));
    EXPECT_CALL(evaluate_at, evaluate_at(otherwise, holed_key)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(evaluate_at, evaluate_at(otherwise, key_at_zero)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(evaluate_at, evaluate_at(otherwise, key_at_one)).WillOnce(Return(value));
    EXPECT_CALL(match_output, match(value, value, _)).WillOnce(Return(true));
    const pw_coverage coverage = filter.find_uncovered({pool.make_functor(8, {holed_key, value})}, otherwise);
    EXPECT_TRUE(coverage.uncovered.empty());
    EXPECT_EQ(coverage.bindings, (std::map<uint32_t, const expr*>{{1, one}}));
}

TEST_F(PwBranchFilterTest, AKeyNoFillerSettlesIsNotCovered) {
    const expr* holed_key = pool.make_functor(1, {pool.make_var(1)});
    ON_CALL(substitute_holes, substitute_holes(_, _)).WillByDefault(Return(key1));
    EXPECT_CALL(evaluate_at, evaluate_at(otherwise, _)).WillRepeatedly(Return(std::nullopt));
    const expr* branch = pool.make_functor(8, {holed_key, value});
    const pw_coverage coverage = filter.find_uncovered({branch}, otherwise);
    EXPECT_EQ(coverage.uncovered, std::vector<const expr*>{branch});
    EXPECT_TRUE(coverage.bindings.empty());
}

TEST_F(PwBranchFilterTest, AKeyWhoseHolesLeaveTheModelUnmatchedIsTriedAtTheFillers) {
    // at `k(?1)` the model gives something else; at `k(0)` the label
    const expr* holed_key = pool.make_functor(1, {pool.make_var(1)});
    const expr* key_at_zero = pool.make_functor(1, {zero});
    ON_CALL(substitute_holes, substitute_holes(holed_key, std::map<uint32_t, const expr*>{{1, zero}}))
        .WillByDefault(Return(key_at_zero));
    EXPECT_CALL(evaluate_at, evaluate_at(otherwise, holed_key)).WillOnce(Return(other_value));
    EXPECT_CALL(evaluate_at, evaluate_at(otherwise, key_at_zero)).WillOnce(Return(value));
    EXPECT_CALL(match_output, match(value, other_value, _)).WillRepeatedly(Return(false));
    EXPECT_CALL(match_output, match(value, value, _)).WillOnce(Return(true));
    const pw_coverage coverage = filter.find_uncovered({pool.make_functor(8, {holed_key, value})}, otherwise);
    EXPECT_TRUE(coverage.uncovered.empty());
    EXPECT_EQ(coverage.bindings, (std::map<uint32_t, const expr*>{{1, zero}}));
}
