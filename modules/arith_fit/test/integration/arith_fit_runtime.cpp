#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "infrastructure/arith_fit_runtime.hpp"

namespace {

struct ArithFitRuntimeIntegrationTest : ::testing::Test {
    void drain(arith_fit_runtime& runtime) {
        while (runtime.next()) {}
    }
};

}

TEST_F(ArithFitRuntimeIntegrationTest, TheDataStartsAsAPiecewiseWithAnOpenCatchAll) {
    arith_fit_runtime runtime{{{.input = 0, .output = 1}, {.input = 1, .output = 3}}, 1000};
    EXPECT_EQ(runtime.initial(), "pw(case(0, 1), case(1, 3), ?0)");
}

TEST_F(ArithFitRuntimeIntegrationTest, LinearDataReducesToItsLineAndThePiecewiseDisappears) {
    arith_fit_runtime runtime{
        {{.input = 0, .output = 1}, {.input = 1, .output = 3}, {.input = 2, .output = 5}, {.input = 3, .output = 7}},
        1000};
    drain(runtime);
    EXPECT_EQ(runtime.best(), "lam(add(mul(2, #0), 1))");
    EXPECT_EQ(runtime.best_cost(), 6u);
}

TEST_F(ArithFitRuntimeIntegrationTest, AnOutlierStaysAsTheOnlyBranch) {
    arith_fit_runtime runtime{
        {{.input = 0, .output = 1}, {.input = 1, .output = 3}, {.input = 2, .output = 5}, {.input = 3, .output = 7},
         {.input = 10, .output = 99}},
        1000};
    drain(runtime);
    EXPECT_EQ(runtime.best(), "pw(case(10, 99), lam(add(mul(2, #0), 1)))");
}

TEST_F(ArithFitRuntimeIntegrationTest, MostlyConstantDataBindsTheCatchAllToTheConstant) {
    arith_fit_runtime runtime{
        {{.input = 0, .output = 4}, {.input = 1, .output = 4}, {.input = 2, .output = 4}, {.input = 3, .output = 9}},
        1000};
    drain(runtime);
    EXPECT_EQ(runtime.best(), "pw(case(3, 9), lam(4))");
}

TEST_F(ArithFitRuntimeIntegrationTest, TheDerivationStartsAtTheDataAndEndsAtTheBest) {
    arith_fit_runtime runtime{{{.input = 0, .output = 0}, {.input = 5, .output = 5}}, 1000};
    drain(runtime);
    EXPECT_EQ(runtime.derivation(), (std::vector<std::string>{"pw(case(0, 0), case(5, 5), ?0)", "lam(#0)"}));
}
