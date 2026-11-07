#include <gtest/gtest.h>
#include "infrastructure/mcts_path_walker.hpp"

namespace {

struct MctsPathWalkerTest : ::testing::Test {
    mcts_path_walker walker{};
    const size_t a = 1;
    const size_t b = 2;
};

}

TEST_F(MctsPathWalkerTest, TheSameChoiceFromTheSameNodeIsTheSameNode) {
    EXPECT_EQ(walker.walk(0, a), walker.walk(0, a));
}

TEST_F(MctsPathWalkerTest, TheSameChoicesInADifferentOrderAreDifferentNodes) {
    EXPECT_NE(walker.walk(walker.walk(0, a), b), walker.walk(walker.walk(0, b), a));
}
