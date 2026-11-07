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
        list_fit_runtime runtime{samples, k_poly_default_limits, k_poly_fit_default_limits, 20000};
        while (runtime.rewrites() < 3000 && runtime.next()) {}
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
