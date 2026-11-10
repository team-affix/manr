#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "composition/poly_fit_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"

namespace {

// Data with several guarded branches, each a constant under a condition, before a model.
struct PolyGuardedFittingIntegrationTest : ::testing::Test {
    struct fit {
        size_t cost;
        std::string best;
    };
    static fit run(const std::vector<poly_sample>& samples) {
        poly_fit_manifest manifest{1, samples, k_poly_default_limits, k_poly_fit_default_limits, 20000};
        while (manifest.search_.rewrites() < 3000 && manifest.search_.next()) {}
        std::ostringstream os;
        manifest.kernel_.printer_.print(os, manifest.search_.best());
        return fit{.cost = manifest.search_.best_cost(), .best = os.str()};
    }
};

}

TEST_F(PolyGuardedFittingIntegrationTest, TwoConstantsUnderConditionsBeforeAModelReadAsThreeBranches) {
    // reduce request #30: 1 on the evens, 2 on the other multiples of 3, #0^2 elsewhere
    std::vector<poly_sample> samples;
    for (int x = 0; x <= 30; ++x)
        samples.push_back(poly_sample{.inputs = {x}, .output = x % 2 == 0 ? 1 : x % 3 == 0 ? 2 : x * x});
    const fit reached = run(samples);
    EXPECT_LE(reached.cost, 19u) << reached.best;
}

TEST_F(PolyGuardedFittingIntegrationTest, AConstantUnderAConditionBeforeAGuardedModelNests) {
    // 0 at the primes, 5 at multiples of 4, 2 * #0 + 1 elsewhere
    std::vector<poly_sample> samples;
    for (int x = 0; x <= 40; ++x) {
        bool prime = x >= 2;
        for (int divisor = 2; divisor * divisor <= x; ++divisor)
            prime = prime && x % divisor != 0;
        samples.push_back(poly_sample{.inputs = {x}, .output = prime ? 0 : x % 4 == 0 ? 5 : 2 * x + 1});
    }
    const fit reached = run(samples);
    // lam(pw(when(is_prime(#0), 0), when(eq(mod(#0, 4), 0), 5), 2 * #0 + 1))
    EXPECT_LE(reached.cost, 19u) << reached.best;
}
