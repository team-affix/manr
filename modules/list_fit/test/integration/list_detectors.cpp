#include <algorithm>
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

TEST_F(ListDetectorsIntegrationTest, TheExtremumFoldDetectorReadsTheLargestElementWithTheEmptyListsOutputAsBase) {
    rules.extremum_folds_.propose_hypotheses(points(sample_scalar(all_lists(0, 3, 3), [](const std::vector<integer>& list) {
                                                 integer largest = 0;
                                                 for (const integer& value : list)
                                                     largest = std::max(largest, value);
                                                 return largest;
                                             })),
                                             out);
    EXPECT_EQ(printed(), std::vector<std::string>{"fold(pw(when(lt(acc, h), h), acc), 0, xs)"});
}

TEST_F(ListDetectorsIntegrationTest, TheExtremumFoldDetectorReadsTheSmallestElementWithoutTails) {
    rules.extremum_folds_.propose_hypotheses(
        points(sample_scalar(random_lists(20, 3, 8, -9, 9, 12), [](const std::vector<integer>& list) {
            return *std::min_element(list.begin(), list.end());
        })),
        out);
    EXPECT_EQ(printed(), std::vector<std::string>{"fold(pw(when(lt(h, acc), h), acc), 0, xs)"});
}

TEST_F(ListDetectorsIntegrationTest, TheExtremumFoldDetectorTakesTheBaseFromOutputsBeyondEveryElement) {
    rules.extremum_folds_.propose_hypotheses(
        points(sample_scalar(random_lists(20, 1, 4, 0, 5, 13), [](const std::vector<integer>& list) {
            return std::max(integer(2), *std::max_element(list.begin(), list.end()));
        })),
        out);
    EXPECT_EQ(printed(), std::vector<std::string>{"fold(pw(when(lt(acc, h), h), acc), 2, xs)"});
}

TEST_F(ListDetectorsIntegrationTest, TheExtremumFoldDetectorProposesNothingForSums) {
    rules.extremum_folds_.propose_hypotheses(points(sample_scalar(all_lists(0, 3, 3), [](const std::vector<integer>& list) {
                                                 integer total = 0;
                                                 for (const integer& value : list)
                                                     total += value;
                                                 return total;
                                             })),
                                             out);
    EXPECT_TRUE(out.empty());
}

TEST_F(ListDetectorsIntegrationTest, TheExtremumFoldDetectorDoesNotTakeAWrongLabelBeyondItsListAsTheBase) {
    std::vector<list_sample> samples = sample_scalar(random_lists(20, 3, 8, 1, 9, 16), [](const std::vector<integer>& list) {
        return *std::max_element(list.begin(), list.end());
    });
    samples[0].output[0] += 100;
    rules.extremum_folds_.propose_hypotheses(points(samples), out);
    EXPECT_EQ(printed(), std::vector<std::string>{"fold(pw(when(lt(acc, h), h), acc), 0, xs)"});
}

TEST_F(ListDetectorsIntegrationTest, TheProductFoldDetectorReadsAProductWithoutTails) {
    rules.product_folds_.propose_hypotheses(
        points(sample_scalar(random_lists(20, 2, 6, -5, 5, 21), [](const std::vector<integer>& list) {
            integer product = 1;
            for (const integer& value : list)
                product *= value;
            return product;
        })),
        out);
    EXPECT_EQ(printed(), std::vector<std::string>{"fold(h * acc, 1, xs)"});
}

TEST_F(ListDetectorsIntegrationTest, TheProductFoldDetectorReadsALinearFactorAndABase) {
    rules.product_folds_.propose_hypotheses(
        points(sample_scalar(random_lists(20, 1, 5, -4, 4, 22), [](const std::vector<integer>& list) {
            integer product = 3;
            for (const integer& value : list)
                product *= 2 - value;
            return product;
        })),
        out);
    EXPECT_EQ(printed(), std::vector<std::string>{"fold(-1 * h * acc + 2 * acc, 3, xs)"});
}

