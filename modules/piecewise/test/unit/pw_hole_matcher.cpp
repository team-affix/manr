#include <cstdint>
#include <map>
#include <optional>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/pw_hole_matcher.hpp"

using ::testing::_;
using ::testing::Return;

namespace {

struct MockSolveHole {
    MOCK_METHOD(std::optional<const expr*>, solve_hole, (const expr*, uint32_t, const expr*));
};

struct PwHoleMatcherTest : ::testing::Test {
    MockSolveHole solve_hole;
    pw_hole_matcher<MockSolveHole> matcher{solve_hole};
    expr_pool pool{};
    std::map<uint32_t, const expr*> bindings;
    const expr* one = pool.make_functor(1, {});
    const expr* two = pool.make_functor(2, {});
    const expr* hole1 = pool.make_var(1);
    const expr* hole2 = pool.make_var(2);
};

}

TEST_F(PwHoleMatcherTest, AnIdenticalValueMatchesWithoutBinding) {
    EXPECT_TRUE(matcher.match(one, one, bindings));
    EXPECT_TRUE(bindings.empty());
}

TEST_F(PwHoleMatcherTest, ADifferentValueWithoutHolesDoesNotMatch) {
    EXPECT_CALL(solve_hole, solve_hole(_, _, _)).Times(0);
    EXPECT_FALSE(matcher.match(one, two, bindings));
}

TEST_F(PwHoleMatcherTest, AHoleBindsToTheValue) {
    EXPECT_TRUE(matcher.match(hole1, two, bindings));
    EXPECT_EQ(bindings, (std::map<uint32_t, const expr*>{{1, two}}));
}

TEST_F(PwHoleMatcherTest, ABoundHoleMatchesOnlyItsValue) {
    bindings.emplace(1, one);
    EXPECT_FALSE(matcher.match(hole1, two, bindings));
    EXPECT_TRUE(matcher.match(hole1, one, bindings));
}

TEST_F(PwHoleMatcherTest, FunctorsOfTheSameShapeMatchArgumentByArgument) {
    const expr* pattern = pool.make_functor(5, {one, hole1});
    const expr* value = pool.make_functor(5, {one, two});
    EXPECT_TRUE(matcher.match(pattern, value, bindings));
    EXPECT_EQ(bindings, (std::map<uint32_t, const expr*>{{1, two}}));
}

TEST_F(PwHoleMatcherTest, AFailedMatchLeavesTheBindingsUnchanged) {
    const expr* pattern = pool.make_functor(5, {hole1, hole2, one});
    const expr* value = pool.make_functor(5, {one, two, two});
    EXPECT_FALSE(matcher.match(pattern, value, bindings));
    EXPECT_TRUE(bindings.empty());
}

TEST_F(PwHoleMatcherTest, AnOtherwiseUnmatchedPatternWithOneHoleIsSolvedByTheDomain) {
    const expr* pattern = pool.make_functor(6, {two, hole2});
    EXPECT_CALL(solve_hole, solve_hole(pattern, 2u, one)).WillOnce(Return(two));
    EXPECT_TRUE(matcher.match(pattern, one, bindings));
    EXPECT_EQ(bindings, (std::map<uint32_t, const expr*>{{2, two}}));
}

TEST_F(PwHoleMatcherTest, ASolutionConflictingWithABindingDoesNotMatch) {
    bindings.emplace(2, one);
    const expr* pattern = pool.make_functor(6, {two, hole2});
    EXPECT_CALL(solve_hole, solve_hole(pattern, 2u, one)).WillOnce(Return(two));
    EXPECT_FALSE(matcher.match(pattern, one, bindings));
}

TEST_F(PwHoleMatcherTest, APatternWithTwoHolesIsNotHandedToTheDomain) {
    EXPECT_CALL(solve_hole, solve_hole(_, _, _)).Times(0);
    EXPECT_FALSE(matcher.match(pool.make_functor(6, {hole1, hole2}), one, bindings));
}
