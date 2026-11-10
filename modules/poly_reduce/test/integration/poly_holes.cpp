#include <string>
#include <vector>
#include "infrastructure/best_first_search.hpp"
#include "infrastructure/poly_rules_manifest.hpp"
#include "poly_kernel_fixture.hpp"

using ::testing::Contains;
using ::testing::IsEmpty;

namespace {

// Expressions holding free variables `?n`: the hole binder alone, and a search over every rule.
struct PolyHolesIntegrationTest : PolyKernelFixture {
    poly_rules_manifest rules{kernel};
    std::vector<rewrite> out{};

    std::vector<std::string> bound(const std::string& text) {
        out.clear();
        rules.holes_.detect(parse(text), out);
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

TEST_F(PolyHolesIntegrationTest, AHoleTwiceIsBoundToZero) {
    EXPECT_THAT(bound("?0 + ?0"), Contains("0"));
}

TEST_F(PolyHolesIntegrationTest, AHoleOfDegreeOneIsBoundToTheNearestQuotient) {
    // reduce request #17: (?0 + 1)^2 - ?0^2 is 2 * ?0 + 1, which ?0 = 0 leaves 1
    EXPECT_THAT(bound("(?0 + 1)^2 - ?0^2"), Contains("1"));
    // 3 * ?0 + 7: ?0 = -2 leaves 1
    EXPECT_THAT(bound("3 * ?0 + 7"), Contains("1"));
}

TEST_F(PolyHolesIntegrationTest, AHoleIsBoundToAnExpressionOfTheInputsThatCancelsTheRest) {
    // reduce request #26: ?0 = -y
    EXPECT_THAT(bound("x * ?0 + x * y"), Contains("0"));
    // ?0 = x + 1 cancels (x + 1) * (x - 1)
    EXPECT_THAT(bound("(x - 1) * ?0 - x^2 + 1"), Contains("0"));
}

TEST_F(PolyHolesIntegrationTest, AHoleOfHigherDegreeIsBoundToARootOfTheRest) {
    // reduce request #25: ?0 = x
    EXPECT_THAT(bound("?0^2 - x^2 + 1"), Contains("1"));
    // ?0 = x + y
    EXPECT_THAT(bound("2 * ?0^3 - 2 * (x + y)^3 + 4"), Contains("4"));
}

TEST_F(PolyHolesIntegrationTest, OtherHolesStayHoles) {
    // ?1 = 0 drops ?0 * ?1, and ?0 stays
    EXPECT_THAT(bound("?0 * ?1 + ?0 * x + x"), Contains("?0 * x + x"));
}

TEST_F(PolyHolesIntegrationTest, NothingIsProposedWithoutAHole) {
    EXPECT_THAT(bound("(x + 1)^2 - x^2"), IsEmpty());
}

TEST_F(PolyHolesIntegrationTest, NothingIsProposedThatIsNotSmaller) {
    // ?0 alone is as small as anything it could become
    EXPECT_THAT(bound("?0"), IsEmpty());
}

TEST_F(PolyHolesIntegrationTest, SearchReducesExpressionsWithHolesToLiterals) {
    EXPECT_EQ(reduced("(?0 + 1)^2 - ?0^2"), "1");
    EXPECT_EQ(reduced("?0^2 - x^2 + 1"), "1");
    EXPECT_EQ(reduced("x * ?0 + x * y"), "0");
    EXPECT_EQ(reduced("?0 * ?1 - 5"), "-5");
}
