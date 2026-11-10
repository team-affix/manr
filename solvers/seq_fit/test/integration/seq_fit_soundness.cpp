#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "composition/seq_fit_manifest.hpp"
#include "seq_fit_fixture.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"

// Every rewrite every rule detects, at every position, on every expression a fit can reach, must succeed when
// performed and still give every sample's output at its input. Checked along random walks from the data's
// piecewise, against the samples themselves as the oracle.
namespace {

struct SeqFitSoundnessIntegrationTest : ::testing::Test {
    std::mt19937 rng{2026};

    // Performs every rewrite of each expression on `walks` random walks of `depth` steps; the number checked.
    size_t walk(const std::vector<poly_sample>& samples, int walks, int depth) {
        seq_fit_manifest manifest{samples, k_poly_default_limits, k_poly_fit_default_limits, 1};
        seq_fit_rules_manifest& rules = manifest.rule_set_;
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

    std::string print(seq_fit_manifest& manifest, const expr* e) {
        std::ostringstream os;
        manifest.kernel_.printer_.print(os, e);
        return os.str();
    }
};

integer pell_step(const integer&, const integer& previous, const integer& before) {
    return 2 * previous + before;
}

integer affine_step(const integer&, const integer& previous, const integer&) {
    return 3 * previous - 4;
}

integer indexed_step(const integer& at, const integer& previous, const integer&) {
    return at * previous + 1;
}

}

TEST_F(SeqFitSoundnessIntegrationTest, EveryProposalOnSecondOrderDataReproducesEverySample) {
    std::vector<poly_sample> samples = recurrence_samples({0, 1}, pell_step, 0, 14);
    samples[6].output += 2;
    EXPECT_GT(walk(samples, 20, 6), 100u);
}

TEST_F(SeqFitSoundnessIntegrationTest, EveryProposalOnAffineDataReproducesEverySample) {
    std::vector<poly_sample> samples = recurrence_samples({5}, affine_step, 0, 14);
    samples[3].output -= 1;
    EXPECT_GT(walk(samples, 20, 6), 100u);
}

TEST_F(SeqFitSoundnessIntegrationTest, EveryProposalOnIndexedDataReproducesEverySample) {
    EXPECT_GT(walk(recurrence_samples({1}, indexed_step, 0, 12), 20, 6), 50u);
}
