#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "infrastructure/poly_fit_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"

namespace {

struct PolyConditionEvaluatorIntegrationTest : ::testing::Test {
    using kernel_t = poly_kernel_manifest;
    poly_fit_manifest manifest{{"x", "y"}, {{.inputs = {0, 0}, .output = 0}}, k_poly_default_limits,
                               k_poly_fit_default_limits, 1};
    poly_fit_rules_manifest& rules = manifest.rule_set_;
    const expr* point(int x, int y) {
        return manifest.kernel_.exprs_.make_functor(
            rules.point_symbol_, {manifest.kernel_.literals_.make_literal(x), manifest.kernel_.literals_.make_literal(y)});
    }
    const expr* less(const std::string& left, const std::string& right) {
        return manifest.kernel_.exprs_.make_functor(
            rules.less_symbol_, {manifest.kernel_.parser_.parse(left), manifest.kernel_.parser_.parse(right)});
    }
};

}

TEST_F(PolyConditionEvaluatorIntegrationTest, LessThanComparesBothSidesAtTheInput) {
    EXPECT_EQ(rules.fit_.conditions_.holds_at(less("x", "2"), point(1, 0)), std::optional<bool>{true});
    EXPECT_EQ(rules.fit_.conditions_.holds_at(less("x", "2"), point(2, 0)), std::optional<bool>{false});
    EXPECT_EQ(rules.fit_.conditions_.holds_at(less("x * y", "x + y"), point(2, 3)), std::optional<bool>{false});
}

TEST_F(PolyConditionEvaluatorIntegrationTest, EqualityOfAResidueDecidesParityOfNegativesToo) {
    kernel_t& kernel = manifest.kernel_;
    const expr* remainder = kernel.exprs_.make_functor(rules.fit_.modulo_symbol_,
                                                       {kernel.parser_.parse("x"), kernel.literals_.make_literal(2)});
    const expr* even = kernel.exprs_.make_functor(rules.fit_.equal_symbol_, {remainder, kernel.literals_.make_literal(0)});
    EXPECT_EQ(rules.fit_.conditions_.holds_at(even, point(-4, 0)), std::optional<bool>{true});
    EXPECT_EQ(rules.fit_.conditions_.holds_at(even, point(-3, 0)), std::optional<bool>{false});
    EXPECT_EQ(rules.fit_.conditions_.holds_at(even, point(6, 0)), std::optional<bool>{true});
}

TEST_F(PolyConditionEvaluatorIntegrationTest, AnythingButAComparisonIsUndecided) {
    EXPECT_EQ(rules.fit_.conditions_.holds_at(manifest.kernel_.parser_.parse("x"), point(1, 0)), std::nullopt);
}

TEST_F(PolyConditionEvaluatorIntegrationTest, APiecewiseIsEvaluatedThroughItsRegions) {
    const expr* piecewise = manifest.kernel_.exprs_.make_functor(
        rules.pw_symbols_.piecewise,
        {manifest.kernel_.exprs_.make_functor(rules.pw_symbols_.region, {less("x", "0"), manifest.kernel_.parser_.parse("x * y")}),
         manifest.kernel_.parser_.parse("y")});
    EXPECT_EQ(rules.fit_.pw_evaluator_.evaluate_at(piecewise, point(-2, 5)),
              std::optional<const expr*>{manifest.kernel_.literals_.make_literal(-10)});
    EXPECT_EQ(rules.fit_.pw_evaluator_.evaluate_at(piecewise, point(2, 5)),
              std::optional<const expr*>{manifest.kernel_.literals_.make_literal(5)});
}
