#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/subterm_proposer.hpp"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace {

struct MockDetectLocal {
    MOCK_METHOD(void, detect, (const expr*, (std::vector<rewrite>&)));
};

struct MockMakeFunctor {
    MOCK_METHOD(const expr*, make_functor, (uint32_t, (const std::vector<const expr*>&)));
};

struct SubtermProposerTest : ::testing::Test {
    NiceMock<MockDetectLocal> detect_local;
    StrictMock<MockMakeFunctor> make_functor;
    subterm_proposer<MockDetectLocal, MockMakeFunctor> proposer{detect_local, make_functor};
    expr_pool pool{};
    const expr* a = pool.make_functor(1, {});
    const expr* b = pool.make_functor(2, {});
    const expr* c = pool.make_functor(3, {});
    const expr* root = pool.make_functor(0, {a, b});
    std::vector<rewrite> out{};

    void SetUp() override {
        EXPECT_CALL(detect_local, detect(_, _)).Times(AnyNumber());
    }

    void detect_at(const expr* site, rewrite detected) {
        EXPECT_CALL(detect_local, detect(site, _)).WillOnce([detected](const expr*, std::vector<rewrite>& o) {
            o.push_back(detected);
        });
    }
};

}

TEST_F(SubtermProposerTest, ARewriteOfTheWholeExpressionComesThroughAsIs) {
    detect_at(root, ready(c));
    proposer.detect(root, out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0](), c);
}

TEST_F(SubtermProposerTest, ARewriteOfAnArgumentIsRebuiltIntoItsParentOnlyWhenPerformed) {
    const expr* rebuilt = pool.make_functor(0, {a, c});
    detect_at(b, ready(c));
    proposer.detect(root, out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_CALL(make_functor, make_functor(0, std::vector<const expr*>{a, c})).WillOnce(Return(rebuilt));
    EXPECT_EQ(out[0](), rebuilt);
}
