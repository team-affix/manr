#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "infrastructure/poly_fit_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"

namespace {

struct AbsReducerIntegrationTest : ::testing::Test {
    poly_fit_manifest manifest{{"x"}, {{.inputs = {0}, .output = 0}}, k_poly_default_limits, k_poly_fit_default_limits,
                               1};
    poly_fit_rules_manifest& rules = manifest.rule_set_;
    poly_kernel_manifest& kernel = manifest.kernel_;

    const expr* parse(const std::string& text) { return kernel.parser_.parse(text); }
    const expr* lam(const expr* body) { return kernel.exprs_.make_functor(rules.abs_symbols_.lambda, {body}); }
    const expr* app(const expr* f, const expr* a) {
        return kernel.exprs_.make_functor(rules.abs_symbols_.application, {f, a});
    }
    const expr* let(const expr* value, const expr* body) {
        return kernel.exprs_.make_functor(rules.abs_symbols_.binding, {value, body});
    }
    std::string reduced(const expr* e) {
        const std::optional<const expr*> result = rules.reducer_.reduce(e);
        if (!result.has_value())
            return "(none)";
        std::ostringstream os;
        kernel.printer_.print(os, result.value());
        return os.str();
    }
};

}

TEST_F(AbsReducerIntegrationTest, AnApplicationOfALambdaSubstitutesItsArgument) {
    EXPECT_EQ(reduced(app(lam(parse("#0^2 + 1")), parse("x + 3"))), "(x + 3)^2 + 1");
}

TEST_F(AbsReducerIntegrationTest, ALetBindsAHelperThatItsBodyCallsByIndex) {
    const expr* helper = lam(parse("#0^2 + 1"));
    EXPECT_EQ(reduced(let(helper, app(parse("#0"), app(parse("#0"), parse("x"))))), "(x^2 + 1)^2 + 1");
}

TEST_F(AbsReducerIntegrationTest, AnIndexPastTheBinderIsShiftedWhenTheLambdaGoes) {
    // inside the let, `#1` under the inner lam is the helper; the helper's body keeps its own `#0`
    const expr* helper = lam(parse("2 * #0"));
    const expr* twice = lam(app(parse("#1"), app(parse("#1"), parse("#0"))));
    EXPECT_EQ(reduced(let(helper, app(twice, parse("x")))), "2 * (2 * x)");
}

TEST_F(AbsReducerIntegrationTest, AnExpressionWithoutHelpersIsUnchanged) {
    EXPECT_EQ(reduced(parse("x^2 + 1")), "x^2 + 1");
}

TEST_F(AbsReducerIntegrationTest, AReductionThatDoesNotFinishHasNoResult) {
    const expr* self = lam(app(parse("#0"), parse("#0")));
    EXPECT_EQ(reduced(app(self, self)), "(none)");
}

TEST_F(AbsReducerIntegrationTest, TheEvaluatorReducesHelpersBeforeThePolynomialEvaluator) {
    const expr* helper = lam(parse("#0^2 + 1"));
    const expr* twice = let(helper, app(parse("#0"), app(parse("#0"), parse("x"))));
    const expr* input = kernel.exprs_.make_functor(rules.point_symbol_, {kernel.literals_.make_literal(2)});
    EXPECT_EQ(rules.pw_evaluator_.evaluate_at(twice, input), std::optional<const expr*>{kernel.literals_.make_literal(26)});
}
