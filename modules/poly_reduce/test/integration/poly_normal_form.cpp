#include <optional>
#include "infrastructure/poly_divisibility_filter.hpp"
#include "poly_kernel_fixture.hpp"

namespace {

struct PolyNormalFormIntegrationTest : PolyKernelFixture {};

}

TEST_F(PolyNormalFormIntegrationTest, EqualPolynomialsHaveEqualNormalForms) {
    EXPECT_EQ(normal_form("(x + 1)^2 - (x - 1)^2"), normal_form("4 * x"));
    EXPECT_EQ(normal_form("x * y"), normal_form("y * x"));
    EXPECT_EQ(normal_form("(a + b) * (a - b)"), normal_form("a^2 - b^2"));
    EXPECT_NE(normal_form("x + 1"), normal_form("x + 2"));
}

TEST_F(PolyNormalFormIntegrationTest, AnExpansionPastTheTermLimitHasNoNormalForm) {
    EXPECT_FALSE(kernel.expander_.find_polynomial(parse("(a + b + c + d + e)^9")).has_value());
}

TEST_F(PolyNormalFormIntegrationTest, RenderingIsTheExpandedSumLeadingTermFirst) {
    EXPECT_EQ(print(kernel.renderer_.render(normal_form("3 + (x - y)^2"))), "x^2 - 2 * x * y + y^2 + 3");
    EXPECT_EQ(print(kernel.renderer_.render(normal_form("x - x"))), "0");
}

TEST_F(PolyNormalFormIntegrationTest, HornerFormNestsTheMultiplications) {
    EXPECT_EQ(print(kernel.horner_.render_horner(normal_form("x^3 + 2*x^2 + 3*x + 4"))), "((x + 2) * x + 3) * x + 4");
    EXPECT_EQ(print(kernel.horner_.render_horner(normal_form("x^5 + 2*x^4 + 3*x^3"))), "((x + 2) * x + 3) * x^3");
}

TEST_F(PolyNormalFormIntegrationTest, ExactDivisionFindsTheQuotientOrNothing) {
    EXPECT_EQ(kernel.arithmetic_.find_quotient(normal_form("x^3 + 6*x^2 + 11*x + 6"), normal_form("x + 2")),
              std::optional<poly_polynomial>(normal_form("x^2 + 4*x + 3")));
    EXPECT_FALSE(kernel.arithmetic_.find_quotient(normal_form("x^2 + 1"), normal_form("x + 1")).has_value());
    EXPECT_FALSE(kernel.arithmetic_.find_quotient(normal_form("x^2 + 1"), normal_form("2*x")).has_value());
}

TEST_F(PolyNormalFormIntegrationTest, CoefficientsPast64BitsAreExact) {
    EXPECT_EQ(print(kernel.renderer_.render(normal_form("(4294967296 * x)^3"))), "79228162514264337593543950336 * x^3");
    EXPECT_EQ(normal_form("(3*x + 5*y)^30 - (3*x + 5*y)^30"), normal_form("0"));
}

TEST_F(PolyNormalFormIntegrationTest, LiteralsOfAnyLengthRoundTrip) {
    EXPECT_EQ(print(parse("123456789012345678901234567890 * x - (-98765432109876543210)")),
              "123456789012345678901234567890 * x - (-98765432109876543210)");
}

TEST_F(PolyNormalFormIntegrationTest, ExactDivisionWorksOnHugeCoefficients) {
    EXPECT_EQ(kernel.arithmetic_.find_quotient(normal_form("(3*x + 5*y)^30"), normal_form("(3*x + 5*y)^29")),
              std::optional<poly_polynomial>(normal_form("3*x + 5*y")));
}

TEST_F(PolyNormalFormIntegrationTest, RootExtractionFindsExactPowersAndCompletesThem) {
    const std::vector<poly_root> exact = kernel.roots_.find_roots(normal_form("x^3 + 3*x^2*y + 3*x*y^2 + y^3"), 3);
    ASSERT_FALSE(exact.empty());
    EXPECT_EQ(exact.front().root, normal_form("x + y"));
    EXPECT_TRUE(exact.front().remainder.terms.empty());
    const std::vector<poly_root> completed = kernel.roots_.find_roots(normal_form("x^2 + 2*x + y^2 + 2*y + 2"), 2);
    ASSERT_FALSE(completed.empty());
    EXPECT_EQ(completed.front().root, normal_form("x + 1"));
    EXPECT_EQ(completed.front().remainder, normal_form("y^2 + 2*y + 1"));
}

TEST_F(PolyNormalFormIntegrationTest, TheRenderCostIsTheSizeOfWhatRenderBuilds) {
    for (const std::string text : {"0", "1", "-1", "x", "-x", "3*x^2*y - x*y + 7", "-2*x^3 + y^5 - 1", "x*y*z",
                                   "(x - y)^4", "(2*a - 3*b + 1)^3", "-(x^2)"}) {
        const poly_polynomial polynomial = normal_form(text);
        EXPECT_EQ(kernel.renderer_.find_cost(polynomial), kernel.sizes_.cost(kernel.renderer_.render(polynomial)))
            << text;
    }
}

TEST_F(PolyNormalFormIntegrationTest, TheDivisibilityFilterNeverRejectsATrueDivisor) {
    const poly_divisibility_filter filter{};
    EXPECT_TRUE(filter.may_divide(normal_form("(x - 3*z + 2) * (2*x + y - 1)^2"), normal_form("x - 3*z + 2")));
    EXPECT_TRUE(filter.may_divide(normal_form("(x^2 + y^2 + 1) * (x - y)"), normal_form("x^2 + y^2 + 1")));
}

TEST_F(PolyNormalFormIntegrationTest, TheDivisibilityFilterRejectsMostWrongDivisors) {
    const poly_divisibility_filter filter{};
    const poly_polynomial product = normal_form("(x - 3*z + 2) * (2*x + y - 1)");
    size_t rejected = 0;
    for (const std::string wrong : {"x + 1", "x - 1", "y + 2", "x + y", "x - y", "2*x + 3", "z - 1", "x + z"})
        rejected += filter.may_divide(product, normal_form(wrong)) ? 0 : 1;
    EXPECT_EQ(rejected, 8u);
}
