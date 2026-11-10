#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/seq_fit_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"
#include "seq_fit_fixture.hpp"

using ::testing::Contains;

namespace {

struct SeqDetectorsIntegrationTest : ::testing::Test {
    seq_fit_manifest manifest{{}, k_poly_default_limits, k_poly_fit_default_limits, 1};
    seq_fit_rules_manifest& rules = manifest.rule_set_;
    poly_kernel_manifest& kernel = manifest.kernel_;
    std::vector<const expr*> out{};

    std::vector<pw_data_point> points(const std::vector<poly_sample>& samples) {
        return rules.fit_.converter_.convert(samples);
    }
    std::string print(const expr* e) {
        std::ostringstream os;
        kernel.printer_.print(os, e);
        return os.str();
    }
    std::vector<std::string> printed() {
        std::vector<std::string> texts;
        for (const expr* e : out)
            texts.push_back(print(e));
        return texts;
    }
    const expr* literal(int value) {
        return kernel.literals_.make_literal(value);
    }
    const expr* index() {
        return kernel.parser_.parse("#0");
    }
    // `rec` of the step body `step`, closed under one lambda for the index and one per base
    const expr* recurrence(const std::string& step, const std::vector<int>& bases) {
        std::vector<const expr*> args{kernel.parser_.parse(step)};
        for (size_t binder = 0; binder <= bases.size(); ++binder)
            args[0] = kernel.exprs_.make_functor(rules.seq_symbols_.lambda, {args[0]});
        for (const int base : bases)
            args.push_back(literal(base));
        args.push_back(index());
        return kernel.exprs_.make_functor(rules.seq_symbols_.recurrence, args);
    }
    const expr* power(int base) {
        return kernel.exprs_.make_functor(rules.seq_symbols_.power, {literal(base), index()});
    }
    std::optional<integer> value_at(const expr* e, int at) {
        const expr* input = kernel.exprs_.make_functor(rules.point_symbol_, {literal(at)});
        const std::optional<const expr*> value = rules.pw_evaluator_.evaluate_at(e, input);
        if (!value.has_value())
            return std::nullopt;
        return kernel.literals_.find_value(value.value());
    }
    std::vector<std::string> closed(const expr* e) {
        std::vector<rewrite> rewrites;
        rules.closer_.detect(e, rewrites);
        std::vector<std::string> texts;
        for (const rewrite& detected : rewrites)
            texts.push_back(print(detected()));
        return texts;
    }
};

integer fibonacci_step(const integer&, const integer& previous, const integer& before) {
    return previous + before;
}

integer doubling_step(const integer&, const integer& previous, const integer&) {
    return 2 * previous;
}

integer factorial_step(const integer& at, const integer& previous, const integer&) {
    return at * previous;
}

}

TEST_F(SeqDetectorsIntegrationTest, TheEvaluatorRunsARecurrenceFromItsBases) {
    EXPECT_EQ(value_at(recurrence("#1 + #2", {0, 1}), 10), integer(55));
    EXPECT_EQ(value_at(recurrence("#1 + #2", {0, 1}), 1), integer(1));
}

TEST_F(SeqDetectorsIntegrationTest, TheEvaluatorBindsTheIndexInTheStep) {
    EXPECT_EQ(value_at(recurrence("#0 * #1", {1}), 5), integer(120));
}

TEST_F(SeqDetectorsIntegrationTest, TheEvaluatorSeesZeroForTermsBeforeTheStart) {
    EXPECT_EQ(value_at(recurrence("#1 + #2", {1}), 3), integer(3));
}

TEST_F(SeqDetectorsIntegrationTest, TheEvaluatorHasNoValueAtANegativeIndex) {
    EXPECT_FALSE(value_at(recurrence("2 * #1", {1}), -1).has_value());
    EXPECT_FALSE(value_at(power(2), -1).has_value());
}

TEST_F(SeqDetectorsIntegrationTest, TheEvaluatorRaisesAPower) {
    EXPECT_EQ(value_at(power(-2), 5), integer(-32));
}

TEST_F(SeqDetectorsIntegrationTest, TheEvaluatorEvaluatesArithmeticOverAPower) {
    const expr* scaled = kernel.exprs_.make_functor(kernel.poly_symbols_.mul, {literal(5), power(3)});
    const expr* shifted = kernel.exprs_.make_functor(kernel.poly_symbols_.sub, {scaled, literal(1)});
    EXPECT_EQ(value_at(shifted, 2), integer(44));
}

TEST_F(SeqDetectorsIntegrationTest, TheEvaluatorLeavesPolynomialsToThePolynomialDomain) {
    EXPECT_EQ(value_at(kernel.parser_.parse("#0^2 + 1"), 3), integer(10));
}

TEST_F(SeqDetectorsIntegrationTest, TheSecondOrderDetectorReadsFibonacci) {
    rules.second_order_.propose_hypotheses(points(recurrence_samples({0, 1}, fibonacci_step, 0, 15)), out);
    EXPECT_THAT(printed(), Contains("rec(lam(lam(lam(#1 + #2))), 0, 1, #0)"));
}

TEST_F(SeqDetectorsIntegrationTest, TheFirstOrderDetectorReadsAStepThatUsesTheIndex) {
    rules.first_order_.propose_hypotheses(points(recurrence_samples({1}, factorial_step, 0, 10)), out);
    EXPECT_THAT(printed(), Contains("rec(lam(lam(#0 * #1)), 1, #0)"));
}

