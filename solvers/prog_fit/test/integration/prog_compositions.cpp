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

// Programs of several steps found by decomposing the data (`prog_decomposing_hypotheses`), on real interning.
struct ProgCompositionsIntegrationTest : ::testing::Test {
    std::vector<prog_sample> samples{};

    std::string print(prog_fit_manifest& m, const expr* e) {
        std::ostringstream os;
        m.kernel_.printer_.print(os, e);
        return os.str();
    }
    std::vector<std::string> composed() {
        prog_fit_manifest m{samples, k_poly_default_limits, 1};
        std::vector<const expr*> out;
        m.rule_set_.compositions_.propose_hypotheses(m.rule_set_.points_, out);
        std::vector<std::string> texts;
        for (const expr* e : out)
            texts.push_back(print(m, e));
        return texts;
    }
    bool matches(uint8_t relation, const std::string& value, const std::string& wanted) {
        prog_fit_manifest m{{}, k_poly_default_limits, 1};
        return m.rule_set_.matcher_.matches(relation, m.rule_set_.parser_.parse(value),
                                            m.rule_set_.parser_.parse(wanted));
    }
};

}

TEST_F(ProgCompositionsIntegrationTest, RelationsComparePermutationsSetsTruthAndSubsequences) {
    EXPECT_TRUE(matches(k_prog_relation_permutation, "[3, 1, 3]", "[1, 3, 3]"));
    EXPECT_FALSE(matches(k_prog_relation_permutation, "[3, 1]", "[1, 3, 3]"));
    EXPECT_TRUE(matches(k_prog_relation_set, "[3, 1, 3]", "[1, 3]"));
    EXPECT_TRUE(matches(k_prog_relation_truth, "7", "1"));
    EXPECT_FALSE(matches(k_prog_relation_truth, "0", "1"));
    EXPECT_TRUE(matches(k_prog_relation_supersequence, "[4, 1, 5, 2]", "[1, 2]"));
    EXPECT_FALSE(matches(k_prog_relation_supersequence, "[4, 2, 5, 1]", "[1, 2]"));
}

TEST_F(ProgCompositionsIntegrationTest, ASortOverAMapIsReadAsASortThenAMap) {
    samples = {{"[3, -1, 2]", "[1, 4, 9]"}, {"[]", "[]"}, {"[-4, 0]", "[0, 16]"}, {"[2, -3, 1, 0]", "[0, 1, 4, 9]"},
               {"[5]", "[25]"}, {"[-2, -5, 3]", "[4, 9, 25]"}, {"[1, -1]", "[1, 1]"}, {"[6, -2, 4]", "[4, 16, 36]"}};
    EXPECT_THAT(composed(), Contains("sort(map(lam(#0 * #0), #0))"));
}

TEST_F(ProgCompositionsIntegrationTest, AMapOverListsOfListsHasAListBody) {
    samples = {{"[[1, 2], [3]]", "[[2, 1], [3]]"}, {"[]", "[]"}, {"[[4, 5, 6]]", "[[6, 5, 4]]"},
               {"[[], [7, 8]]", "[[], [8, 7]]"}, {"[[1], [2, 3], [4, 5]]", "[[1], [3, 2], [5, 4]]"},
               {"[[9, 0, 1]]", "[[1, 0, 9]]"}};
    EXPECT_THAT(composed(), Contains("map(lam(reverse(#0)), #0)"));
}

TEST_F(ProgCompositionsIntegrationTest, AFilterReadsItsPredicateFromTheElementsKept) {
    samples = {{"[[1, 2], [], [3]]", "[[1, 2], [3]]"}, {"[]", "[]"}, {"[[], []]", "[]"}, {"[[4]]", "[[4]]"},
               {"[[5, 6], [], [], [7]]", "[[5, 6], [7]]"}, {"[[], [8, 9, 1]]", "[[8, 9, 1]]"}};
    EXPECT_THAT(composed(), Contains("filter(lam(length(#0)), #0)"));
}

TEST_F(ProgCompositionsIntegrationTest, ACountFillsItsPredicateThroughTheFrame) {
    samples = {{"[1, 2, 3, 4]", "2"}, {"[]", "0"}, {"[5, 7]", "0"}, {"[2, 2, 6]", "3"}, {"[-4, 3, 0]", "2"},
               {"[9, 8, 1, 10, 12]", "3"}, {"[11]", "0"}, {"[6]", "1"}};
    const std::vector<std::string> found = composed();
    ASSERT_EQ(found.size(), 1u);
    EXPECT_THAT(found[0], ::testing::StartsWith("length(filter(lam("));
}

TEST_F(ProgCompositionsIntegrationTest, AFunctionOfTwoInputsCombinedIsOneHelper) {
    samples = {{"point([1, 2], [3])", "-4"}, {"point([0], [1, 1])", "-2"}, {"point([2, 2, 1], [])", "9"},
               {"point([3], [2, 2])", "1"}, {"point([-1, 4], [1])", "16"}, {"point([], [5])", "-25"},
               {"point([1, 1, 1], [1])", "2"}, {"point([2, -3], [0, 1])", "12"}};
    const std::vector<std::string> found = composed();
    ASSERT_EQ(found.size(), 1u);
    EXPECT_THAT(found[0], ::testing::StartsWith("let(lam(fold("));
}

TEST_F(ProgCompositionsIntegrationTest, AShapePreservingTreeProgramIsATreeMap) {
    samples = {{"leaf", "leaf"}, {"node(leaf, 1, leaf)", "node(leaf, 2, leaf)"},
               {"node(node(leaf, 4, leaf), 0, leaf)", "node(node(leaf, 5, leaf), 1, leaf)"},
               {"node(leaf, -3, node(leaf, 7, leaf))", "node(leaf, -2, node(leaf, 8, leaf))"},
               {"node(node(leaf, 2, leaf), 9, node(leaf, 6, leaf))",
                "node(node(leaf, 3, leaf), 10, node(leaf, 7, leaf))"}};
    EXPECT_THAT(composed(), Contains("tfold(lams(3, node(#2, #1 + 1, #0)), leaf, #0)"));
}

TEST_F(ProgCompositionsIntegrationTest, TooFewSamplesAreNotDecomposed) {
    samples = {{"[1]", "[1]"}, {"[2, 1]", "[1, 2]"}};
    EXPECT_THAT(composed(), IsEmpty());
}