TEST_F(ListDetectorsIntegrationTest, TheProductFoldDetectorStartsPastAWrongLabelOnTheFirstList) {
    std::vector<list_sample> samples = sample_scalar(random_lists(20, 2, 6, 1, 5, 23), [](const std::vector<integer>& list) {
        integer product = 1;
        for (const integer& value : list)
            product *= value;
        return product;
    });
    samples[0].output[0] += 1;
    rules.product_folds_.propose_hypotheses(points(samples), out);
    EXPECT_EQ(printed(), std::vector<std::string>{"fold(h * acc, 1, xs)"});
}

TEST_F(ListDetectorsIntegrationTest, TheProductFoldDetectorProposesNothingForSums) {
    rules.product_folds_.propose_hypotheses(points(sample_scalar(all_lists(0, 3, 3), [](const std::vector<integer>& list) {
                                                integer total = 0;
                                                for (const integer& value : list)
                                                    total += value;
                                                return total;
                                            })),
                                            out);
    EXPECT_TRUE(out.empty());
}

TEST_F(ListDetectorsIntegrationTest, TheCountFoldDetectorCountsElementsAboveAThresholdWithoutTails) {
    rules.count_folds_.propose_hypotheses(
        points(sample_scalar(random_lists(20, 3, 8, -9, 9, 24), [](const std::vector<integer>& list) {
            return integer(std::count_if(list.begin(), list.end(), [](const integer& value) { return value > 2; }));
        })),
        out);
    EXPECT_EQ(printed(), std::vector<std::string>{"fold(pw(when(lt(2, h), acc + 1), acc), 0, xs)"});
}

TEST_F(ListDetectorsIntegrationTest, TheCountFoldDetectorCountsElementsBelowAThresholdFromABase) {
    rules.count_folds_.propose_hypotheses(
        points(sample_scalar(random_lists(20, 3, 8, -9, 9, 25), [](const std::vector<integer>& list) {
            return 5 + integer(std::count_if(list.begin(), list.end(), [](const integer& value) { return value < -1; }));
        })),
        out);
    EXPECT_EQ(printed(), std::vector<std::string>{"fold(pw(when(lt(h, -1), acc + 1), acc), 5, xs)"});
}

TEST_F(ListDetectorsIntegrationTest, TheCountFoldDetectorProposesNothingForSums) {
    rules.count_folds_.propose_hypotheses(points(sample_scalar(random_lists(20, 3, 8, -9, 9, 26), [](const std::vector<integer>& list) {
                                              integer total = 0;
                                              for (const integer& value : list)
                                                  total += value;
                                              return total;
                                          })),
                                          out);
    EXPECT_TRUE(out.empty());
}

TEST_F(ListDetectorsIntegrationTest, TheMapFilterDetectorReadsAMapOverAFilter) {
    rules.map_filters_.propose_hypotheses(
        points(sample_listed(random_lists(30, 2, 6, -9, 9, 28), [](const std::vector<integer>& list) {
            std::vector<integer> result;
            for (const integer& value : list)
                if (value > 0)
                    result.push_back(value * value);
            return result;
        })),
        out);
    EXPECT_THAT(printed(), Contains("map(e^2, filter(lt(0, e), xs))"));
}

TEST_F(ListDetectorsIntegrationTest, TheMapFilterDetectorLeavesAPlainMapToTheMapDetector) {
    rules.map_filters_.propose_hypotheses(points(sample_listed(all_lists(-2, 2, 3), doubled)), out);
    EXPECT_TRUE(out.empty());
}

TEST_F(ListDetectorsIntegrationTest, TheMapFilterDetectorLeavesAPlainFilterToTheFilterDetector) {
    rules.map_filters_.propose_hypotheses(
        points(sample_listed(all_lists(-2, 2, 3), [](const std::vector<integer>& list) {
            std::vector<integer> kept;
            for (const integer& value : list)
                if (value > 0)
                    kept.push_back(value);
            return kept;
        })),
        out);
    EXPECT_TRUE(out.empty());
}