TEST_F(SeqDetectorsIntegrationTest, TheFirstOrderDetectorFindsNoStepForFibonacci) {
    rules.first_order_.propose_hypotheses(points(recurrence_samples({0, 1}, fibonacci_step, 0, 15)), out);
    for (const expr* proposal : out)
        EXPECT_NE(value_at(proposal, 15), integer(610)) << print(proposal);
}

TEST_F(SeqDetectorsIntegrationTest, TheDetectorSolvesBasesTheDataDoesNotHaveByRunningTheStepBackwards) {
    rules.second_order_.propose_hypotheses(points(recurrence_samples({0, 1}, fibonacci_step, 10, 30)), out);
    EXPECT_THAT(printed(), Contains("rec(lam(lam(lam(#1 + #2))), 0, 1, #0)"));
}

TEST_F(SeqDetectorsIntegrationTest, TheDetectorRunsAStepThatUsesTheIndexBackwards) {
    rules.first_order_.propose_hypotheses(points(recurrence_samples({1}, factorial_step, 4, 14)), out);
    EXPECT_THAT(printed(), Contains("rec(lam(lam(#0 * #1)), 1, #0)"));
}

TEST_F(SeqDetectorsIntegrationTest, TheDetectorProposesNoRecurrenceWhoseStepCannotBeRunBackwards) {
    // a(j) = a(j-1)^2 + 1 from a(0) = 1, observed from index 2: no term before 5 steps to it linearly
    std::vector<poly_sample> samples;
    integer term = 1;
    for (int position = 0; position <= 7; ++position) {
        if (position >= 2)
            samples.push_back(poly_sample{.inputs = {position}, .output = term});
        term = term * term + 1;
    }
    rules.first_order_.propose_hypotheses(points(samples), out);
    EXPECT_TRUE(out.empty());
}

TEST_F(SeqDetectorsIntegrationTest, TheDetectorLeavesAConstantSequenceAlone) {
    rules.first_order_.propose_hypotheses(
        points({{.inputs = {0}, .output = 7}, {.inputs = {1}, .output = 7}, {.inputs = {2}, .output = 7},
                {.inputs = {3}, .output = 7}, {.inputs = {4}, .output = 7}, {.inputs = {5}, .output = 7}}),
        out);
    EXPECT_TRUE(out.empty());
}

TEST_F(SeqDetectorsIntegrationTest, TheDetectorNeedsMoreTermsThanItsUnknowns) {
    rules.third_order_.propose_hypotheses(points(recurrence_samples({0, 1}, fibonacci_step, 0, 7)), out);
    EXPECT_TRUE(out.empty());
}

TEST_F(SeqDetectorsIntegrationTest, TheDetectorReadsAroundAWrongLabel) {
    std::vector<poly_sample> samples = recurrence_samples({1}, doubling_step, 0, 20);
    samples[9].output += 1;
    rules.first_order_.propose_hypotheses(points(samples), out);
    EXPECT_THAT(printed(), Contains("rec(lam(lam(2 * #1)), 1, #0)"));
}

TEST_F(SeqDetectorsIntegrationTest, TheCloserTurnsAGeometricRecurrenceIntoAPower) {
    EXPECT_THAT(closed(recurrence("2 * #1", {1})), Contains("exp(2, #0)"));
    EXPECT_THAT(closed(recurrence("3 * #1", {5})), Contains("5 * exp(3, #0)"));
}

TEST_F(SeqDetectorsIntegrationTest, TheCloserTurnsAnAffineRecurrenceIntoAShiftedPower) {
    EXPECT_THAT(closed(recurrence("2 * #1 + 1", {0})), Contains("exp(2, #0) - 1"));
    EXPECT_THAT(closed(recurrence("3 * #1 - 4", {5})), Contains("3 * exp(3, #0) + 2"));
}

TEST_F(SeqDetectorsIntegrationTest, TheCloserTurnsARecurrenceStartingAtItsFixedPointIntoTheFixedPoint) {
    EXPECT_THAT(closed(recurrence("2 * #1 + 1", {-1})), Contains("-1"));
}

TEST_F(SeqDetectorsIntegrationTest, TheCloserKeepsTheMeaning) {
    const expr* original = recurrence("3 * #1 - 4", {5});
    std::vector<rewrite> rewrites;
    rules.closer_.detect(original, rewrites);
    ASSERT_EQ(rewrites.size(), 1u);
    const expr* result = rewrites[0]();
    for (int at = -1; at <= 8; ++at)
        EXPECT_EQ(value_at(result, at), value_at(original, at)) << at;
}

TEST_F(SeqDetectorsIntegrationTest, TheCloserLeavesAFractionalFixedPoint) {
    EXPECT_TRUE(closed(recurrence("3 * #1 + 1", {0})).empty());
}

TEST_F(SeqDetectorsIntegrationTest, TheCloserLeavesStepsThatAreNotAffineInThePreviousTerm) {
    EXPECT_TRUE(closed(recurrence("#1 + 1", {0})).empty());
    EXPECT_TRUE(closed(recurrence("#0 * #1", {1})).empty());
    EXPECT_TRUE(closed(recurrence("#1^2", {2})).empty());
    EXPECT_TRUE(closed(recurrence("#1 + #2", {0, 1})).empty());
}
