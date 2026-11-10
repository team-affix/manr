#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "composition/poly_fit_manifest.hpp"
#include "poly_fit_fixture.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"

using ::testing::Contains;
using ::testing::Not;

namespace {

struct PolyInterpolationHypothesesIntegrationTest : ::testing::Test {
    std::vector<std::string> propose(const std::vector<std::string>& variables, const std::vector<poly_sample>& samples) {
        manifest = std::make_unique<poly_fit_manifest>(variables.size(), samples, k_poly_default_limits,
                                                       k_poly_fit_default_limits, 1);
        std::vector<const expr*> out;
        manifest->rule_set_.fit_.interpolations_.propose_hypotheses(manifest->rule_set_.fit_.converter_.convert(samples), out);
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

}

TEST_F(PolyInterpolationHypothesesIntegrationTest, CubicDataGivesItsCubic) {
    const std::vector<std::string> hypotheses =
        propose({"x"}, sample_line(-3, 4, [](integer x) { return integer(x * x * x - x); }, {}));
    EXPECT_THAT(hypotheses, Contains("#0^3 - #0"));
}

TEST_F(PolyInterpolationHypothesesIntegrationTest, AFitStartsPastAnOutlier) {
    const std::vector<std::string> hypotheses =
        propose({"x"}, sample_line(0, 8, [](integer x) { return integer(2 * x * x - 3); }, {{0, 5}}));
    EXPECT_THAT(hypotheses, Contains("2 * #0^2 - 3"));
}

TEST_F(PolyInterpolationHypothesesIntegrationTest, AFitStartsPastALongRunOfOutliers) {
    std::vector<std::pair<size_t, int>> outliers;
    for (size_t index = 0; index < 12; ++index)
        outliers.emplace_back(index, 1);
    const std::vector<std::string> hypotheses =
        propose({"x"}, sample_line(0, 39, [](integer x) { return integer(x * x - 4 * x); }, outliers));
    EXPECT_THAT(hypotheses, Contains("#0^2 - 4 * #0"));
}

TEST_F(PolyInterpolationHypothesesIntegrationTest, AThousandPointsGiveTheirPolynomial) {
    const std::vector<std::string> hypotheses =
        propose({"x"}, sample_line(-500, 499, [](integer x) { return integer(x * x * x - 2 * x + 5); }, {}));
    EXPECT_THAT(hypotheses, Contains("#0^3 - 2 * #0 + 5"));
}

TEST_F(PolyInterpolationHypothesesIntegrationTest, TwoVariableDataGivesItsExpandedPolynomial) {
    const std::vector<std::string> hypotheses =
        propose({"x", "y"}, sample_grid(-2, 2, [](integer x, integer y) { return integer((x + y) * (x + y)); }));
    EXPECT_THAT(hypotheses, Contains("#1^2 + 2 * #1 * #0 + #0^2"));
}

TEST_F(PolyInterpolationHypothesesIntegrationTest, FractionalCoefficientsGiveNoPolynomial) {
    const std::vector<std::string> hypotheses =
        propose({"x"}, sample_line(0, 6, [](integer x) { return integer(x * (x + 1) / 2); }, {}));
    EXPECT_THAT(hypotheses, Not(Contains("#0^2 + #0")));
    for (const std::string& hypothesis : hypotheses)
        EXPECT_EQ(hypothesis.find("#0^2"), std::string::npos) << hypothesis;
}

TEST_F(PolyInterpolationHypothesesIntegrationTest, AnInterpolantThatExplainsOnlyItsOwnPointsIsNotProposed) {
    // any 3 points fix a quadratic; these 3 have no pattern beyond that
    const std::vector<std::string> hypotheses = propose(
        {"x"}, {{.inputs = {0}, .output = 1}, {.inputs = {1}, .output = 7}, {.inputs = {2}, .output = 4}});
    EXPECT_TRUE(hypotheses.empty());
}
