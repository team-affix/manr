#include <cstdint>
#include <map>
#include <optional>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/pw_branch_filter.hpp"

using ::testing::_;
using ::testing::Return;

namespace {

struct MockEvaluateAt {
    MOCK_METHOD(std::optional<const expr*>, evaluate_at, (const expr*, const expr*));
};

struct MockMatchOutput {
    MOCK_METHOD(bool, match, (const expr*, const expr*, (std::map<uint32_t, const expr*>&)));
};

struct PwBranchFilterTest : ::testing::Test {
    MockEvaluateAt evaluate_at;
    MockMatchOutput match_output;
    pw_branch_filter<MockEvaluateAt, MockMatchOutput> filter{
        evaluate_at, match_output, pw_symbols{.piecewise = 7, .branch = 8, .region = 9}};
    expr_pool pool{};
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
    EXPECT_EQ(filter.find_uncovered({branch0, branch1}, otherwise), std::vector<const expr*>{branch1});
}

TEST_F(PwBranchFilterTest, AnOtherwiseWithNoValueCoversNothing) {
    EXPECT_CALL(evaluate_at, evaluate_at(otherwise, key0)).WillOnce(Return(std::nullopt));
    EXPECT_EQ(filter.find_uncovered({branch0}, otherwise), std::vector<const expr*>{branch0});
}

TEST_F(PwBranchFilterTest, ARegionBranchIsNeverCovered) {
    const expr* region = pool.make_functor(9, {key0, value});
    EXPECT_EQ(filter.find_uncovered({region}, otherwise), std::vector<const expr*>{region});
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
    EXPECT_TRUE(filter.find_uncovered({holed0, holed1}, otherwise).empty());
    EXPECT_EQ(seen, (std::map<uint32_t, const expr*>{{1, value}}));
}
