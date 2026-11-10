#include <cstdint>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "composition/logic_fit_manifest.hpp"
#include "composition/logic_fit_runtime.hpp"
#include "infrastructure/best_first_search.hpp"
#include "value_objects/poly_default_limits.hpp"

using ::testing::Contains;

namespace {

// Formulas over three inputs `#2`, `#1`, `#0` without data: the structural laws, the truth-table refit with holes, and
// a search over both.
struct LogicFormulasIntegrationTest : ::testing::Test {
    logic_fit_manifest manifest{3, {}, k_poly_default_limits, 1};
    logic_fit_rules_manifest& rules = manifest.rule_set_;
    poly_kernel_manifest& kernel = manifest.kernel_;
    const logic_symbols& s = rules.logic_symbols_;
    const expr* a = kernel.parser_.parse("#2");
    const expr* b = kernel.parser_.parse("#1");
    const expr* c = kernel.parser_.parse("#0");
    std::vector<rewrite> out{};

    const expr* op(uint32_t id, std::vector<const expr*> args) { return kernel.exprs_.make_functor(id, args); }
    const expr* no(const expr* e) { return op(s.negation, {e}); }
    std::string print(const expr* e) {
        std::ostringstream os;
        kernel.printer_.print(os, e);
        return os.str();
    }
    std::vector<std::string> simplified(const expr* e) {
        out.clear();
        rules.simplifier_.detect(e, out);
        std::vector<std::string> texts;
        for (const rewrite& r : out)
            texts.push_back(print(r()));
        return texts;
    }
    std::vector<std::string> refitted(const expr* e) {
        out.clear();
        rules.refitter_.detect(e, out);
        std::vector<std::string> texts;
        for (const rewrite& r : out)
            texts.push_back(print(r()));
        return texts;
    }
    std::string reduced(const expr* e) {
        best_first_search<logic_fit_rules_manifest::rules_t, expr_size> search(rules.rules_, kernel.sizes_, e, 20000);
        while (search.rewrites() < 500 && search.next()) {}
        return print(search.best());
    }
    // the truth table of a formula without holes, row r giving #2, #1, #0 as bits 0, 1, 2
    std::vector<bool> table(const expr* e) {
        std::vector<bool> rows;
        for (int row = 0; row < 8; ++row) {
            const expr* input = op(rules.point_symbol_, {kernel.literals_.make_literal(row & 1),
                                                         kernel.literals_.make_literal((row >> 1) & 1),
                                                         kernel.literals_.make_literal((row >> 2) & 1)});
            rows.push_back(rules.evaluator_.holds_at(e, input).value());
        }
        return rows;
    }
    // a random formula over the three inputs and the constants, of every connective
    const expr* random_formula(std::mt19937& rng, int depth) {
        const std::vector<const expr*> leaves{a, b, c, kernel.literals_.make_literal(0), kernel.literals_.make_literal(1)};
        if (depth == 0 || rng() % 4 == 0)
            return leaves[rng() % leaves.size()];
        const std::vector<uint32_t> binary{s.conjunction, s.disjunction, s.exclusion, s.equivalence,
                                           s.alternative_denial, s.joint_denial, s.implication};
        const uint32_t pick = rng() % 10;
        if (pick == 9)
            return op(s.multiplexer, {random_formula(rng, depth - 1), random_formula(rng, depth - 1),
                                      random_formula(rng, depth - 1)});
        if (pick == 7)
            return no(random_formula(rng, depth - 1));
        if (pick == 8)
            return op(s.majority, {random_formula(rng, depth - 1), random_formula(rng, depth - 1),
                                   random_formula(rng, depth - 1)});
        return op(binary[pick], {random_formula(rng, depth - 1), random_formula(rng, depth - 1)});
    }
};

}

TEST_F(LogicFormulasIntegrationTest, DoubleNegationCancels) {
    EXPECT_THAT(simplified(no(no(op(s.conjunction, {a, b})))), Contains("and(#2, #1)"));
}

