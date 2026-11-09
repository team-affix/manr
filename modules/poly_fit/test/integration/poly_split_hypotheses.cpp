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

struct PolySplitHypothesesIntegrationTest : ::testing::Test {
    std::vector<std::string> propose(const std::vector<std::string>& variables, const std::vector<poly_sample>& samples) {
        manifest = std::make_unique<poly_fit_manifest>(variables.size(), samples, k_poly_default_limits,
                                                       k_poly_fit_default_limits, 1);
        std::vector<const expr*> out;
        manifest->rule_set_.fit_.splits_.propose_hypotheses(manifest->rule_set_.fit_.converter_.convert(samples), out);
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

TEST_F(PolySplitHypothesesIntegrationTest, TwoPolynomialsSplitWhereTheSecondStarts) {
    const std::vector<std::string> hypotheses = propose(
        {"x"}, sample_line(-6, 5, [](integer x) { return x < 0 ? integer(x * x) : integer(3 * x + 1); }, {}));
    EXPECT_EQ(hypotheses, std::vector<std::string>{"pw(when(lt(#0, 0), #0^2), 3 * #0 + 1)"});
}

TEST_F(PolySplitHypothesesIntegrationTest, ThreePolynomialsGiveTwoBounds) {
    const std::vector<std::string> hypotheses = propose({"x"}, sample_line(0, 17, [](integer x) {
        return x < 6 ? integer(5) : x < 12 ? integer(x * x - 7) : integer(2 * x);
    }, {}));
    EXPECT_EQ(hypotheses, std::vector<std::string>{"pw(when(lt(#0, 6), 5), when(lt(#0, 12), #0^2 - 7), 2 * #0)"});
}

TEST_F(PolySplitHypothesesIntegrationTest, AnOutlierInsideAPieceDoesNotSplitIt) {
    const std::vector<std::string> hypotheses = propose(
        {"x"}, sample_line(-6, 5, [](integer x) { return x < 0 ? integer(x * x) : integer(3 * x + 1); }, {{2, 4}}));
    EXPECT_EQ(hypotheses, std::vector<std::string>{"pw(when(lt(#0, 0), #0^2), 3 * #0 + 1)"});
}

TEST_F(PolySplitHypothesesIntegrationTest, MultivariateDataIsSplitOnTheVariableThatSeparatesIt) {
    const std::vector<std::string> hypotheses = propose({"x", "y"}, sample_grid(-3, 3, [](integer x, integer y) {
        return y < 0 ? integer(x - y) : integer((x + y) * (x + y));
    }));
    EXPECT_THAT(hypotheses, Contains("pw(when(lt(#0, 0), #1 - #0), #1^2 + 2 * #1 * #0 + #0^2)"));
}

TEST_F(PolySplitHypothesesIntegrationTest, OnePolynomialIsNotSplit) {
    const std::vector<std::string> hypotheses =
        propose({"x"}, sample_line(-5, 5, [](integer x) { return integer(x * x * x - x); }, {}));
    EXPECT_TRUE(hypotheses.empty());
}

TEST_F(PolySplitHypothesesIntegrationTest, AHigherDegreeThatSkipsAStepDoesNotBeatTheSimplePiece) {
    // x(x + 3)(x + y) + 3x + y is x - y at x = -2, -1 and x + y at x = 0, and reaches further than x - y
    const std::vector<std::string> hypotheses = propose({"x", "y"}, sample_grid(-3, 3, [](integer x, integer y) {
        return x < 0 ? integer(x - y) : integer(x + y);
    }));
    EXPECT_THAT(hypotheses, Contains("pw(when(lt(#1, 0), #1 - #0), #1 + #0)"));
}
