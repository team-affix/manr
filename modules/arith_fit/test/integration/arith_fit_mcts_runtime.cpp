#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "infrastructure/arith_fit_mcts_runtime.hpp"

namespace {

struct ArithFitMctsRuntimeIntegrationTest : ::testing::Test {
    std::vector<arith_sample> line_with_outlier{
        {.input = 0, .output = 1}, {.input = 1, .output = 3}, {.input = 2, .output = 5}, {.input = 3, .output = 7},
        {.input = 10, .output = 99}};

    void drain(arith_fit_mcts_runtime& runtime) {
        while (runtime.next()) {}
    }
};

}

TEST_F(ArithFitMctsRuntimeIntegrationTest, SimulationsFindTheLineAndLeaveTheOutlierAsTheOnlyBranch) {
    arith_fit_mcts_runtime runtime{line_with_outlier, 10, 200, 0.5, 1};
    drain(runtime);
    EXPECT_EQ(runtime.best(), "pw(case(10, 99), lam(add(mul(2, #0), 1)))");
    EXPECT_EQ(runtime.simulations(), 200u);
}

TEST_F(ArithFitMctsRuntimeIntegrationTest, TheDerivationStartsAtTheDataAndEndsAtTheBest) {
    arith_fit_mcts_runtime runtime{{{.input = 0, .output = 0}, {.input = 5, .output = 5}}, 10, 50, 0.5, 1};
    drain(runtime);
    EXPECT_EQ(runtime.derivation(), (std::vector<std::string>{"pw(case(0, 0), case(5, 5), ?0)", "lam(#0)"}));
}
