#include <gtest/gtest.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/mcts_path_walker.hpp"

namespace {

struct MctsPathWalkerTest : ::testing::Test {
    mcts_path_walker walker{};
    expr_pool pool{};
    const expr* a = pool.make_functor(1, {});
    const expr* b = pool.make_functor(2, {});
};

}

TEST_F(MctsPathWalkerTest, TheSameChoiceFromTheSameNodeIsTheSameNode) {
    EXPECT_EQ(walker.walk(0, a), walker.walk(0, a));
}

TEST_F(MctsPathWalkerTest, TwoRoutesToTheSameExpressionAreDifferentNodes) {
    EXPECT_NE(walker.walk(walker.walk(0, a), b), walker.walk(walker.walk(0, b), b));
}
