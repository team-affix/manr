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
    EXPECT_EQ(fit(sample_scalar(lists, sum)), "fold(h + acc, 0, xs)");
}

TEST_F(ListFitRuntimeIntegrationTest, LengthsReduceToAFoldThatCounts) {
    EXPECT_EQ(fit(sample_scalar(lists, [](const std::vector<integer>& list) { return integer(list.size()); })),
              "fold(acc + 1, 0, xs)");
}

TEST_F(ListFitRuntimeIntegrationTest, ProductsReduceToAFoldFromOne) {
    EXPECT_EQ(fit(sample_scalar(lists, [](const std::vector<integer>& list) {
                  integer product = 1;
                  for (const integer& value : list)
                      product *= value;
                  return product;
              })),
              "fold(h * acc, 1, xs)");
}

TEST_F(ListFitRuntimeIntegrationTest, SquaringEveryElementReducesToAMap) {
    EXPECT_EQ(fit(sample_listed(lists, [](const std::vector<integer>& list) {
                  std::vector<integer> squares;
                  for (const integer& value : list)
                      squares.push_back(value * value);
                  return squares;
              })),
              "map(e^2, xs)");
}

TEST_F(ListFitRuntimeIntegrationTest, AbsoluteValuesReduceToAMapOfAPiecewise) {
    // each piece needs more element values than it has coefficients
    EXPECT_EQ(fit(sample_listed(all_lists(-3, 3, 2), [](const std::vector<integer>& list) {
                  std::vector<integer> magnitudes;
                  for (const integer& value : list)
                      magnitudes.push_back(value < 0 ? integer(-value) : value);
                  return magnitudes;
              })),
              "map(pw(when(lt(e, 1), -1 * e), e), xs)");
}

TEST_F(ListFitRuntimeIntegrationTest, KeepingThePositivesReducesToAFilter) {
    EXPECT_EQ(fit(sample_listed(lists, [](const std::vector<integer>& list) {
                  std::vector<integer> kept;
                  for (const integer& value : list)
                      if (value > 0)
                          kept.push_back(value);
                  return kept;
              })),
              "filter(lt(0, e), xs)");
}

TEST_F(ListFitRuntimeIntegrationTest, AWrongLabelStaysAsTheOnlyBranch) {
    std::vector<list_sample> samples = sample_scalar(lists, sum);
    samples[7].output[0] += 10;
    EXPECT_EQ(fit(samples), "pw(case(cons(-1, cons(-2, nil)), 7), fold(h + acc, 0, xs))");
}

TEST_F(ListFitRuntimeIntegrationTest, SumsOfLongListsReduceToAFold) {
    // 12 lists of 20 to 40 elements, with their tails
    EXPECT_EQ(fit(sample_scalar(with_tails(random_lists(12, 20, 40, -50, 50, 1)), sum)), "fold(h + acc, 0, xs)");
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
              "fold(h * acc, 1, xs)");
}

TEST_F(ListFitRuntimeIntegrationTest, MapsOverLongListsReduceWithoutTails) {
    EXPECT_EQ(fit(sample_listed(random_lists(30, 20, 40, -50, 50, 3), [](const std::vector<integer>& list) {
                  std::vector<integer> result;
                  for (const integer& value : list)
                      result.push_back(2 * value * value - 3);
                  return result;
              })),
              "map(2 * e^2 - 3, xs)");
}

TEST_F(ListFitRuntimeIntegrationTest, FiltersOverLongListsReduceWithoutTails) {
    EXPECT_EQ(fit(sample_listed(random_lists(30, 20, 40, -50, 50, 4), [](const std::vector<integer>& list) {
                  std::vector<integer> kept;
                  for (const integer& value : list)
                      if (value < 17)
                          kept.push_back(value);
                  return kept;
              })),
              "filter(lt(e, 17), xs)");
}
