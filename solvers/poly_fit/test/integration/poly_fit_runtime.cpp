#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "composition/poly_fit_runtime.hpp"
#include "poly_fit_fixture.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"

namespace {

struct PolyFitRuntimeIntegrationTest : ::testing::Test {
    void drain(poly_fit_runtime& runtime) {
        while (runtime.rewrites() < 3000 && runtime.next()) {}
    }
};

}

TEST_F(PolyFitRuntimeIntegrationTest, TheDataStartsAsAPiecewiseOfPointsWithAnOpenCatchAll) {
    poly_fit_runtime runtime{2, {{.inputs = {1, 2}, .output = 3}}, k_poly_default_limits,
                             k_poly_fit_default_limits, 1000};
    EXPECT_EQ(runtime.initial(), "pw(case(point(1, 2), 3), ?0)");
}

TEST_F(PolyFitRuntimeIntegrationTest, CubicDataReducesToItsCubicAndThePiecewiseDisappears) {
    poly_fit_runtime runtime{1, sample_line(-3, 4, [](integer x) { return integer(x * x * x - x); }, {}),
                             k_poly_default_limits, k_poly_fit_default_limits, 1000};
    drain(runtime);
    EXPECT_EQ(runtime.best(), "lam(#0^3 - #0)");
}

TEST_F(PolyFitRuntimeIntegrationTest, TheFittedPolynomialIsCompressedByThePolynomialRules) {
    poly_fit_runtime runtime{1,
                             sample_line(-3, 4, [](integer x) { return integer((2 * x - 1) * (2 * x - 1) * (2 * x - 1)); }, {}),
                             k_poly_default_limits, k_poly_fit_default_limits, 1000};
    drain(runtime);
    EXPECT_EQ(runtime.best(), "lam((2 * #0 - 1)^3)");
}

TEST_F(PolyFitRuntimeIntegrationTest, AnOutlierStaysAsTheOnlyBranch) {
    poly_fit_runtime runtime{1, sample_line(0, 8, [](integer x) { return integer(x * x + 1); }, {{3, 7}}),
                             k_poly_default_limits, k_poly_fit_default_limits, 1000};
    drain(runtime);
    EXPECT_EQ(runtime.best(), "pw(case(point(3), 17), lam(#0^2 + 1))");
}

TEST_F(PolyFitRuntimeIntegrationTest, TwoVariableDataReducesToItsSmallestForm) {
    poly_fit_runtime runtime{2, sample_grid(-2, 2, [](integer x, integer y) { return integer((x + y) * (x + y)); }),
                             k_poly_default_limits, k_poly_fit_default_limits, 1000};
    drain(runtime);
    EXPECT_EQ(runtime.best(), "lams(2, (#1 + #0)^2)");
}

TEST_F(PolyFitRuntimeIntegrationTest, AThousandPointsReduceToTheirPolynomial) {
    poly_fit_runtime runtime{1, sample_line(-500, 499, [](integer x) { return integer(x * x * x - 2 * x + 5); }, {}),
                             k_poly_default_limits, k_poly_fit_default_limits, 1000};
    drain(runtime);
    EXPECT_EQ(runtime.best(), "lam(#0^3 - 2 * #0 + 5)");
}

TEST_F(PolyFitRuntimeIntegrationTest, ScatteredOutliersEachStayAsABranch) {
    poly_fit_runtime runtime{1, sample_line(0, 29, [](integer x) { return integer(x * x + 1); }, {{3, 5}, {14, -2}, {25, 1}}),
                             k_poly_default_limits, k_poly_fit_default_limits, 1000};
    drain(runtime);
    EXPECT_EQ(runtime.best(), "pw(case(point(3), 15), case(point(14), 195), case(point(25), 627), lam(#0^2 + 1))");
}

TEST_F(PolyFitRuntimeIntegrationTest, DataFromTwoPolynomialsReducesToARegionPerPolynomial) {
    poly_fit_runtime runtime{1, sample_line(-200, 199, [](integer x) { return x < 0 ? integer(x * x) : integer(3 * x + 1); }, {}),
                             k_poly_default_limits, k_poly_fit_default_limits, 1000};
    drain(runtime);
    EXPECT_EQ(runtime.best(), "lam(pw(when(lt(#0, 0), #0^2), 3 * #0 + 1))");
}

TEST_F(PolyFitRuntimeIntegrationTest, EachPieceIsCompressedByThePolynomialRules) {
    poly_fit_runtime runtime{1, sample_line(0, 29, [](integer x) {
                                 return x < 10 ? integer(5) : x < 20 ? integer(x * x - 7) : integer((x - 1) * (x - 1) * (x - 1));
                             }, {}),
                             k_poly_default_limits, k_poly_fit_default_limits, 1000};
    drain(runtime);
    EXPECT_EQ(runtime.best(), "lam(pw(when(lt(#0, 10), 5), when(lt(#0, 20), #0^2 - 7), (#0 - 1)^3))");
}

TEST_F(PolyFitRuntimeIntegrationTest, AnOutlierInPiecewiseDataStaysAsACaseBeforeTheRegions) {
    poly_fit_runtime runtime{1, sample_line(-10, 9, [](integer x) { return x < 0 ? integer(x * x) : integer(3 * x + 1); }, {{4, 3}}),
                             k_poly_default_limits, k_poly_fit_default_limits, 1000};
    drain(runtime);
    EXPECT_EQ(runtime.best(), "pw(case(point(-6), 39), lam(pw(when(lt(#0, 0), #0^2), 3 * #0 + 1)))");
}

TEST_F(PolyFitRuntimeIntegrationTest, MultivariateDataSplitsOnOneVariable) {
    poly_fit_runtime runtime{2, sample_grid(-3, 3, [](integer x, integer y) {
                                 return x < 1 ? integer(x * y) : integer(x + y * y);
                             }),
                             k_poly_default_limits, k_poly_fit_default_limits, 1000};
    drain(runtime);
    EXPECT_EQ(runtime.best(), "lams(2, pw(when(lt(#1, 1), #1 * #0), #0^2 + #1))");
}