TEST_F(ListDetectorsIntegrationTest, TheRenamerTurnsAnElementBodyIntoAHeadBody) {
    const expr* e = manifest.kernel_.exprs_.make_functor(rules.list_symbols_.element, {});
    const expr* body = manifest.kernel_.exprs_.make_functor(manifest.kernel_.poly_symbols_.mul, {e, e});
    out.push_back(rules.renamer_.rename(body, rules.list_symbols_.element, rules.list_symbols_.head));
    EXPECT_EQ(printed(), std::vector<std::string>{"h * h"});
}

TEST_F(ListDetectorsIntegrationTest, TheSelectionFoldDetectorReadsTheLargestSquare) {
    rules.selection_folds_.propose_hypotheses(
        points(sample_scalar(random_lists(30, 2, 6, -9, 9, 32), [](const std::vector<integer>& list) {
            integer largest = 0;
            for (const integer& value : list)
                largest = std::max(largest, integer(value * value));
            return largest;
        })),
        out);
    EXPECT_THAT(printed(), Contains("fold(pw(when(lt(acc, h^2), h^2), acc), 0, xs)"));
}

TEST_F(ListDetectorsIntegrationTest, TheSelectionFoldDetectorReadsTheSmallestOfAnAffineFunction) {
    rules.selection_folds_.propose_hypotheses(
        points(sample_scalar(random_lists(30, 2, 6, -9, 9, 37), [](const std::vector<integer>& list) {
            integer smallest = 100;
            for (const integer& value : list)
                smallest = std::min(smallest, integer(2 * value + 1));
            return smallest;
        })),
        out);
    // without the empty list the data shows no base; any base at or above every output fits, and the highest is taken
    EXPECT_THAT(printed(), Contains("fold(pw(when(lt(2 * h + 1, acc), 2 * h + 1), acc), 7, xs)"));
}

TEST_F(ListDetectorsIntegrationTest, TheSelectionFoldDetectorLeavesTheLargestElementToTheExtremumDetector) {
    rules.selection_folds_.propose_hypotheses(
        points(sample_scalar(random_lists(30, 2, 6, -9, 9, 38), [](const std::vector<integer>& list) {
            return *std::max_element(list.begin(), list.end());
        })),
        out);
    EXPECT_TRUE(out.empty());
}

TEST_F(ListDetectorsIntegrationTest, TheSelectionFoldDetectorProposesNothingForSums) {
    rules.selection_folds_.propose_hypotheses(
        points(sample_scalar(random_lists(30, 2, 6, -9, 9, 39), [](const std::vector<integer>& list) {
            integer total = 0;
            for (const integer& value : list)
                total += value;
            return total;
        })),
        out);
    EXPECT_TRUE(out.empty());
}

TEST_F(ListDetectorsIntegrationTest, TheFactorFoldDetectorReadsAProductOfSquaresWithoutTails) {
    rules.factor_folds_.propose_hypotheses(
        points(sample_scalar(random_lists(30, 2, 6, -4, 4, 33), [](const std::vector<integer>& list) {
            integer product = 1;
            for (const integer& value : list)
                product *= value * value;
            return product;
        })),
        out);
    EXPECT_THAT(printed(), Contains("fold(h^2 * acc, 1, xs)"));
}

TEST_F(ListDetectorsIntegrationTest, TheFactorFoldDetectorLeavesAPlainProductToTheProductDetector) {
    rules.factor_folds_.propose_hypotheses(
        points(sample_scalar(random_lists(30, 2, 6, -4, 4, 40), [](const std::vector<integer>& list) {
            integer product = 1;
            for (const integer& value : list)
                product *= value;
            return product;
        })),
        out);
    EXPECT_TRUE(out.empty());
}

TEST_F(ListDetectorsIntegrationTest, TheFactorFoldDetectorProposesNothingForSums) {
    rules.factor_folds_.propose_hypotheses(
        points(sample_scalar(random_lists(30, 2, 6, -4, 4, 41), [](const std::vector<integer>& list) {
            integer total = 0;
            for (const integer& value : list)
                total += value;
            return total;
        })),
        out);
    EXPECT_TRUE(out.empty());
}
