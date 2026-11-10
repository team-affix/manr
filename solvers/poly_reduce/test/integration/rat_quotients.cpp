#include <string>
#include <vector>
#include "composition/poly_rules_manifest.hpp"
#include "infrastructure/best_first_search.hpp"
#include "poly_kernel_fixture.hpp"

using ::testing::Contains;
using ::testing::IsEmpty;

namespace {

// Quotients of polynomials (`rational`): the canceller alone, and a search over every polynomial rule.
struct RatQuotientsIntegrationTest : PolyKernelFixture {
    poly_rules_manifest rules{kernel};
    std::vector<rewrite> out{};

    std::vector<std::string> cancelled(const std::string& text) {
        out.clear();
        rules.quotients_.detect(parse(text), out);
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
