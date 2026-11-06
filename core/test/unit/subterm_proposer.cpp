#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/subterm_proposer.hpp"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

struct MockProposeLocal {
    MOCK_METHOD(void, propose, (const expr*, (std::vector<const expr*>&)));
};

struct MockMakeFunctor {
    MOCK_METHOD(const expr*, make_functor, (uint32_t, (const std::vector<const expr*>&)));
};

struct SubtermProposerTest : ::testing::Test {
    NiceMock<MockProposeLocal> propose_local;
    MockMakeFunctor make_functor;
    subterm_proposer<MockProposeLocal, MockMakeFunctor> proposer{propose_local, make_functor};
    expr_pool pool{};
    const expr* a = pool.make_functor(1, {});
    const expr* b = pool.make_functor(2, {});
    const expr* c = pool.make_functor(3, {});
    const expr* root = pool.make_functor(0, {a, b});
    std::vector<const expr*> out{};

    void SetUp() override {
        EXPECT_CALL(propose_local, propose(_, _)).Times(AnyNumber());
    }
};

}

TEST_F(SubtermProposerTest, ARewriteOfTheWholeExpressionIsProposedAsIs) {
    EXPECT_CALL(propose_local, propose(root, _)).WillOnce((
        [this](const expr*, std::vector<const expr*>& o) { o.push_back(c); }));
    proposer.propose(root, out);
    EXPECT_EQ(out, std::vector<const expr*>{c});
}

TEST_F(SubtermProposerTest, ARewriteOfAnArgumentIsRebuiltIntoItsParent) {
    const expr* rebuilt = pool.make_functor(0, {a, c});
    EXPECT_CALL(propose_local, propose(b, _)).WillOnce((
        [this](const expr*, std::vector<const expr*>& o) { o.push_back(c); }));
    EXPECT_CALL(make_functor, make_functor(0, std::vector<const expr*>{a, c})).WillOnce(Return(rebuilt));
    proposer.propose(root, out);
    EXPECT_EQ(out, std::vector<const expr*>{rebuilt});
}
