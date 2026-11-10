#include <memory>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "composition/poly_fit_manifest.hpp"
#include "poly_fit_fixture.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"

// Every rewrite every rule detects, at every position, on every expression a fit can reach, must succeed when
// performed and still give every sample's output at its input. Checked along random walks from the data's
// piecewise, against the samples themselves as the oracle.
namespace {

struct PolyFitSoundnessIntegrationTest : ::testing::Test {
    std::mt19937 rng{2026};

    // Performs every rewrite of each expression on `walks` random walks of `depth` steps; the number checked.
    size_t walk(const std::vector<std::string>& variables, const std::vector<poly_sample>& samples, int walks, int depth) {
        poly_fit_manifest manifest{variables.size(), samples, k_poly_default_limits, k_poly_fit_default_limits, 1};
        poly_fit_rules_manifest& rules = manifest.rule_set_;
        const std::vector<pw_data_point> points = rules.fit_.converter_.convert(samples);
        size_t checked = 0;
        for (int index = 0; index < walks; ++index) {
            const expr* e = rules.initial_;
            for (int step = 0; step < depth; ++step) {
                std::vector<rewrite> out;
                rules.rules_.detect(e, out);
                if (out.empty())
                    break;
                for (const rewrite& detected : out) {
                    const expr* proposal = detected();
                    EXPECT_NE(proposal, nullptr) << "a detected rewrite failed on " << print(manifest, e);
                    if (proposal == nullptr)
                        return checked;
                    for (const pw_data_point& point : points) {
                        const std::optional<const expr*> value = rules.pw_evaluator_.evaluate_at(proposal, point.input);
                        const bool reproduced = value.has_value() && value.value() == point.output;
                        EXPECT_TRUE(reproduced) << print(manifest, e) << "  ->  " << print(manifest, proposal)
                                                << "  misses " << print(manifest, point.input);
                        if (!reproduced)
                            return checked;
                    }
                    ++checked;
                }
                e = out[rng() % out.size()]();
            }
        }
        return checked;
    }

    std::string print(poly_fit_manifest& manifest, const expr* e) {
        std::ostringstream os;
        manifest.kernel_.printer_.print(os, e);
        return os.str();
    }
};

}

TEST_F(PolyFitSoundnessIntegrationTest, EveryProposalOnPolynomialDataReproducesEverySample) {
    EXPECT_GT(walk({"x"}, sample_line(-3, 5, [](integer x) { return integer(x * x * x - 2 * x); }, {{2, 4}}), 10, 12), 100u);
}

TEST_F(PolyFitSoundnessIntegrationTest, EveryProposalOnPiecewiseDataReproducesEverySample) {
    const auto three = [](integer x) { return x < 3 ? integer(5) : x < 7 ? integer(x * x - 7) : integer(2 * x + 1); };
    EXPECT_GT(walk({"x"}, sample_line(0, 10, three, {{5, 1}}), 10, 12), 100u);
}

TEST_F(PolyFitSoundnessIntegrationTest, EveryProposalOnMultivariateDataReproducesEverySample) {
    const auto split = [](integer x, integer y) { return x < 0 ? integer(x - y) : integer((x + y) * (x + y)); };
    EXPECT_GT(walk({"x", "y"}, sample_grid(-2, 2, split), 6, 10), 100u);
}
