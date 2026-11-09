#include <algorithm>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "infrastructure/list_fit_runtime.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"
#include "list_fit_fixture.hpp"

namespace {

struct ListFitRuntimeIntegrationTest : ::testing::Test {
    std::string fit(const std::vector<list_sample>& samples) {
        return fit_within(samples, 3000);
    }
    std::string fit_within(const std::vector<list_sample>& samples, size_t rewrites) {
        list_fit_runtime runtime{samples, k_poly_default_limits, k_poly_fit_default_limits, 20000};
        while (runtime.rewrites() < rewrites && runtime.next()) {}
        return runtime.best();
    }

    std::vector<std::vector<integer>> lists = all_lists(-2, 2, 3);
};

integer sum(const std::vector<integer>& list) {
    integer total = 0;
    for (const integer& value : list)
        total += value;
    return total;
}

}

TEST_F(ListFitRuntimeIntegrationTest, TheDataStartsAsAPiecewiseOfListsWithAnOpenCatchAll) {
    list_fit_runtime runtime{{{.input = {1, 2}, .output = {3}, .output_is_list = false}}, k_poly_default_limits,
                             k_poly_fit_default_limits, 1000};
    EXPECT_EQ(runtime.initial(), "pw(case(cons(1, cons(2, nil)), 3), ?0)");
}

TEST_F(ListFitRuntimeIntegrationTest, SumsReduceToAFold) {
    EXPECT_EQ(fit(sample_scalar(lists, sum)), "lam(fold(lam(lam(#0 + #1)), 0, #0))");
}

TEST_F(ListFitRuntimeIntegrationTest, LengthsReduceToAFoldThatCounts) {
    EXPECT_EQ(fit(sample_scalar(lists, [](const std::vector<integer>& list) { return integer(list.size()); })),
              "lam(fold(lam(lam(#1 + 1)), 0, #0))");
}

TEST_F(ListFitRuntimeIntegrationTest, ProductsReduceToAFoldFromOne) {
    EXPECT_EQ(fit(sample_scalar(lists, [](const std::vector<integer>& list) {
                  integer product = 1;
                  for (const integer& value : list)
                      product *= value;
                  return product;
              })),
              "lam(fold(lam(lam(#0 * #1)), 1, #0))");
}

TEST_F(ListFitRuntimeIntegrationTest, SquaringEveryElementReducesToAMap) {
    EXPECT_EQ(fit(sample_listed(lists, [](const std::vector<integer>& list) {
                  std::vector<integer> squares;
                  for (const integer& value : list)
                      squares.push_back(value * value);
                  return squares;
              })),
              "lam(map(lam(#0^2), #0))");
}

TEST_F(ListFitRuntimeIntegrationTest, AbsoluteValuesReduceToAMapOfAPiecewise) {
    // each piece needs more element values than it has coefficients
    EXPECT_EQ(fit(sample_listed(all_lists(-3, 3, 2), [](const std::vector<integer>& list) {
                  std::vector<integer> magnitudes;
                  for (const integer& value : list)
                      magnitudes.push_back(value < 0 ? integer(-value) : value);
                  return magnitudes;
              })),
              "lam(map(lam(pw(when(lt(#0, 1), -1 * #0), #0)), #0))");
}

TEST_F(ListFitRuntimeIntegrationTest, KeepingThePositivesReducesToAFilter) {
    EXPECT_EQ(fit(sample_listed(lists, [](const std::vector<integer>& list) {
                  std::vector<integer> kept;
                  for (const integer& value : list)
                      if (value > 0)
                          kept.push_back(value);
                  return kept;
              })),
              "lam(filter(lam(lt(0, #0)), #0))");
}

TEST_F(ListFitRuntimeIntegrationTest, AWrongLabelStaysAsTheOnlyBranch) {
    std::vector<list_sample> samples = sample_scalar(lists, sum);
    samples[7].output[0] += 10;
    EXPECT_EQ(fit(samples), "pw(case(cons(-1, cons(-2, nil)), 7), lam(fold(lam(lam(#0 + #1)), 0, #0)))");
}

TEST_F(ListFitRuntimeIntegrationTest, SumsOfLongListsReduceToAFold) {
    // 12 lists of 20 to 40 elements, with their tails
    EXPECT_EQ(fit(sample_scalar(with_tails(random_lists(12, 20, 40, -50, 50, 1)), sum)), "lam(fold(lam(lam(#0 + #1)), 0, #0))");
}

TEST_F(ListFitRuntimeIntegrationTest, ProductsOfLongListsReduceToAFoldDespiteHugeValues) {
    // found on the first expansion; the rest of a full budget only revisits a piecewise of ~15000 nodes
    EXPECT_EQ(fit_within(sample_scalar(with_tails(random_lists(12, 20, 40, -9, 9, 2)), [](const std::vector<integer>& list) {
                  integer product = 1;
                  for (const integer& value : list)
                      product *= value;
                  return product;
              }),
                         100),
              "lam(fold(lam(lam(#0 * #1)), 1, #0))");
}

TEST_F(ListFitRuntimeIntegrationTest, MapsOverLongListsReduceWithoutTails) {
    EXPECT_EQ(fit(sample_listed(random_lists(30, 20, 40, -50, 50, 3), [](const std::vector<integer>& list) {
                  std::vector<integer> result;
                  for (const integer& value : list)
                      result.push_back(2 * value * value - 3);
                  return result;
              })),
              "lam(map(lam(2 * #0^2 - 3), #0))");
}

