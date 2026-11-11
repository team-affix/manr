#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/hole_scope_proposer.hpp"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace {

struct MockDetectRoot {
    MOCK_METHOD(void, detect, (const expr*, (std::vector<rewrite>&)));
};

struct MockMakeFunctor {
    MOCK_METHOD(const expr*, make_functor, (uint32_t, (const std::vector<const expr*>&)));
};

struct HoleScopeProposerTest : ::testing::Test {
    NiceMock<MockDetectRoot> detect_root;
    StrictMock<MockMakeFunctor> make_functor;
    hole_scope_proposer<MockDetectRoot, MockMakeFunctor> proposer{detect_root, make_functor};
    expr_pool pool{};
    const expr* a = pool.make_functor(1, {});
    const expr* hole0 = pool.make_var(0);
    const expr* hole1 = pool.make_var(1);
    const expr* owner = pool.make_functor(2, {a, hole1});
    std::vector<rewrite> out{};

    void SetUp() override {
        EXPECT_CALL(detect_root, detect(_, _)).Times(AnyNumber());
    }
};

}

TEST_F(HoleScopeProposerTest, ASubtermHoldingEveryOccurrenceOfItsHoleIsDetectedAndRebuiltWhenPerformed) {
    const expr* root = pool.make_functor(0, {a, owner});
    const expr* rebuilt = pool.make_functor(0, {a, a});
    EXPECT_CALL(detect_root, detect(owner, _)).WillOnce([this](const expr*, std::vector<rewrite>& o) {
        o.push_back(ready(a));
    });
    proposer.detect(root, out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_CALL(make_functor, make_functor(0, std::vector<const expr*>{a, a})).WillOnce(Return(rebuilt));
    EXPECT_EQ(out[0](), rebuilt);
}

TEST_F(HoleScopeProposerTest, ASubtermSharingItsHoleWithTheRestIsNotDetected) {
    const expr* root = pool.make_functor(0, {hole1, owner});
    EXPECT_CALL(detect_root, detect(owner, _)).Times(0);
    proposer.detect(root, out);
}

TEST_F(HoleScopeProposerTest, TheWholeExpressionIsDetectedWhenItHoldsAHole) {
    const expr* root = pool.make_functor(0, {hole1, owner});
    EXPECT_CALL(detect_root, detect(root, _)).Times(1);
    proposer.detect(root, out);
}

TEST_F(HoleScopeProposerTest, SubtermsWithoutHolesAndBareHolesAreSkipped) {
    const expr* plain = pool.make_functor(3, {a});
    const expr* root = pool.make_functor(0, {plain, hole0});
    StrictMock<MockDetectRoot> strict_root;
    hole_scope_proposer<MockDetectRoot, MockMakeFunctor> strict{strict_root, make_functor};
    EXPECT_CALL(strict_root, detect(root, _)).Times(1);
    strict.detect(root, out);
}
