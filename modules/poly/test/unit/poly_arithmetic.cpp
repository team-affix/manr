#include <optional>
#include <gtest/gtest.h>
#include "infrastructure/poly_arithmetic.hpp"

namespace {

// polynomials in the variable 1, coefficients lowest degree first
poly_polynomial dense(const std::vector<int>& coefficients) {
    poly_polynomial polynomial;
    for (size_t degree = 0; degree < coefficients.size(); ++degree) {
        if (coefficients[degree] == 0)
            continue;
        poly_monomial monomial;
        if (degree > 0)
            monomial.powers.push_back(poly_power{.variable = 1, .exponent = static_cast<uint32_t>(degree)});
        polynomial.terms.emplace(monomial, integer(coefficients[degree]));
    }
    return polynomial;
}

struct PolyArithmeticTest : ::testing::Test {
    poly_arithmetic arithmetic{};
};

}

TEST_F(PolyArithmeticTest, TheGcdOfTwoPolynomialsIsTheirPrimitiveCommonFactor) {
    // x^2 - 1 and 2x^2 + 4x + 2 share x + 1
    EXPECT_EQ(arithmetic.find_gcd(dense({-1, 0, 1}), dense({2, 4, 2})), std::optional<poly_polynomial>{dense({1, 1})});
}

TEST_F(PolyArithmeticTest, PolynomialsWithNoCommonFactorHaveGcdOne) {
    EXPECT_EQ(arithmetic.find_gcd(dense({1, 0, 1}), dense({1, 1})), std::optional<poly_polynomial>{dense({1})});
}

TEST_F(PolyArithmeticTest, TheGcdHasAPositiveLeadingCoefficient) {
    EXPECT_EQ(arithmetic.find_gcd(dense({1, -1}), dense({-1, 1})), std::optional<poly_polynomial>{dense({-1, 1})});
}

TEST_F(PolyArithmeticTest, PolynomialsInTwoVariablesHaveNoGcdHere) {
    poly_polynomial two = dense({0, 1});
    poly_monomial other;
    other.powers.push_back(poly_power{.variable = 2, .exponent = 1});
    two.terms.emplace(other, integer(1));
    EXPECT_FALSE(arithmetic.find_gcd(two, dense({1, 1})).has_value());
}
