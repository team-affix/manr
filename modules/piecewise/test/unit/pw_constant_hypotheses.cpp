#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/pw_constant_hypotheses.hpp"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

struct MockGetCost {
    MOCK_METHOD(size_t, cost, (const expr*));
};

struct PwConstantHypothesesTest : ::testing::Test {
    PwConstantHypothesesTest() {
        ON_CALL(get_cost, cost(_)).WillByDefault(Return(1));
    }
    NiceMock<MockGetCost> get_cost;
    pw_constant_hypotheses<MockGetCost> hypotheses{get_cost};
    expr_pool pool{};
    const expr* a = pool.make_functor(1, {});
    const expr* b = pool.make_functor(2, {});
    const expr* c = pool.make_functor(3, {});
    std::vector<const expr*> out{};
};

}

TEST_F(PwConstantHypothesesTest, TheOutputThatDropsTheMostIsTheOnlyOneProposed) {
    hypotheses.propose_hypotheses({{.input = a, .output = b}, {.input = b, .output = c}, {.input = c, .output = c}}, out);
    EXPECT_EQ(out, std::vector<const expr*>{c});
}

TEST_F(PwConstantHypothesesTest, ALargeOutputSavesLessThanItsBranchesSize) {
    // b's branch is 1 + 1 + 50 nodes but b stays as the catch-all; two branches of c save 2 * 3 - 1
    ON_CALL(get_cost, cost(b)).WillByDefault(Return(50));
    hypotheses.propose_hypotheses({{.input = a, .output = b}, {.input = b, .output = c}, {.input = c, .output = c}}, out);
    EXPECT_EQ(out, std::vector<const expr*>{c});
}

TEST_F(PwConstantHypothesesTest, OnATieTheFirstSeenIsProposed) {
    hypotheses.propose_hypotheses({{.input = a, .output = b}, {.input = b, .output = a}}, out);
    EXPECT_EQ(out, std::vector<const expr*>{b});
}

TEST_F(PwConstantHypothesesTest, NoDataProposesNothing) {
    hypotheses.propose_hypotheses({}, out);
    EXPECT_TRUE(out.empty());
}
