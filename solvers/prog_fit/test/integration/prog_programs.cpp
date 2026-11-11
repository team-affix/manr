#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "composition/prog_fit_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"

using ::testing::Contains;
using ::testing::IsEmpty;

namespace {

// Programs over data of any shape: the value parser, the interpreter, and the enumerator on real interning.
struct ProgProgramsIntegrationTest : ::testing::Test {
    std::vector<prog_sample> samples{};

    std::string print(prog_fit_manifest& m, const expr* e) {
        std::ostringstream os;
        m.kernel_.printer_.print(os, e);
        return os.str();
    }
    // the value of a program (text over prog symbols, parsed as data then read as a program) at an input
    std::string run(const std::string& program, const std::string& input) {
        prog_fit_manifest m{{}, k_poly_default_limits, 1};
        const std::optional<const expr*> value =
            m.rule_set_.interpreter_.evaluate_at(m.rule_set_.parser_.parse(program), m.rule_set_.parser_.parse(input));
        return value.has_value() ? print(m, value.value()) : "none";
    }
    std::vector<std::string> proposed() {
        prog_fit_manifest m{samples, k_poly_default_limits, 1};
        std::vector<const expr*> out;
        m.rule_set_.programs_.propose_hypotheses(m.rule_set_.points_, out);
        std::vector<std::string> texts;
        for (const expr* e : out)
            texts.push_back(print(m, e));
        return texts;
    }
};

}

TEST_F(ProgProgramsIntegrationTest, TheParserReadsListsTreesRecordsTuplesAndHoles) {
    prog_fit_manifest m{{}, k_poly_default_limits, 1};
    EXPECT_EQ(print(m, m.rule_set_.parser_.parse("[1, [2, -3], []]")),
              "cons(1, cons(cons(2, cons(-3, nil)), cons(nil, nil)))");
    EXPECT_EQ(print(m, m.rule_set_.parser_.parse("node(leaf, 4, leaf)")), "node(leaf, 4, leaf)");
    EXPECT_EQ(print(m, m.rule_set_.parser_.parse("point([1], edge(0, ?2))")), "point(cons(1, nil), edge(0, ?2))");
}

TEST_F(ProgProgramsIntegrationTest, TheInterpreterRunsHigherOrderProgramsOverAnyShape) {
    EXPECT_EQ(run("lam(fold(lam(lam(add(#0, #1))), 0, #0))", "[1, 2, 3]"), "6");
    EXPECT_EQ(run("lam(scan(lam(lam(add(#0, #1))), 0, #0))", "[1, 2, 3]"), "cons(1, cons(3, cons(6, nil)))");
    EXPECT_EQ(run("lams(2, zipwith(lam(lam(mul(#0, #1))), #1, #0))", "point([1, 2], [3, 4])"),
              "cons(3, cons(8, nil))");
    EXPECT_EQ(run("lam(tfold(lams(3, add(add(#2, #1), #0)), 0, #0))", "node(node(leaf, 1, leaf), 2, leaf)"), "3");
    EXPECT_EQ(run("lam(length(filter(lam(eq(field(0, #0), 0)), #0)))", "[edge(0, 1), edge(1, 0), edge(0, 2)]"), "2");
    EXPECT_EQ(run("lam(reachable(#0, 0))", "[edge(0, 1), edge(1, 2), edge(3, 4)]"), "cons(0, cons(1, cons(2, nil)))");
}

TEST_F(ProgProgramsIntegrationTest, TheInterpreterRunsHelpers) {
    EXPECT_EQ(run("lam(let(lam(mul(#0, #0)), add(app(#0, #1), 1)))", "5"), "26");
}

TEST_F(ProgProgramsIntegrationTest, ArithmeticOnAHoleStaysSymbolic) {
    EXPECT_EQ(run("lam(add(#0, 1))", "?1"), "?1 + 1");
}

TEST_F(ProgProgramsIntegrationTest, TheEnumeratorFindsTheSmallestProgram) {
    samples = {{"[3, 1, 2]", "[1, 2, 3]"}, {"[]", "[]"}, {"[5, 4]", "[4, 5]"}, {"[2, 2, 1]", "[1, 2, 2]"}};
    EXPECT_THAT(proposed(), Contains("sort(#0)"));
}

TEST_F(ProgProgramsIntegrationTest, TheEnumeratorReadsAFoldOverATree) {
    samples = {{"leaf", "0"}, {"node(leaf, 3, leaf)", "3"}, {"node(node(leaf, 1, leaf), 2, leaf)", "3"},
               {"node(leaf, 5, node(leaf, -1, leaf))", "4"}, {"node(node(leaf, 7, leaf), -2, node(leaf, 4, leaf))", "9"},
               {"node(leaf, 0, leaf)", "0"}, {"node(node(node(leaf, 2, leaf), 2, leaf), 2, leaf)", "6"},
               {"node(leaf, -3, node(leaf, 8, leaf))", "5"}};
    // the sum of the folded left, the value and the folded right (in some order): it sums trees it has not seen
    prog_fit_manifest m{samples, k_poly_default_limits, 1};
    std::vector<const expr*> out;
    m.rule_set_.programs_.propose_hypotheses(m.rule_set_.points_, out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_THAT(print(m, out[0]), ::testing::StartsWith("tfold(lams(3, "));
    const std::optional<const expr*> value = m.rule_set_.interpreter_.evaluate_with(
        out[0], {m.rule_set_.parser_.parse("node(node(leaf, 10, leaf), 20, node(leaf, 30, node(leaf, 40, leaf)))")});
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(print(m, value.value()), "100");
}

TEST_F(ProgProgramsIntegrationTest, TheEnumeratorReadsTwoInputs) {
    samples = {{"point([1], [2, 3])", "[1, 2, 3]"}, {"point([], [4])", "[4]"}, {"point([5, 6], [])", "[5, 6]"}};
    EXPECT_THAT(proposed(), Contains("append(#1, #0)"));
}

TEST_F(ProgProgramsIntegrationTest, TheEnumeratorProposesNothingForTooFewSamples) {
    samples = {{"[1]", "1"}, {"[2]", "2"}};
    EXPECT_THAT(proposed(), IsEmpty());
}
