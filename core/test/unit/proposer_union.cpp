#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/proposer_union.hpp"

using ::testing::_;
using ::testing::InSequence;

namespace {

struct MockProposer {
    MOCK_METHOD(void, propose, (const expr*, (std::vector<const expr*>&)));
};

struct ProposerUnionTest : ::testing::Test {
    MockProposer first;
    MockProposer second;
    proposer_union<MockProposer, MockProposer> proposers{first, second};
    expr_pool pool{};
    std::vector<const expr*> out{};
};

}

TEST_F(ProposerUnionTest, EveryProposerIsAskedInOrderWithTheSameOutput) {
    const expr* e = pool.make_var(0);
    InSequence order;
    EXPECT_CALL(first, propose(e, _));
    EXPECT_CALL(second, propose(e, _));
    proposers.propose(e, out);
}