TEST_F(LogicFormulasIntegrationTest, DeMorganGivesNor) {
    EXPECT_THAT(simplified(op(s.conjunction, {no(a), no(b)})), Contains("nor(#2, #1)"));
}

TEST_F(LogicFormulasIntegrationTest, AbsorptionKeepsTheSharedOperand) {
    EXPECT_THAT(simplified(op(s.conjunction, {a, op(s.disjunction, {b, a})})), Contains("#2"));
}

TEST_F(LogicFormulasIntegrationTest, AnXorChainCancelsEqualOperandsAtAnyDepth) {
    const expr* inner = op(s.conjunction, {a, b});
    const expr* chain = op(s.exclusion, {op(s.exclusion, {inner, c}), inner});
    EXPECT_THAT(simplified(chain), Contains("#0"));
}

TEST_F(LogicFormulasIntegrationTest, TheExpansionOfXorIsRecognizedOverSubformulas) {
    const expr* p = op(s.disjunction, {a, c});
    const expr* q = op(s.conjunction, {b, c});
    const expr* expanded = op(s.disjunction, {op(s.conjunction, {p, no(q)}), op(s.conjunction, {no(p), q})});
    EXPECT_THAT(simplified(expanded), Contains("xor(or(#2, #0), and(#1, #0))"));
}

TEST_F(LogicFormulasIntegrationTest, TheFactoredMajorityIsRecognized) {
    const expr* factored = op(s.disjunction, {op(s.conjunction, {a, op(s.disjunction, {b, c})}), op(s.conjunction, {b, c})});
    EXPECT_THAT(simplified(factored), Contains("maj(#2, #1, #0)"));
}

TEST_F(LogicFormulasIntegrationTest, EverySimplificationKeepsTheTruthTableAndShrinks) {
    std::mt19937 rng{7};
    size_t checked = 0;
    for (int trial = 0; trial < 3000; ++trial) {
        const expr* formula = random_formula(rng, 4);
        out.clear();
        rules.simplifier_.detect(formula, out);
        for (const rewrite& r : out) {
            const expr* simpler = r();
            EXPECT_EQ(table(simpler), table(formula)) << print(formula) << " -> " << print(simpler);
            EXPECT_LT(kernel.sizes_.cost(simpler), kernel.sizes_.cost(formula)) << print(formula);
            ++checked;
        }
    }
    EXPECT_GT(checked, 100u);
}

TEST_F(LogicFormulasIntegrationTest, TheRefitRecoversXorFromItsSumOfProducts) {
    const expr* expanded = op(s.disjunction, {op(s.conjunction, {a, no(b)}), op(s.conjunction, {no(a), b})});
    EXPECT_THAT(refitted(expanded), Contains("xor(#2, #1)"));
}

TEST_F(LogicFormulasIntegrationTest, TheRefitBindsAHoleToWhatMakesTheFormulaSmallest) {
    // or(x, ?0) can be anything at least x, so ?0 = 1 gives the smallest formula
    const expr* half = op(s.conjunction, {no(b), c});
    EXPECT_THAT(refitted(op(s.disjunction, {half, kernel.exprs_.make_var(0)})), Contains("1"));
    // xor(x, ?0) can be anything at all
    EXPECT_THAT(refitted(op(s.exclusion, {op(s.conjunction, {a, b}), kernel.exprs_.make_var(0)})), Contains("0"));
}

TEST_F(LogicFormulasIntegrationTest, TheRefitKeepsTheRowsAHoleCannotChange) {
    // and(or(#2, #1), ?0) is 0 wherever or(#2, #1) is 0, and free elsewhere: the smallest such formula is 0
    EXPECT_THAT(refitted(op(s.conjunction, {op(s.disjunction, {a, b}), kernel.exprs_.make_var(0)})), Contains("0"));
    // maj(#2, #1, ?0) is #2 where #2 = #1, free elsewhere
    EXPECT_THAT(refitted(op(s.majority, {a, b, kernel.exprs_.make_var(0)})), Contains("#2"));
}

