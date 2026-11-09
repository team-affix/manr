#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/poly_fit_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"
#include "poly_fit_fixture.hpp"

using ::testing::Contains;

namespace {

struct PolyIterationHypothesesIntegrationTest : ::testing::Test {
    std::vector<std::string> propose(const std::vector<poly_sample>& samples) {
        manifest = std::make_unique<poly_fit_manifest>(std::vector<std::string>{"x"}, samples, k_poly_default_limits,
                                                       k_poly_fit_default_limits, 1);
        std::vector<const expr*> out;
        manifest->rule_set_.iterations_.propose_hypotheses(manifest->rule_set_.fit_.converter_.convert(samples), out);
        std::vector<std::string> printed;
        for (const expr* hypothesis : out) {
            std::ostringstream os;
            manifest->kernel_.printer_.print(os, hypothesis);
            printed.push_back(os.str());
        }
        return printed;
    }

    std::unique_ptr<poly_fit_manifest> manifest;
};

integer square_plus_one(const integer& y) {
    return y * y + 1;
}

}

TEST_F(PolyIterationHypothesesIntegrationTest, AThreefoldIterateIsReadWithAHelper) {
    const std::vector<poly_sample> samples =
        sample_line(-6, 6, [](integer x) { return square_plus_one(square_plus_one(square_plus_one(x))); }, {});
    EXPECT_THAT(propose(samples), Contains("let(lam(#0^2 + 1), app(#0, app(#0, app(#0, x))))"));
}

TEST_F(PolyIterationHypothesesIntegrationTest, ANegativeLeadingCoefficientIsRead) {
    // g(y) = -y^2 + 3, iterated twice: the leading coefficient of g^2 is -(-1)^2 = -1
    const std::vector<poly_sample> samples = sample_line(-6, 6, [](integer x) {
        const integer once = -x * x + 3;
        return integer(-once * once + 3);
    }, {});
    EXPECT_THAT(propose(samples), Contains("let(lam(-1 * #0^2 + 3), app(#0, app(#0, x)))"));
}

TEST_F(PolyIterationHypothesesIntegrationTest, APolynomialThatIsNoIterateProposesNothing) {
    EXPECT_TRUE(propose(sample_line(-6, 6, [](integer x) { return integer(x * x * x * x * x * x * x * x + 1); }, {}))
                    .empty());
}

TEST_F(PolyIterationHypothesesIntegrationTest, DataThatIsNoPolynomialProposesNothing) {
    EXPECT_TRUE(propose(sample_line(-6, 6, [](integer x) { return integer(x < 0 ? 0 : 1); }, {})).empty());
}
