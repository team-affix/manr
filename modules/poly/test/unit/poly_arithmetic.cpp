#include <optional>
#include <vector>
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

// polynomials in the variables 1 and 2: {coefficient, exponent of 1, exponent of 2} per term
poly_polynomial sparse(const std::vector<std::vector<int>>& terms) {
    poly_polynomial polynomial;
    for (const std::vector<int>& term : terms) {
        poly_monomial monomial;
        if (term[1] > 0)
            monomial.powers.push_back(poly_power{.variable = 1, .exponent = static_cast<uint32_t>(term[1])});
        if (term[2] > 0)
            monomial.powers.push_back(poly_power{.variable = 2, .exponent = static_cast<uint32_t>(term[2])});
        polynomial.terms.emplace(monomial, integer(term[0]));
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

TEST_F(PolyArithmeticTest, TheGcdInTwoVariablesIsTheirCommonFactor) {
    // x^2 - y^2 and x^2 - 2xy + y^2 share x - y
    const poly_polynomial gcd = arithmetic.find_gcd(sparse({{1, 2, 0}, {-1, 0, 2}}),
                                                    sparse({{1, 2, 0}, {-2, 1, 1}, {1, 0, 2}})).value();
    EXPECT_TRUE(gcd == sparse({{1, 1, 0}, {-1, 0, 1}}) || gcd == sparse({{-1, 1, 0}, {1, 0, 1}}));
}

TEST_F(PolyArithmeticTest, TheGcdInTwoVariablesKeepsACommonContent) {
    // y * (x + 1) and y * (x - 1) share y
    EXPECT_EQ(arithmetic.find_gcd(sparse({{1, 1, 1}, {1, 0, 1}}), sparse({{1, 1, 1}, {-1, 0, 1}})),
              std::optional<poly_polynomial>{sparse({{1, 0, 1}})});
}

TEST_F(PolyArithmeticTest, PolynomialsInTwoVariablesWithNoCommonFactorHaveGcdOne) {
    // x + y and x - y
    EXPECT_EQ(arithmetic.find_gcd(sparse({{1, 1, 0}, {1, 0, 1}}), sparse({{1, 1, 0}, {-1, 0, 1}})),
              std::optional<poly_polynomial>{dense({1})});
}

TEST_F(PolyArithmeticTest, APolynomialFreeOfOneVariableSharesOnlyItsFactorsInTheOther) {
    // x + 1 and (x + 1) * y
    EXPECT_EQ(arithmetic.find_gcd(dense({1, 1}), sparse({{1, 1, 1}, {1, 0, 1}})),
              std::optional<poly_polynomial>{dense({1, 1})});
}