TEST_F(LogicFormulasIntegrationTest, SearchReducesANestOfLawsToAPrimitive) {
    // not(and(not(or(#2, #1)), not(and(not(#2), #1)))) is or(or(#2, #1), and(not(#2), #1)) = or(#2, #1)
    const expr* nested = no(op(s.conjunction, {no(op(s.disjunction, {a, b})), no(op(s.conjunction, {no(a), b}))}));
    EXPECT_EQ(reduced(nested), "or(#2, #1)");
}

TEST_F(LogicFormulasIntegrationTest, DataOfAPrimitiveWithUnseenRowsIsReadAsThePrimitive) {
    // maj(#2, #1, #0) with rows (0, 0, 0) and (1, 1, 1) unseen
    std::vector<poly_sample> samples;
    for (int row = 1; row < 7; ++row) {
        const int x = row & 1, y = (row >> 1) & 1, z = (row >> 2) & 1;
        samples.push_back(poly_sample{.inputs = {x, y, z}, .output = x + y + z >= 2 ? 1 : 0});
    }
    logic_fit_runtime runtime{3, samples, k_poly_default_limits, 20000};
    while (runtime.rewrites() < 500 && runtime.next()) {}
    EXPECT_LE(runtime.best_cost(), 6u) << runtime.best();
}

TEST_F(LogicFormulasIntegrationTest, DataOfAPrimitiveComposedWithAnInputIsReadAsTheComposition) {
    // xor(maj(#3, #2, #1), #0) over four inputs
    std::vector<poly_sample> samples;
    for (int row = 0; row < 16; ++row) {
        const int w = row & 1, x = (row >> 1) & 1, y = (row >> 2) & 1, z = (row >> 3) & 1;
        samples.push_back(poly_sample{.inputs = {w, x, y, z}, .output = (w + x + y >= 2 ? 1 : 0) ^ z});
    }
    logic_fit_runtime runtime{4, samples, k_poly_default_limits, 20000};
    while (runtime.rewrites() < 500 && runtime.next()) {}
    EXPECT_EQ(runtime.best(), "lams(4, xor(maj(#3, #2, #1), #0))");
}

TEST_F(LogicFormulasIntegrationTest, DataOfCompositionsSharingAnInputIsReadThroughTheSharedInput) {
    // xor(maj(#3, #2, #1), and(#3, #0)): both parts read #3
    std::vector<poly_sample> samples;
    for (int row = 0; row < 16; ++row) {
        const int w = row & 1, x = (row >> 1) & 1, y = (row >> 2) & 1, z = (row >> 3) & 1;
        samples.push_back(poly_sample{.inputs = {w, x, y, z}, .output = (w + x + y >= 2 ? 1 : 0) ^ (w & z)});
    }
    logic_fit_runtime runtime{4, samples, k_poly_default_limits, 20000};
    while (runtime.rewrites() < 500 && runtime.next()) {}
    EXPECT_EQ(runtime.best(), "lams(4, xor(maj(#3, #2, #1), and(#3, #0)))");
}

TEST_F(LogicFormulasIntegrationTest, DataOfATwoLevelCompositionIsReadAtEveryLevel) {
    // or(xor(#5, #4), and(#3, maj(#2, #1, #0)))
    std::vector<poly_sample> samples;
    for (int row = 0; row < 64; ++row) {
        std::vector<int> bits;
        for (int position = 0; position < 6; ++position)
            bits.push_back((row >> position) & 1);
        const int output = (bits[0] != bits[1]) || (bits[2] && bits[3] + bits[4] + bits[5] >= 2);
        samples.push_back(poly_sample{.inputs = {bits[0], bits[1], bits[2], bits[3], bits[4], bits[5]}, .output = output});
    }
    logic_fit_runtime runtime{6, samples, k_poly_default_limits, 20000};
    while (runtime.rewrites() < 500 && runtime.next()) {}
    EXPECT_EQ(runtime.best(), "lams(6, or(xor(#5, #4), and(#3, maj(#2, #1, #0))))");
}

