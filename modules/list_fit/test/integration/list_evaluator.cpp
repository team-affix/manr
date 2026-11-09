#include <optional>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "infrastructure/list_fit_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"

namespace {

struct ListEvaluatorIntegrationTest : ::testing::Test {
    list_fit_manifest manifest{{}, k_poly_default_limits, k_poly_fit_default_limits, 1};
    poly_kernel_manifest& kernel = manifest.kernel_;
    list_fit_rules_manifest& rules = manifest.rule_set_;
    const list_symbols& symbols = rules.list_symbols_;
    const expr* xs = kernel.exprs_.make_functor(symbols.input, {});

    const expr* parse(const std::string& text) { return kernel.parser_.parse(text); }
    const expr* list(const std::vector<int>& values) {
        std::vector<const expr*> elements;
        for (const int value : values)
            elements.push_back(kernel.literals_.make_literal(value));
        return rules.values_.make_list(elements);
    }
    const expr* program(uint32_t id, const std::vector<const expr*>& args) {
        return kernel.exprs_.make_functor(id, args);
    }
    // a closed function of one or two arguments: `lam(body)`, `lam(lam(body))`
    const expr* lam(const std::string& body) {
        return program(symbols.lambda, {parse(body)});
    }
    const expr* lam2(const expr* body) {
        return program(symbols.lambda, {program(symbols.lambda, {body})});
    }
    std::optional<const expr*> evaluate(const expr* e, const std::vector<int>& input) {
        return rules.evaluator_.evaluate_at(e, list(input));
    }
};

}

TEST_F(ListEvaluatorIntegrationTest, TheInputIsTheInputList) {
    EXPECT_EQ(evaluate(xs, {1, 2}), list({1, 2}));
}

TEST_F(ListEvaluatorIntegrationTest, AMapAppliesItsBodyToEveryElement) {
    EXPECT_EQ(evaluate(program(symbols.map, {lam("#0^2 + 1"), xs}), {1, -2, 3}), list({2, 5, 10}));
}

TEST_F(ListEvaluatorIntegrationTest, AFilterKeepsTheElementsWhereItsConditionHolds) {
    const expr* positive = program(symbols.lambda, {program(rules.less_symbol_, {parse("0"), parse("#0")})});
    EXPECT_EQ(evaluate(program(symbols.filter, {positive, xs}), {3, -1, 0, 2}), list({3, 2}));
}

TEST_F(ListEvaluatorIntegrationTest, AFoldRunsItsStepFromTheRight) {
    // 2 * #1 + #0 (twice the accumulator plus the head) reads the list as binary digits, least significant first
    EXPECT_EQ(evaluate(program(symbols.fold, {lam2(parse("2 * #1 + #0")), parse("0"), xs}), {1, 1, 0, 1}),
              kernel.literals_.make_literal(11));
}

TEST_F(ListEvaluatorIntegrationTest, AFoldOfTheEmptyListIsItsBase) {
    EXPECT_EQ(evaluate(program(symbols.fold, {lam2(parse("#0 + #1")), parse("7"), xs}), {}), kernel.literals_.make_literal(7));
}

TEST_F(ListEvaluatorIntegrationTest, ProgramsCompose) {
    const expr* squares = program(symbols.map, {lam("#0^2"), xs});
    EXPECT_EQ(evaluate(program(symbols.fold, {lam2(parse("#0 + #1")), parse("0"), squares}), {1, 2, 3}),
              kernel.literals_.make_literal(14));
}

TEST_F(ListEvaluatorIntegrationTest, AFoldStepMayBeAPiecewise) {
    const expr* larger = program(rules.pw_symbols_.piecewise,
                                 {program(rules.pw_symbols_.region, {program(rules.less_symbol_, {parse("#1"), parse("#0")}),
                                                                     parse("#0")}),
                                  parse("#1")});
    EXPECT_EQ(evaluate(program(symbols.fold, {lam2(larger), parse("0"), xs}), {3, 9, 4}), kernel.literals_.make_literal(9));
}

TEST_F(ListEvaluatorIntegrationTest, AppendJoinsTwoLists) {
    const expr* doubled = program(symbols.append, {xs, xs});
    EXPECT_EQ(evaluate(doubled, {1, 2}), list({1, 2, 1, 2}));
}

TEST_F(ListEvaluatorIntegrationTest, AFoldStepThatBuildsAListIsEvaluatedByTheListEvaluator) {
    const expr* nil = program(symbols.nil, {});
    const expr* step = program(symbols.append, {parse("#1"), program(symbols.cons, {parse("#0"), nil})});
    EXPECT_EQ(evaluate(program(symbols.fold, {lam2(step), nil, xs}), {1, 2, 3}), list({3, 2, 1}));
}

TEST_F(ListEvaluatorIntegrationTest, ReverseReversesAList) {
    EXPECT_EQ(evaluate(program(symbols.reverse, {xs}), {1, 2, 3}), list({3, 2, 1}));
}

TEST_F(ListEvaluatorIntegrationTest, AnExtensionTermHasNoValue) {
    EXPECT_EQ(evaluate(kernel.exprs_.make_var(0), {1}), std::nullopt);
}

TEST_F(ListEvaluatorIntegrationTest, ABodyWithAnIndexItsFunctionDoesNotBindHasNoValue) {
    EXPECT_EQ(evaluate(program(symbols.map, {lam("#1"), xs}), {1}), std::nullopt);
}

TEST_F(ListEvaluatorIntegrationTest, AProgramWhoseFunctionIsNotALambdaHasNoValue) {
    EXPECT_EQ(evaluate(program(symbols.map, {parse("#0"), xs}), {1}), std::nullopt);
    EXPECT_EQ(evaluate(program(symbols.fold, {lam("#0 + #1"), parse("0"), xs}), {1}), std::nullopt);
}

TEST_F(ListEvaluatorIntegrationTest, ARegionOverListsComparesTheirValues) {
    const expr* length = program(symbols.fold, {lam2(parse("#1 + 1")), parse("0"), xs});
    EXPECT_EQ(rules.evaluator_.holds_at(program(rules.less_symbol_, {length, parse("3")}), list({5, 5})),
              std::optional<bool>{true});
}