TEST_F(ListFitRuntimeIntegrationTest, FiltersOverLongListsReduceWithoutTails) {
    EXPECT_EQ(fit(sample_listed(random_lists(30, 20, 40, -50, 50, 4), [](const std::vector<integer>& list) {
                  std::vector<integer> kept;
                  for (const integer& value : list)
                      if (value < 17)
                          kept.push_back(value);
                  return kept;
              })),
              "lam(filter(lam(lt(#0, 17)), #0))");
}

TEST_F(ListFitRuntimeIntegrationTest, SumsOfLongListsReduceToAFoldWithoutTails) {
    EXPECT_EQ(fit(sample_scalar(random_lists(40, 20, 40, -50, 50, 8), sum)), "lam(fold(lam(lam(#0 + #1)), 0, #0))");
}

TEST_F(ListFitRuntimeIntegrationTest, NumbersFromTheirDigitsReduceToAFoldWithoutTails) {
    EXPECT_EQ(fit(sample_scalar(random_lists(30, 2, 6, 0, 9, 9), [](const std::vector<integer>& list) {
                  integer number = 0;
                  for (size_t index = list.size(); index-- > 0;)
                      number = list[index] + 10 * number;
                  return number;
              })),
              "lam(fold(lam(lam(#0 + 10 * #1)), 0, #0))");
}

TEST_F(ListFitRuntimeIntegrationTest, AlternatingSumsReduceToAFoldWithoutTails) {
    EXPECT_EQ(fit(sample_scalar(random_lists(30, 5, 10, -20, 20, 10), [](const std::vector<integer>& list) {
                  integer total = 0;
                  for (size_t index = list.size(); index-- > 0;)
                      total = list[index] - total;
                  return total;
              })),
              "lam(fold(lam(lam(#0 - #1)), 0, #0))");
}

TEST_F(ListFitRuntimeIntegrationTest, AWrongLabelOnTheFirstListWithoutTailsStaysAsTheOnlyBranch) {
    std::vector<list_sample> samples = sample_scalar(random_lists(30, 3, 6, -9, 9, 11), sum);
    samples[0].output[0] += 100;
    const std::string best = fit(samples);
    EXPECT_TRUE(best.starts_with("pw(case(")) << best;
    EXPECT_TRUE(best.ends_with("), lam(fold(lam(lam(#0 + #1)), 0, #0)))")) << best;
    EXPECT_EQ(best.find("case(", 8), std::string::npos) << best;
}

TEST_F(ListFitRuntimeIntegrationTest, MaximaReduceToASelectionFold) {
    EXPECT_EQ(fit(sample_scalar(all_lists(0, 3, 3), [](const std::vector<integer>& list) {
                  integer largest = 0;
                  for (const integer& value : list)
                      largest = std::max(largest, value);
                  return largest;
              })),
              "lam(fold(lam(lam(pw(when(lt(#1, #0), #0), #1))), 0, #0))");
}

TEST_F(ListFitRuntimeIntegrationTest, MinimaOfLongListsReduceToASelectionFoldWithoutTails) {
    EXPECT_EQ(fit(sample_scalar(random_lists(30, 20, 40, -50, 50, 14), [](const std::vector<integer>& list) {
                  return *std::min_element(list.begin(), list.end());
              })),
              "lam(fold(lam(lam(pw(when(lt(#0, #1), #0), #1))), 0, #0))");
}

TEST_F(ListFitRuntimeIntegrationTest, ProductsReduceToAFoldWithoutTails) {
    EXPECT_EQ(fit(sample_scalar(random_lists(20, 2, 6, -5, 5, 21), [](const std::vector<integer>& list) {
                  integer product = 1;
                  for (const integer& value : list)
                      product *= value;
                  return product;
              })),
              "lam(fold(lam(lam(#0 * #1)), 1, #0))");
}

TEST_F(ListFitRuntimeIntegrationTest, CountsOfLongListsReduceToACountingFoldWithoutTails) {
    EXPECT_EQ(fit(sample_scalar(random_lists(30, 20, 40, -50, 50, 27), [](const std::vector<integer>& list) {
                  return integer(std::count_if(list.begin(), list.end(), [](const integer& value) { return value > 10; }));
              })),
              "lam(fold(lam(lam(pw(when(lt(10, #0), #1 + 1), #1))), 0, #0))");
}

TEST_F(ListFitRuntimeIntegrationTest, SquaresOfThePositivesReduceToAMapOverAFilter) {
    EXPECT_EQ(fit(sample_listed(random_lists(30, 2, 6, -9, 9, 28), [](const std::vector<integer>& list) {
                  std::vector<integer> result;
                  for (const integer& value : list)
                      if (value > 0)
                          result.push_back(value * value);
                  return result;
              })),
              "lam(map(lam(#0^2), filter(lam(lt(0, #0)), #0)))");
}

TEST_F(ListFitRuntimeIntegrationTest, LargestSquaresReduceToASelectionFoldOfASquare) {
    EXPECT_EQ(fit(sample_scalar(random_lists(30, 2, 6, -9, 9, 32), [](const std::vector<integer>& list) {
                  integer largest = 0;
                  for (const integer& value : list)
                      largest = std::max(largest, integer(value * value));
                  return largest;
              })),
              "lam(fold(lam(lam(pw(when(lt(#1, #0^2), #0^2), #1))), 0, #0))");
}

TEST_F(ListFitRuntimeIntegrationTest, ProductsOfSquaresReduceToAFoldWithoutTails) {
    EXPECT_EQ(fit(sample_scalar(random_lists(30, 2, 6, -4, 4, 33), [](const std::vector<integer>& list) {
                  integer product = 1;
                  for (const integer& value : list)
                      product *= value * value;
                  return product;
              })),
              "lam(fold(lam(lam(#0^2 * #1)), 1, #0))");
}
