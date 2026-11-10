#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "composition/poly_fit_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"

namespace {

struct AbsReducerIntegrationTest : ::testing::Test {
    poly_fit_manifest manifest{1, {{.inputs = {0}, .output = 0}}, k_poly_default_limits, k_poly_fit_default_limits,
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
    // `#0` in the argument is free here: bound further out, it stays itself
    EXPECT_EQ(reduced(app(lam(parse("#0^2 + 1")), parse("#0 + 3"))), "(#0 + 3)^2 + 1");
}

TEST_F(AbsReducerIntegrationTest, ALetBindsAHelperThatItsBodyCallsByIndex) {
    // in the let's scope `#0` is the helper and `#1` the input; once reduced, the input is `#0` again
    const expr* helper = lam(parse("#0^2 + 1"));
    EXPECT_EQ(reduced(let(helper, app(parse("#0"), app(parse("#0"), parse("#1"))))), "(#0^2 + 1)^2 + 1");
}

TEST_F(AbsReducerIntegrationTest, AnIndexPastTheBinderIsShiftedWhenTheLambdaGoes) {
    // under the inner lam, `#1` is the helper and `#0` the inner parameter; the input is `#1` in the let's scope
    const expr* helper = lam(parse("2 * #0"));
    const expr* twice = lam(app(parse("#1"), app(parse("#1"), parse("#0"))));
    EXPECT_EQ(reduced(let(helper, app(twice, parse("#1")))), "2 * (2 * #0)");
}

TEST_F(AbsReducerIntegrationTest, ALamsTakesItsFirstArgumentAsItsOutermostIndex) {
    const expr* difference = kernel.exprs_.make_functor(rules.abs_symbols_.lambdas,
                                                        {kernel.literals_.make_literal(2), parse("#1 - #0")});
    EXPECT_EQ(reduced(app(app(difference, parse("7")), parse("3"))), "7 - 3");
}

TEST_F(AbsReducerIntegrationTest, AnExpressionWithoutHelpersIsUnchanged) {
    EXPECT_EQ(reduced(parse("#0^2 + 1")), "#0^2 + 1");
}

TEST_F(AbsReducerIntegrationTest, AReductionThatDoesNotFinishHasNoResult) {
    const expr* self = lam(app(parse("#0"), parse("#0")));
    EXPECT_EQ(reduced(app(self, self)), "(none)");
}

TEST_F(AbsReducerIntegrationTest, AClosedModelWithAHelperIsAppliedToTheInput) {
    const expr* helper = lam(parse("#0^2 + 1"));
    const expr* model = lam(let(helper, app(parse("#0"), app(parse("#0"), parse("#1")))));
    const expr* input = kernel.exprs_.make_functor(rules.point_symbol_, {kernel.literals_.make_literal(2)});
    EXPECT_EQ(rules.pw_evaluator_.evaluate_at(model, input), std::optional<const expr*>{kernel.literals_.make_literal(26)});
}
