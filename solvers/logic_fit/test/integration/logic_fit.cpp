#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "composition/logic_fit_manifest.hpp"
#include "composition/logic_fit_runtime.hpp"
#include "value_objects/poly_default_limits.hpp"

using ::testing::Contains;

namespace {

struct LogicFitIntegrationTest : ::testing::Test {
    logic_fit_manifest manifest{3, {}, k_poly_default_limits, 1};
    logic_fit_rules_manifest& rules = manifest.rule_set_;
    poly_kernel_manifest& kernel = manifest.kernel_;
    std::vector<const expr*> out{};

    // every input of three bits, valued by `truth`
    std::vector<poly_sample> table(int (*truth)(int, int, int)) {
        std::vector<poly_sample> samples;
        for (int row = 0; row < 8; ++row) {
            const int a = row & 1;
            const int b = (row >> 1) & 1;
            const int c = (row >> 2) & 1;
            samples.push_back(poly_sample{.inputs = {a, b, c}, .output = truth(a, b, c)});
        }
        return samples;
    }
    std::vector<pw_data_point> points(const std::vector<poly_sample>& samples) {
        return rules.converter_.convert(samples);
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
    const expr* variable(const std::string& name) {
        return kernel.exprs_.make_functor(kernel.symbols_.intern_symbol(name), {});
    }
    std::optional<integer> value_at(const expr* e, int a, int b, int c) {
        const expr* input = kernel.exprs_.make_functor(
            rules.point_symbol_, {kernel.literals_.make_literal(a), kernel.literals_.make_literal(b),
                                  kernel.literals_.make_literal(c)});
        const std::optional<const expr*> value = rules.pw_evaluator_.evaluate_at(e, input);
        if (!value.has_value())
            return std::nullopt;
        return kernel.literals_.find_value(value.value());
    }
};

int majority(int a, int b, int c) {
    return a + b + c >= 2 ? 1 : 0;
}

int parity(int a, int b, int c) {
    return (a + b + c) % 2;
}

int multiplexer(int s, int a, int b) {
    return s == 1 ? a : b;
}

}

TEST_F(LogicFitIntegrationTest, TheEvaluatorComputesEveryConnective) {
    const expr* a = variable("#2");
    const expr* b = variable("#1");
    const logic_symbols& symbols = rules.logic_symbols_;
    const expr* left = kernel.exprs_.make_functor(
        symbols.conjunction, {a, kernel.exprs_.make_functor(symbols.negation, {b})});
    const expr* right = kernel.exprs_.make_functor(symbols.exclusion, {a, variable("#0")});
    const expr* formula = kernel.exprs_.make_functor(symbols.disjunction, {left, right});
    EXPECT_EQ(value_at(formula, 1, 0, 1), integer(1));
    EXPECT_EQ(value_at(formula, 1, 1, 1), integer(0));
    EXPECT_EQ(value_at(formula, 0, 1, 1), integer(1));
}

TEST_F(LogicFitIntegrationTest, TheEvaluatorHasNoValueForAnotherLiteral) {
    EXPECT_FALSE(value_at(kernel.literals_.make_literal(2), 0, 0, 0).has_value());
}

TEST_F(LogicFitIntegrationTest, TheParityDetectorReadsAnXorChain) {
    rules.parities_.propose_hypotheses(points(table(parity)), out);
    EXPECT_THAT(printed(), Contains("xor(xor(#2, #1), #0)"));
}

TEST_F(LogicFitIntegrationTest, TheParityDetectorReadsAroundOneWrongLabel) {
    std::vector<poly_sample> samples = table(parity);
    samples[5].output = 1 - samples[5].output;
    rules.parities_.propose_hypotheses(points(samples), out);
    EXPECT_THAT(printed(), Contains("xor(xor(#2, #1), #0)"));
}

TEST_F(LogicFitIntegrationTest, TheParityDetectorRefutesMajority) {
    rules.parities_.propose_hypotheses(points(table(majority)), out);
    for (const expr* proposal : out)
        EXPECT_NE(print(proposal), "xor(xor(#2, #1), #0)");
}

TEST_F(LogicFitIntegrationTest, TheCoverDetectorFactorsMajority) {
    rules.covers_.propose_hypotheses(points(table(majority)), out);
    EXPECT_THAT(printed(), Contains("or(and(#2, or(#1, #0)), and(#1, #0))"));
}

TEST_F(LogicFitIntegrationTest, TheCoverDetectorReadsAMultiplexer) {
    rules.covers_.propose_hypotheses(points(table(multiplexer)), out);
    EXPECT_THAT(printed(), Contains("or(and(#2, #1), and(not(#2), #0))"));
}

TEST_F(LogicFitIntegrationTest, EveryProposalOnMajorityWithAWrongLabelReproducesEverySample) {
    std::vector<poly_sample> samples = table(majority);
    samples[2].output = 1 - samples[2].output;
    logic_fit_manifest walked{3, samples, k_poly_default_limits, 1};
    const std::vector<pw_data_point> data = walked.rule_set_.converter_.convert(samples);
    std::mt19937 rng{2026};
    size_t checked = 0;
    for (int walk = 0; walk < 10; ++walk) {
        const expr* e = walked.rule_set_.initial_;
        for (int step = 0; step < 6; ++step) {
            std::vector<rewrite> rewrites;
            walked.rule_set_.rules_.detect(e, rewrites);
            if (rewrites.empty())
                break;
            for (const rewrite& detected : rewrites) {
                const expr* proposal = detected();
                for (const pw_data_point& point : data) {
                    const std::optional<const expr*> value =
                        walked.rule_set_.pw_evaluator_.evaluate_at(proposal, point.input);
                    EXPECT_TRUE(value.has_value() && value.value() == point.output);
                }
                ++checked;
            }
            e = rewrites[rng() % rewrites.size()]();
        }
    }
    EXPECT_GT(checked, 30u);
}

TEST_F(LogicFitIntegrationTest, TheRuntimeReducesMajorityToItsPrimitive) {
    logic_fit_runtime runtime{3, table(majority), k_poly_default_limits, 20000};
    while (runtime.rewrites() < 500 && runtime.next()) {}
    EXPECT_EQ(runtime.best(), "lams(3, maj(#2, #1, #0))");
    EXPECT_EQ(runtime.best_cost(), 6u);
}
