#include <map>
#include <optional>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "composition/poly_fit_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"

namespace {

struct PolyPointEvaluatorIntegrationTest : ::testing::Test {
    poly_fit_manifest manifest{2, {}, k_poly_default_limits, k_poly_fit_default_limits, 1};
    poly_kernel_manifest& kernel = manifest.kernel_;

    const expr* point(int x, int y) {
        return kernel.exprs_.make_functor(manifest.rule_set_.point_symbol_,
                                          {kernel.literals_.make_literal(x), kernel.literals_.make_literal(y)});
    }

    std::optional<integer> evaluate(const expr* e, const expr* at) {
        const std::optional<const expr*> value = manifest.rule_set_.fit_.evaluator_.evaluate_at(e, at);
        if (!value.has_value())
            return std::nullopt;
        return kernel.literals_.find_value(value.value());
    }
};

}

TEST_F(PolyPointEvaluatorIntegrationTest, EveryShapeOfTheSamePolynomialHasTheSameValue) {
    EXPECT_EQ(evaluate(kernel.parser_.parse("(#1 + #0)^2 - 3*#1"), point(2, -5)), integer(3));
    EXPECT_EQ(evaluate(kernel.parser_.parse("#1^2 + 2*#1*#0 + #0^2 - 3*#1"), point(2, -5)), integer(3));
}

TEST_F(PolyPointEvaluatorIntegrationTest, AnExtensionTermOrAnotherVariableHasNoValue) {
    EXPECT_FALSE(evaluate(kernel.exprs_.make_var(0), point(1, 1)).has_value());
    EXPECT_FALSE(evaluate(kernel.parser_.parse("#1 + z"), point(1, 1)).has_value());
}

TEST_F(PolyPointEvaluatorIntegrationTest, APointOfTheWrongArityHasNoValue) {
    const expr* one = kernel.exprs_.make_functor(manifest.rule_set_.point_symbol_, {kernel.literals_.make_literal(1)});
    EXPECT_FALSE(evaluate(kernel.parser_.parse("#1"), one).has_value());
}

TEST_F(PolyPointEvaluatorIntegrationTest, ACoordinateHoldingAHoleGivesTheResidual) {
    const expr* at = kernel.exprs_.make_functor(manifest.rule_set_.point_symbol_,
                                                {kernel.exprs_.make_var(1), kernel.literals_.make_literal(2)});
    const std::optional<const expr*> value =
        manifest.rule_set_.fit_.evaluator_.evaluate_at(kernel.parser_.parse("#1^2 + 3*#0"), at);
    ASSERT_TRUE(value.has_value());
    std::map<uint32_t, const expr*> bindings;
    // ?1^2 + 6 = 22 at ?1 = 4
    EXPECT_TRUE(manifest.rule_set_.piecewise_.matcher_.match(kernel.literals_.make_literal(22), value.value(), bindings));
    EXPECT_EQ(bindings.at(1), kernel.literals_.make_literal(4));
}

TEST_F(PolyPointEvaluatorIntegrationTest, ACoordinateThatIsNeitherALiteralNorHoldsAHoleHasNoValue) {
    const expr* at = kernel.exprs_.make_functor(manifest.rule_set_.point_symbol_,
                                                {kernel.parser_.parse("z"), kernel.literals_.make_literal(2)});
    EXPECT_FALSE(evaluate(kernel.parser_.parse("#1 + #0"), at).has_value());
}
