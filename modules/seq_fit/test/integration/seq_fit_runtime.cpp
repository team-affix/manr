#include <vector>
#include <gtest/gtest.h>
#include "infrastructure/seq_fit_runtime.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"
#include "seq_fit_fixture.hpp"

namespace {

struct SeqFitRuntimeIntegrationTest : ::testing::Test {
    void run(seq_fit_runtime& runtime) {
        while (runtime.rewrites() < 500 && runtime.next()) {}
    }
};

integer fibonacci_step(const integer&, const integer& previous, const integer& before) {
    return previous + before;
}

integer doubling_step(const integer&, const integer& previous, const integer&) {
    return 2 * previous;
}

integer mersenne_step(const integer&, const integer& previous, const integer&) {
    return 2 * previous + 1;
}

}

TEST_F(SeqFitRuntimeIntegrationTest, FibonacciReducesToASecondOrderRecurrence) {
    seq_fit_runtime runtime{"n", recurrence_samples({0, 1}, fibonacci_step, 0, 20),
                            k_poly_default_limits, k_poly_fit_default_limits, 20000};
    run(runtime);
    EXPECT_EQ(runtime.best(), "rec(p1 + p2, 0, 1, n)");
    EXPECT_EQ(runtime.best_cost(), 7u);
}

TEST_F(SeqFitRuntimeIntegrationTest, PowersOfTwoReduceToAPower) {
    seq_fit_runtime runtime{"n", recurrence_samples({1}, doubling_step, 0, 20),
                            k_poly_default_limits, k_poly_fit_default_limits, 20000};
    run(runtime);
    EXPECT_EQ(runtime.best(), "exp(2, n)");
    EXPECT_EQ(runtime.best_cost(), 3u);
}

TEST_F(SeqFitRuntimeIntegrationTest, AnAffineRecurrenceReducesToAShiftedPower) {
    seq_fit_runtime runtime{"n", recurrence_samples({0}, mersenne_step, 0, 20),
                            k_poly_default_limits, k_poly_fit_default_limits, 20000};
    run(runtime);
    EXPECT_EQ(runtime.best(), "exp(2, n) - 1");
}

TEST_F(SeqFitRuntimeIntegrationTest, PolynomialSequencesStillReduceToPolynomials) {
    std::vector<poly_sample> samples;
    for (int at = 0; at <= 12; ++at)
        samples.push_back(poly_sample{.inputs = {at}, .output = at * at + 1});
    seq_fit_runtime runtime{"n", samples,
                            k_poly_default_limits, k_poly_fit_default_limits, 20000};
    run(runtime);
    EXPECT_EQ(runtime.best(), "n^2 + 1");
}
