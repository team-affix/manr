#include <vector>
#include "infrastructure/poly_rules_manifest.hpp"
#include "poly_kernel_fixture.hpp"

using ::testing::Contains;

namespace {

struct PolyDetectorsIntegrationTest : PolyKernelFixture {
    poly_rules_manifest rules{kernel};
    std::vector<const expr*> out{};
};

}

TEST_F(PolyDetectorsIntegrationTest, TheCollectorFindsCancellationBehindExpansion) {
    rules.collector_.propose(parse("(x + 1)^2 - (x - 1)^2"), out);
    EXPECT_EQ(print_all(out), std::vector<std::string>{"4 * x"});
}

TEST_F(PolyDetectorsIntegrationTest, TheCollectorProposesNothingThatIsNotSmaller) {
    rules.collector_.propose(parse("(x + y)^3"), out);
    EXPECT_TRUE(out.empty());
}

TEST_F(PolyDetectorsIntegrationTest, TheHornerDetectorNestsADensePolynomial) {
    rules.horner_.propose(parse("x^4 + 2*x^3 + 3*x^2 + 4*x + 5"), out);
    EXPECT_EQ(print_all(out), std::vector<std::string>{"(((x + 2) * x + 3) * x + 4) * x + 5"});
}

TEST_F(PolyDetectorsIntegrationTest, TheFactorDetectorFindsALinearFactorByTheRationalRootTheorem) {
    rules.factor_.propose(parse("x^3 + 6*x^2 + 11*x + 6"), out);
    EXPECT_THAT(print_all(out), Contains("(x + 1) * (x^2 + 5 * x + 6)"));
}

TEST_F(PolyDetectorsIntegrationTest, TheFactorDetectorPullsOutTheContent) {
    rules.factor_.propose(parse("6*x^2*y + 9*x*y^2"), out);
    EXPECT_THAT(print_all(out), Contains("3 * x * y * (2 * x + 3 * y)"));
}

TEST_F(PolyDetectorsIntegrationTest, TheFactorDetectorFindsATwoVariableFactor) {
    rules.factor_.propose(parse("x^2 - y^2 + x + y"), out);
    EXPECT_THAT(print_all(out), Contains("(x + y) * (x - y + 1)"));
}

TEST_F(PolyDetectorsIntegrationTest, ThePowerDetectorRunsTheBinomialTheoremBackwards) {
    rules.power_.propose(parse("x^3 + 3*x^2*y + 3*x*y^2 + y^3"), out);
    EXPECT_THAT(print_all(out), Contains("(x + y)^3"));
}

TEST_F(PolyDetectorsIntegrationTest, ThePowerDetectorCompletesTheSquare) {
    rules.power_.propose(parse("x^2 + 6*x + 10"), out);
    EXPECT_THAT(print_all(out), Contains("(x + 3)^2 + 1"));
}

TEST_F(PolyDetectorsIntegrationTest, TheSplitterGroupsVariableDisjointParts) {
    rules.splitter_.propose(parse("x^2 + y^2 + 2*x + 2*y + 2"), out);
    EXPECT_EQ(print_all(out), std::vector<std::string>{"x^2 + 2 * x + (y^2 + 2 * y) + 2"});
}

TEST_F(PolyDetectorsIntegrationTest, LikeTermsMergeWithoutExpanding) {
    rules.like_terms_.propose(parse("3 * (a + b + c)^9 - (a + b + c)^9 * 2 + 4 - 4"), out);
    EXPECT_EQ(print_all(out), std::vector<std::string>{"(a + b + c)^9"});
}

TEST_F(PolyDetectorsIntegrationTest, RestrictionFindsMultivariateLinearFactorsWithAnyCoefficients) {
    const std::vector<poly_polynomial> factors = kernel.linear_factors_.find_linear_factors(
        normal_form("(x - 3*z + 2) * (3*x - 3*y - z - 2) * (2*x - 3)"));
    EXPECT_THAT(factors, Contains(normal_form("x - 3*z + 2")));
    EXPECT_THAT(factors, Contains(normal_form("3*x - 3*y - z - 2")));
    EXPECT_THAT(factors, Contains(normal_form("2*x - 3")));
}

TEST_F(PolyDetectorsIntegrationTest, RestrictionNeedsAConstantTerm) {
    EXPECT_TRUE(kernel.linear_factors_.find_linear_factors(normal_form("(x - y) * (x + 2*y)")).empty());
}

TEST_F(PolyDetectorsIntegrationTest, ThePowerDetectorRecoversAPowerWhoseExpansionHasHugeCoefficients) {
    const expr* expanded = kernel.renderer_.render(normal_form("(1000003*x + 5*y)^7"));
    rules.power_.propose(expanded, out);
    EXPECT_THAT(print_all(out), Contains("(1000003 * x + 5 * y)^7"));
}

TEST_F(PolyDetectorsIntegrationTest, RestrictionFindsFactorsWithCoefficientsPast64Bits) {
    EXPECT_THAT(kernel.linear_factors_.find_linear_factors(normal_form("(x - 7) * (99991 * y + 3)^4")),
                Contains(normal_form("99991 * y + 3")));
}
