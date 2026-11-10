#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "composition/poly_fit_manifest.hpp"
#include "poly_fit_fixture.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"

namespace {

struct PolyResidueHypothesesIntegrationTest : ::testing::Test {
    std::vector<std::string> propose(const std::vector<poly_sample>& samples) {
        manifest = std::make_unique<poly_fit_manifest>(1, samples, k_poly_default_limits,
                                                       k_poly_fit_default_limits, 1);
        std::vector<const expr*> out;
        manifest->rule_set_.fit_.residues_.propose_hypotheses(manifest->rule_set_.fit_.converter_.convert(samples),
                                                              out);
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

TEST_F(PolyResidueHypothesesIntegrationTest, AParityIsAConstantPerResidue) {
    EXPECT_EQ(propose(sample_line(-6, 6, [](integer x) { return integer(x % 2 == 0 ? 1 : 0); }, {})),
              std::vector<std::string>{"pw(when(eq(mod(#0, 2), 1), 0), 1)"});
}

TEST_F(PolyResidueHypothesesIntegrationTest, EachResidueMayHaveItsOwnPolynomial) {
    EXPECT_EQ(propose(sample_line(-6, 6, [](integer x) { return integer(x % 3 == 0 ? x * x : integer(2)); }, {})),
              std::vector<std::string>{"pw(when(eq(mod(#0, 3), 0), #0^2), 2)"});
}

TEST_F(PolyResidueHypothesesIntegrationTest, APolynomialIsLeftToInterpolation) {
    EXPECT_TRUE(propose(sample_line(-6, 6, [](integer x) { return integer(x * x - 3); }, {})).empty());
}

TEST_F(PolyResidueHypothesesIntegrationTest, NothingPeriodicProposesNothing) {
    EXPECT_TRUE(propose(sample_line(0, 11, [](integer x) { return integer(x < 5 ? 0 : 1); }, {})).empty());
}
