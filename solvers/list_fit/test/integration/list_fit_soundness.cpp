#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "composition/list_fit_manifest.hpp"
#include "list_fit_fixture.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"

// Every rewrite every rule detects, at every position, on every expression a fit can reach, must succeed when
// performed and still give every sample's output at its input. Checked along random walks from the data's
// piecewise, against the samples themselves as the oracle.
namespace {

struct ListFitSoundnessIntegrationTest : ::testing::Test {
    std::mt19937 rng{2026};

    // Performs every rewrite of each expression on `walks` random walks of `depth` steps; the number checked.
    size_t walk(const std::vector<list_sample>& samples, int walks, int depth) {
        list_fit_manifest manifest{samples, k_poly_default_limits, k_poly_fit_default_limits, 1};
        list_fit_rules_manifest& rules = manifest.rule_set_;
        const std::vector<pw_data_point> points = rules.converter_.convert(samples);
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

    std::string print(list_fit_manifest& manifest, const expr* e) {
        std::ostringstream os;
        manifest.kernel_.printer_.print(os, e);
        return os.str();
    }
};

}

TEST_F(ListFitSoundnessIntegrationTest, EveryProposalOnFoldDataReproducesEverySample) {
    std::vector<list_sample> samples = sample_scalar(all_lists(-2, 2, 2), [](const std::vector<integer>& list) {
        integer total = 0;
        for (const integer& value : list)
            total += value * value - 1;
        return total;
    });
    samples[9].output[0] += 3;
    EXPECT_GT(walk(samples, 8, 8), 100u);
}

TEST_F(ListFitSoundnessIntegrationTest, EveryProposalOnMapDataReproducesEverySample) {
    std::vector<list_sample> samples = sample_listed(all_lists(-3, 3, 2), [](const std::vector<integer>& list) {
        std::vector<integer> result;
        for (const integer& value : list)
            result.push_back(value < 0 ? integer(value * value) : integer(3 * value + 1));
        return result;
    });
    samples[4].output[0] += 1;
    EXPECT_GT(walk(samples, 40, 8), 100u);
}

TEST_F(ListFitSoundnessIntegrationTest, EveryProposalOnFilterDataReproducesEverySample) {
    std::vector<list_sample> samples = sample_listed(all_lists(-2, 2, 2), [](const std::vector<integer>& list) {
        std::vector<integer> kept;
        for (const integer& value : list)
            if (value > -1)
                kept.push_back(value);
        return kept;
    });
    samples[12].output.push_back(5);
    EXPECT_GT(walk(samples, 40, 8), 100u);
}
