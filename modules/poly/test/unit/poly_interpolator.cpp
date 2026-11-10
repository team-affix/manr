#include <optional>
#include <vector>
#include <gtest/gtest.h>
#include "infrastructure/poly_interpolator.hpp"

namespace {

struct PolyInterpolatorTest : ::testing::Test {
    // x is 4 and y is 9; inputs are given y first
    poly_interpolator interpolator{{9, 4}, 84};
    poly_monomial x{.powers = {poly_power{.variable = 4, .exponent = 1}}};
    poly_monomial y{.powers = {poly_power{.variable = 9, .exponent = 1}}};
    poly_monomial xy{.powers = {poly_power{.variable = 4, .exponent = 1}, poly_power{.variable = 9, .exponent = 1}}};
    // x*y + 2*x - 3, at every (y, x) in [-2, 2]^2
    std::vector<poly_sample> grid() {
        std::vector<poly_sample> samples;
        for (int y_value = -2; y_value <= 2; ++y_value)
            for (int x_value = -2; x_value <= 2; ++x_value)
                samples.push_back({.inputs = {y_value, x_value}, .output = x_value * y_value + 2 * x_value - 3});
        return samples;
    }
};

}

TEST_F(PolyInterpolatorTest, ThePolynomialThroughTheSamplesIsFoundWithItsUnknowns) {
    const std::optional<poly_interpolant> interpolant = interpolator.interpolate(grid(), 2, 0);
    ASSERT_TRUE(interpolant.has_value());
    const poly_polynomial expected{.terms = {{poly_monomial{}, integer(-3)}, {x, integer(2)}, {xy, integer(1)}}};
    EXPECT_EQ(interpolant.value().polynomial, expected);
    EXPECT_EQ(interpolant.value().unknowns, 6u);
}

TEST_F(PolyInterpolatorTest, ADegreeTooLowStillGivesAnInterpolantThatMissesSomePoints) {
    const std::vector<poly_sample> samples = grid();
    const std::optional<poly_interpolant> interpolant = interpolator.interpolate(samples, 1, 0);
    ASSERT_TRUE(interpolant.has_value());
    size_t missed = 0;
    for (const poly_sample& sample : samples)
        missed += interpolator.evaluate(interpolant.value().polynomial, sample.inputs) != sample.output ? 1 : 0;
    EXPECT_GT(missed, 0u);
}

TEST_F(PolyInterpolatorTest, FractionalCoefficientsGiveNothing) {
    poly_interpolator line{{4}, 84};
    const std::vector<poly_sample> samples{
        {.inputs = {0}, .output = 0}, {.inputs = {1}, .output = 1}, {.inputs = {2}, .output = 3}, {.inputs = {3}, .output = 6}};
    EXPECT_FALSE(line.interpolate(samples, 2, 0).has_value());
}

TEST_F(PolyInterpolatorTest, TooFewSamplesToCheckBeyondTheUnknownsGiveNothing) {
    poly_interpolator line{{4}, 84};
    const std::vector<poly_sample> samples{{.inputs = {0}, .output = 1}, {.inputs = {1}, .output = 2}};
    EXPECT_FALSE(line.interpolate(samples, 1, 0).has_value());
}

TEST_F(PolyInterpolatorTest, MoreUnknownsThanTheLimitGiveNothing) {
    poly_interpolator capped{{9, 4}, 5};
    EXPECT_FALSE(capped.interpolate(grid(), 2, 0).has_value());
}

TEST_F(PolyInterpolatorTest, EvaluationBindsEachInputToItsVariable) {
    const poly_polynomial polynomial{.terms = {{x, integer(10)}, {y, integer(1)}}};
    EXPECT_EQ(interpolator.evaluate(polynomial, {integer(2), integer(3)}), integer(32));
}
