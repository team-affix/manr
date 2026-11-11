#include <string>
#include <vector>
#include "composition/poly_rules_manifest.hpp"
#include "infrastructure/best_first_search.hpp"
#include "poly_kernel_fixture.hpp"

using ::testing::Contains;
using ::testing::IsEmpty;

namespace {

// Quotients of polynomials (`rational`): the canceller, the fraction combiner and the quotient hole binder alone, and a
// search over every polynomial rule.
struct RatQuotientsIntegrationTest : PolyKernelFixture {
    poly_rules_manifest rules{kernel};
    std::vector<rewrite> out{};

    std::vector<std::string> cancelled(const std::string& text) {
        out.clear();
        rules.quotients_.detect(parse(text), out);
        return print_all(perform(out));
    }
    std::vector<std::string> combined(const std::string& text) {
        out.clear();
        rules.fractions_.detect(parse(text), out);
        return print_all(perform(out));
    }
    std::vector<std::string> bound(const std::string& text) {
        out.clear();
        rules.quotient_holes_.detect(parse(text), out);
        return print_all(perform(out));
    }
    std::string reduced(const std::string& text) {
        best_first_search<poly_rules_manifest::rules_t, expr_size> search(rules.rules_, kernel.sizes_, parse(text),
                                                                          20000);
        while (search.rewrites() < 2000 && search.next()) {}
        return print(search.best());
    }
};

}

TEST_F(RatQuotientsIntegrationTest, ADivisorOfTheNumeratorLeavesTheQuotient) {
    // reduce request #38
    EXPECT_THAT(cancelled("(x^2 - 1) / (x - 1)"), Contains("x + 1"));
    EXPECT_EQ(reduced("(x^2 + 2 * x * y + y^2) / (x + y)"), "x + y");
}

TEST_F(RatQuotientsIntegrationTest, ACommonFactorIsCancelledFromBoth) {
    EXPECT_EQ(reduced("(x^2 - 1) / (x^2 + 2 * x + 1)"), "(x - 1) / (x + 1)");
}

TEST_F(RatQuotientsIntegrationTest, AQuotientWithNothingInCommonOrByZeroStays) {
    EXPECT_THAT(cancelled("(x^2 + 1) / (x + 1)"), IsEmpty());
    EXPECT_THAT(cancelled("x / (x - x)"), IsEmpty());
}

TEST_F(RatQuotientsIntegrationTest, ACommonFactorInSeveralVariablesIsCancelled) {
    // reduce request #39
    EXPECT_THAT(cancelled("(x^2 - y^2) / (x^2 - 2*x*y + y^2)"), Contains("(x + y) / (x - y)"));
    EXPECT_EQ(reduced("(x*z + y*z) / (x^2 + 2*x*y + y^2)"), "z / (x + y)");
}

TEST_F(RatQuotientsIntegrationTest, QuotientsOverOneDenominatorAddTheirNumerators) {
    // reduce request #40
    EXPECT_THAT(combined("x / (x + 1) + 1 / (x + 1)"), Contains("1"));
}

TEST_F(RatQuotientsIntegrationTest, QuotientsMeetOverTheProductOfTheirDenominators) {
    // reduce request #41
    EXPECT_THAT(combined("1 / (x - 1) - 1 / (x + 1)"), Contains("2 / (x^2 - 1)"));
}

TEST_F(RatQuotientsIntegrationTest, AProductFoldsItsFactorsIntoOneQuotient) {
    // reduce request #42
    EXPECT_THAT(combined("(x^2 + 1) / (x + 1) * (x + 1)"), Contains("x^2 + 1"));
    EXPECT_THAT(combined("(x / y) / (x^2 / y^2)"), Contains("y / x"));
}

TEST_F(RatQuotientsIntegrationTest, CombiningIsNotProposedWhenNotSmallerOrForALoneQuotient) {
    EXPECT_THAT(combined("1 / x + 1 / y"), IsEmpty());
    EXPECT_THAT(combined("(x^2 - 1) / (x - 1)"), IsEmpty());
}

TEST_F(RatQuotientsIntegrationTest, HolesInTheNumeratorAreBoundToMakeTheQuotientExact) {
    // reduce request #43: ?0 = 1
    EXPECT_THAT(bound("(x^2 - ?0) / (x - 1)"), Contains("x + 1"));
    // ?0 = -1, ?1 = 0, from the roots 1 and -1 of the denominator together
    EXPECT_THAT(bound("(x^3 + ?0 * x + ?1) / (x^2 - 1)"), Contains("x"));
}

TEST_F(RatQuotientsIntegrationTest, AHoleInTheDenominatorIsBoundAtARootOfTheNumerator) {
    // reduce request #44: ?0 = 2 or -2
    EXPECT_THAT(bound("(x^2 - 4) / (x - ?0)"), Contains("x + 2"));
    EXPECT_THAT(bound("(x^2 - 4) / (x - ?0)"), Contains("x - 2"));
}

TEST_F(RatQuotientsIntegrationTest, NoBindingIsProposedWhenNoRootExplainsTheQuotient) {
    EXPECT_THAT(bound("(x^2 + 1) / (x - ?0)"), IsEmpty());
}

TEST_F(RatQuotientsIntegrationTest, AQuotientHoldingAHoleIsBoundInsideASum) {
    EXPECT_EQ(reduced("(x^2 - ?0) / (x + 3) + y"), "x - 3 + y");
}
