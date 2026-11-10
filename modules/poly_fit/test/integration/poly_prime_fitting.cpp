#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/num_prime_table.hpp"
#include "infrastructure/poly_fit_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"

using ::testing::Contains;

namespace {

// n -> 1 if n is prime, else 0, for n = 0..40
std::vector<poly_sample> primality_samples() {
    num_prime_table table{k_num_default_max_value};
    std::vector<poly_sample> samples;
    for (int n = 0; n <= 40; ++n)
        samples.push_back(poly_sample{.inputs = {n}, .output = table.find_primality(n).value() ? 1 : 0});
    return samples;
}

struct PolyPrimeFittingIntegrationTest : ::testing::Test {
    poly_fit_manifest manifest{1, primality_samples(), k_poly_default_limits, k_poly_fit_default_limits, 1};
    poly_fit_rules_manifest& rules = manifest.rule_set_;
    std::vector<rewrite> out{};

    std::vector<std::string> performed() {
        std::vector<std::string> texts;
        for (const rewrite& r : out) {
            std::ostringstream os;
            manifest.kernel_.printer_.print(os, r());
            texts.push_back(os.str());
        }
        return texts;
    }
};

}

TEST_F(PolyPrimeFittingIntegrationTest, PrimalityDataIsBoundToIsPrime) {
    rules.binder_.detect(rules.initial_, out);
    EXPECT_THAT(performed(), Contains("lam(is_prime(#0))"));
}

TEST_F(PolyPrimeFittingIntegrationTest, AFittedIsPrimeEvaluatesThroughTheApplyingEvaluator) {
    const expr* body = manifest.kernel_.exprs_.make_functor(rules.num_symbols_.is_prime,
                                                            {manifest.kernel_.parser_.parse("#0 + 1")});
    const expr* function = manifest.kernel_.exprs_.make_functor(rules.abs_symbols_.lambda, {body});
    const expr* at_six = manifest.kernel_.exprs_.make_functor(rules.point_symbol_,
                                                              {manifest.kernel_.literals_.make_literal(6)});
    EXPECT_EQ(rules.pw_evaluator_.evaluate_at(function, at_six),
              std::optional<const expr*>(manifest.kernel_.literals_.make_literal(1)));
}
