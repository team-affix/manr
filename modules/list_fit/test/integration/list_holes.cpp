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

struct ListHolesIntegrationTest : ::testing::Test {
    list_fit_manifest manifest{{}, k_poly_default_limits, k_poly_fit_default_limits, 1};
    list_fit_rules_manifest& rules = manifest.rule_set_;
    std::vector<rewrite> out{};

    const expr* literal(int value) { return manifest.kernel_.literals_.make_literal(value); }

    // squares of every list of up to 2 elements in [-2, 2], plus the given case, under an open catch-all
    const expr* squares_with(const expr* input, const expr* output) {
        std::vector<pw_data_point> points = rules.converter_.convert(sample_listed(all_lists(-2, 2, 2), squared));
        points.push_back(pw_data_point{.input = input, .output = output});
        return rules.builder_.build(points, 0);
    }
    std::vector<std::string> performed() {
        std::vector<std::string> texts;
        for (const rewrite& r : out) {
            std::ostringstream os;
            manifest.kernel_.printer_.print(os, r());
            texts.push_back(os.str());
        }
        return texts;
    }
    static std::vector<integer> squared(const std::vector<integer>& list) {
        std::vector<integer> result;
        for (const integer& value : list)
            result.push_back(value * value);
        return result;
    }
};

}

TEST_F(ListHolesIntegrationTest, AnOutputListHoldingAHoleIsExplainedByTheModel) {
    const expr* input = rules.values_.make_list({literal(3), literal(4), literal(1)});
    const expr* output = rules.values_.make_list({literal(9), manifest.kernel_.exprs_.make_var(1), literal(1)});
    rules.binder_.detect(squares_with(input, output), out);
    EXPECT_THAT(performed(), Contains("lam(map(lam(#0^2), #0))"));
}

TEST_F(ListHolesIntegrationTest, AnOutputListHoldingAHoleTheModelCannotMatchStays) {
    const expr* input = rules.values_.make_list({literal(3), literal(4), literal(1)});
    const expr* output = rules.values_.make_list({literal(8), manifest.kernel_.exprs_.make_var(1), literal(1)});
    rules.binder_.detect(squares_with(input, output), out);
    EXPECT_THAT(performed(), Contains("pw(case(cons(3, cons(4, cons(1, nil))), cons(8, cons(?1, cons(1, nil)))), "
                                      "lam(map(lam(#0^2), #0)))"));
}
