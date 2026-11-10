#include <optional>
#include <vector>
#include <gtest/gtest.h>
#include "infrastructure/arith_linear_solver.hpp"

namespace {

struct ArithLinearSolverTest : ::testing::Test {
    arith_linear_solver solver{};
};

}

TEST_F(ArithLinearSolverTest, ADeterminedSystemIsSolved) {
    // a + b = 7, a - b = 3
    EXPECT_EQ(solver.solve({{1, 1, 7}, {1, -1, 3}}, 2), (std::optional<std::vector<integer>>{{5, 2}}));
}

TEST_F(ArithLinearSolverTest, RedundantEquationsAreAccepted) {
    EXPECT_EQ(solver.solve({{1, 0, 1}, {0, 1, 2}, {1, 0, 1}}, 2), (std::optional<std::vector<integer>>{{1, 2}}));
}

TEST_F(ArithLinearSolverTest, AnInconsistentSystemHasNoSolution) {
    EXPECT_FALSE(solver.solve({{1, 0, 1}, {1, 0, 2}}, 2).has_value());
}

TEST_F(ArithLinearSolverTest, AFractionalSolutionIsRejected) {
    EXPECT_FALSE(solver.solve({{2, 1}}, 1).has_value());
}

TEST_F(ArithLinearSolverTest, AnUnpinnedUnknownIsZero) {
    EXPECT_EQ(solver.solve({{1, 0, 4}}, 2), (std::optional<std::vector<integer>>{{4, 0}}));
}
