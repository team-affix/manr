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
    std::optional<const expr*> evaluate(const expr* e, const std::vector<int>& input) {
        return rules.evaluator_.evaluate_at(e, list(input));
    }
};

}

TEST_F(ListEvaluatorIntegrationTest, TheInputIsTheInputList) {
    EXPECT_EQ(evaluate(xs, {1, 2}), list({1, 2}));
}

TEST_F(ListEvaluatorIntegrationTest, AMapAppliesItsBodyToEveryElement) {
    EXPECT_EQ(evaluate(program(symbols.map, {parse("e^2 + 1"), xs}), {1, -2, 3}), list({2, 5, 10}));
}

TEST_F(ListEvaluatorIntegrationTest, AFilterKeepsTheElementsWhereItsConditionHolds) {
    const expr* positive = program(rules.less_symbol_, {parse("0"), parse("e")});
    EXPECT_EQ(evaluate(program(symbols.filter, {positive, xs}), {3, -1, 0, 2}), list({3, 2}));
}

TEST_F(ListEvaluatorIntegrationTest, AFoldRunsItsStepFromTheRight) {
    // 2 * acc + h reads the list as binary digits, least significant first
    EXPECT_EQ(evaluate(program(symbols.fold, {parse("2 * acc + h"), parse("0"), xs}), {1, 1, 0, 1}),
              kernel.literals_.make_literal(11));
}

TEST_F(ListEvaluatorIntegrationTest, AFoldOfTheEmptyListIsItsBase) {
    EXPECT_EQ(evaluate(program(symbols.fold, {parse("h + acc"), parse("7"), xs}), {}), kernel.literals_.make_literal(7));
}

TEST_F(ListEvaluatorIntegrationTest, ProgramsCompose) {
    const expr* squares = program(symbols.map, {parse("e^2"), xs});
    EXPECT_EQ(evaluate(program(symbols.fold, {parse("h + acc"), parse("0"), squares}), {1, 2, 3}),
              kernel.literals_.make_literal(14));
}

TEST_F(ListEvaluatorIntegrationTest, AFoldStepMayBeAPiecewise) {
    const expr* larger = program(rules.pw_symbols_.piecewise,
                                 {program(rules.pw_symbols_.region, {program(rules.less_symbol_, {parse("acc"), parse("h")}),
                                                                     parse("h")}),
                                  parse("acc")});
    EXPECT_EQ(evaluate(program(symbols.fold, {larger, parse("0"), xs}), {3, 9, 4}), kernel.literals_.make_literal(9));
}

TEST_F(ListEvaluatorIntegrationTest, AnExtensionTermHasNoValue) {
    EXPECT_EQ(evaluate(kernel.exprs_.make_var(0), {1}), std::nullopt);
}

TEST_F(ListEvaluatorIntegrationTest, ABodyOutsideTheElementDomainHasNoValue) {
    EXPECT_EQ(evaluate(program(symbols.map, {parse("h"), xs}), {1}), std::nullopt);
}

TEST_F(ListEvaluatorIntegrationTest, ARegionOverListsComparesTheirValues) {
    const expr* length = program(symbols.fold, {parse("acc + 1"), parse("0"), xs});
    EXPECT_EQ(rules.evaluator_.holds_at(program(rules.less_symbol_, {length, parse("3")}), list({5, 5})),
              std::optional<bool>{true});
}
