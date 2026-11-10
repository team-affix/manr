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

struct MockSolveHoles {
    using bindings_t = std::map<uint32_t, const expr*>;
    MOCK_METHOD(std::optional<bindings_t>, solve_holes, (const expr*, const expr*, const bindings_t&));
};

struct PwHoleMatcherTest : ::testing::Test {
    ::testing::NiceMock<MockSolveHoles> solve_holes;
    pw_hole_matcher<MockSolveHoles> matcher{solve_holes};
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
    EXPECT_CALL(solve_holes, solve_holes(_, _, _)).Times(0);
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

TEST_F(PwHoleMatcherTest, AnOtherwiseUnmatchedPatternIsSolvedByTheDomain) {
    const expr* pattern = pool.make_functor(6, {two, hole2});
    EXPECT_CALL(solve_holes, solve_holes(pattern, one, _)).WillOnce(Return(MockSolveHoles::bindings_t{{2, two}}));
    EXPECT_TRUE(matcher.match(pattern, one, bindings));
    EXPECT_EQ(bindings, (std::map<uint32_t, const expr*>{{2, two}}));
}

TEST_F(PwHoleMatcherTest, ASolutionConflictingWithABindingDoesNotMatch) {
    bindings.emplace(2, one);
    const expr* pattern = pool.make_functor(6, {two, hole2});
    EXPECT_CALL(solve_holes, solve_holes(pattern, one, _)).WillOnce(Return(MockSolveHoles::bindings_t{{2, two}}));
    EXPECT_FALSE(matcher.match(pattern, one, bindings));
    EXPECT_EQ(bindings, (std::map<uint32_t, const expr*>{{2, one}}));
}

TEST_F(PwHoleMatcherTest, APatternWithTwoHolesIsSolvedByTheDomainForBoth) {
    const expr* pattern = pool.make_functor(6, {hole1, hole2});
    EXPECT_CALL(solve_holes, solve_holes(pattern, one, _))
        .WillOnce(Return(MockSolveHoles::bindings_t{{1, two}, {2, one}}));
    EXPECT_TRUE(matcher.match(pattern, one, bindings));
    EXPECT_EQ(bindings, (std::map<uint32_t, const expr*>{{1, two}, {2, one}}));
}

TEST_F(PwHoleMatcherTest, AValueHoldingAHoleIsMatchedAgainstAnOutputWithout) {
    const expr* value = pool.make_functor(6, {two, hole1});
    EXPECT_CALL(solve_holes, solve_holes(value, one, _)).WillOnce(Return(MockSolveHoles::bindings_t{{1, two}}));
    EXPECT_TRUE(matcher.match(one, value, bindings));
    EXPECT_EQ(bindings, (std::map<uint32_t, const expr*>{{1, two}}));
}

TEST_F(PwHoleMatcherTest, AnUnsolvedPatternDoesNotMatch) {
    EXPECT_CALL(solve_holes, solve_holes(_, _, _)).WillOnce(Return(std::nullopt));
    EXPECT_FALSE(matcher.match(pool.make_functor(6, {hole1, hole2}), one, bindings));
}