TEST_F(LogicFormulasIntegrationTest, ANegatedMajorityOfComplementsIsTheMajorityOfTheOperands) {
    const expr* negated = no(op(s.majority, {op(s.joint_denial, {a, b}), op(s.alternative_denial, {b, c}), no(c)}));
    EXPECT_THAT(simplified(negated), Contains("maj(or(#2, #1), and(#1, #0), #0)"));
}

TEST_F(LogicFormulasIntegrationTest, ANegatedMultiplexerComplementsItsBranchesOnly) {
    EXPECT_THAT(simplified(no(op(s.multiplexer, {a, op(s.equivalence, {b, c}), no(c)}))),
                Contains("mux(#2, xor(#1, #0), #0)"));
}

TEST_F(LogicFormulasIntegrationTest, DataOfAMultiplexerWhoseSelectorIsAFormulaIsReadThroughTheSelector) {
    // reduce request #27: mux(xor(#3, #2), #1, #0)
    std::vector<poly_sample> samples;
    for (int row = 0; row < 16; ++row) {
        const int w = row & 1, x = (row >> 1) & 1, y = (row >> 2) & 1, z = (row >> 3) & 1;
        samples.push_back(poly_sample{.inputs = {w, x, y, z}, .output = (w ^ x) ? y : z});
    }
    logic_fit_runtime runtime{4, samples, k_poly_default_limits, 20000};
    while (runtime.rewrites() < 500 && runtime.next()) {}
    EXPECT_LE(runtime.best_cost(), 8u) << runtime.best();
}

TEST_F(LogicFormulasIntegrationTest, DataOfAMultiplexerOfThreeFormulasIsReadAtEveryPart) {
    // reduce request #29: mux(and(#4, #3), xor(#2, #1), or(#2, #0)), the branches sharing #2
    std::vector<poly_sample> samples;
    for (int row = 0; row < 32; ++row) {
        std::vector<int> bits;
        for (int position = 0; position < 5; ++position)
            bits.push_back((row >> position) & 1);
        const int output = (bits[0] & bits[1]) ? (bits[2] ^ bits[3]) : (bits[2] | bits[4]);
        samples.push_back(poly_sample{.inputs = {bits[0], bits[1], bits[2], bits[3], bits[4]}, .output = output});
    }
    logic_fit_runtime runtime{5, samples, k_poly_default_limits, 20000};
    while (runtime.rewrites() < 500 && runtime.next()) {}
    EXPECT_LE(runtime.best_cost(), 12u) << runtime.best();
}

TEST_F(LogicFormulasIntegrationTest, DataOfAMajorityOfThreeFormulasIsReadWithoutANegation) {
    // reduce request #28: maj(and(#5, #4), or(#3, #2), xor(#1, #0))
    std::vector<poly_sample> samples;
    for (int row = 0; row < 64; ++row) {
        std::vector<int> bits;
        for (int position = 0; position < 6; ++position)
            bits.push_back((row >> position) & 1);
        const int output = (bits[0] & bits[1]) + (bits[2] | bits[3]) + (bits[4] ^ bits[5]) >= 2;
        samples.push_back(poly_sample{.inputs = {bits[0], bits[1], bits[2], bits[3], bits[4], bits[5]},
                                      .output = output});
    }
    logic_fit_runtime runtime{6, samples, k_poly_default_limits, 20000};
    while (runtime.rewrites() < 500 && runtime.next()) {}
    EXPECT_LE(runtime.best_cost(), 12u) << runtime.best();
}
