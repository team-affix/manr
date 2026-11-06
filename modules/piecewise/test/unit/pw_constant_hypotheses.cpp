#include <vector>
#include <gtest/gtest.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/pw_constant_hypotheses.hpp"

namespace {

struct PwConstantHypothesesTest : ::testing::Test {
    pw_constant_hypotheses hypotheses{};
    expr_pool pool{};
    const expr* a = pool.make_functor(1, {});
    const expr* b = pool.make_functor(2, {});
    std::vector<const expr*> out{};
};

}

TEST_F(PwConstantHypothesesTest, EachDistinctOutputIsProposedOnceInFirstSeenOrder) {
    hypotheses.propose_hypotheses({{.input = a, .output = b}, {.input = b, .output = a}, {.input = a, .output = b}}, out);
    EXPECT_EQ(out, (std::vector<const expr*>{b, a}));
}
