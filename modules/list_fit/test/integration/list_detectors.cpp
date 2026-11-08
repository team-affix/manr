#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/list_fit_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"
#include "list_fit_fixture.hpp"

using ::testing::Contains;

namespace {

struct ListDetectorsIntegrationTest : ::testing::Test {
    list_fit_manifest manifest{{}, k_poly_default_limits, k_poly_fit_default_limits, 1};
    list_fit_rules_manifest& rules = manifest.rule_set_;
    std::vector<const expr*> out{};

    std::vector<pw_data_point> points(const std::vector<list_sample>& samples) {
        return rules.converter_.convert(samples);
    }
    std::vector<std::string> printed() {
        std::vector<std::string> texts;
        for (const expr* e : out) {
            std::ostringstream os;
            manifest.kernel_.printer_.print(os, e);
            texts.push_back(os.str());
        }
        return texts;
    }
};

std::vector<integer> doubled(const std::vector<integer>& list) {
    std::vector<integer> result;
    for (const integer& value : list)
        result.push_back(2 * value);
    return result;
}

}

TEST_F(ListDetectorsIntegrationTest, TheIntegerMapDetectorHandsElementPairsToTheElementDomain) {
    rules.integer_maps_.propose_hypotheses(points(sample_listed(all_lists(-2, 2, 2), doubled)), out);
    EXPECT_THAT(printed(), Contains("map(2 * e, xs)"));
}

TEST_F(ListDetectorsIntegrationTest, TheIntegerMapDetectorIgnoresOutputsOfAnotherLength) {
    rules.integer_maps_.propose_hypotheses(
        points({{.input = {1, 2}, .output = {5}, .output_is_list = true}, {.input = {3}, .output = {3, 3},
                                                                           .output_is_list = true}}),
        out);
    EXPECT_TRUE(out.empty());
}

TEST_F(ListDetectorsIntegrationTest, TheIntegerFoldDetectorReadsStepsOffListsAndTheirTails) {
    rules.integer_folds_.propose_hypotheses(points(sample_scalar(all_lists(0, 3, 2), [](const std::vector<integer>& list) {
                                        integer total = 0;
                                        for (const integer& value : list)
                                            total += value * value;
                                        return total;
                                    })),
                                    out);
    EXPECT_THAT(printed(), Contains("fold(h^2 + acc, 0, xs)"));
}

TEST_F(ListDetectorsIntegrationTest, TheIntegerFoldDetectorNeedsTheEmptyList) {
    std::vector<list_sample> samples = sample_scalar(all_lists(0, 3, 2), [](const std::vector<integer>& list) {
        return integer(list.size());
    });
    samples.erase(samples.begin());
    rules.integer_folds_.propose_hypotheses(points(samples), out);
    EXPECT_TRUE(out.empty());
}

TEST_F(ListDetectorsIntegrationTest, TheFilterDetectorKeepsValuesBelowAThreshold) {
    rules.filterings_.propose_hypotheses(points(sample_listed(all_lists(-2, 2, 2), [](const std::vector<integer>& list) {
                                             std::vector<integer> kept;
                                             for (const integer& value : list)
                                                 if (value < 1)
                                                     kept.push_back(value);
                                             return kept;
                                         })),
                                         out);
    EXPECT_EQ(printed(), std::vector<std::string>{"filter(lt(e, 1), xs)"});
}

TEST_F(ListDetectorsIntegrationTest, TheFilterDetectorProposesNothingWhenNoThresholdSeparatesTheValues) {
    rules.filterings_.propose_hypotheses(points(sample_listed(all_lists(-2, 2, 2), [](const std::vector<integer>& list) {
                                             std::vector<integer> kept;
                                             for (const integer& value : list)
                                                 if (value != 0)
                                                     kept.push_back(value);
                                             return kept;
                                         })),
                                         out);
    EXPECT_TRUE(out.empty());
}
