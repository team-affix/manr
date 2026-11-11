#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "rules/list_map_fuser.hpp"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

struct MockSubstituteIndex {
    MOCK_METHOD(const expr*, substitute, (const expr*, size_t, const expr*));
};

struct MockFindCost {
    MOCK_METHOD(size_t, cost, (const expr*));
};

struct MockMakeFunctor {
    MOCK_METHOD(const expr*, make_functor, (uint32_t, (const std::vector<const expr*>&)));
};

struct ListMapFuserTest : ::testing::Test {
    expr_pool pool{};
    list_symbols symbols{.nil = 1, .cons = 2, .append = 3, .reverse = 4, .map = 5, .filter = 6, .fold = 7,
                         .lambda = 8, .input = 9, .element = 9, .head = 9, .accumulator = 10};
    NiceMock<MockSubstituteIndex> substitute_index;
    NiceMock<MockFindCost> find_cost;
    NiceMock<MockMakeFunctor> make_functor;
    list_map_fuser<MockSubstituteIndex, MockFindCost, MockMakeFunctor> fuser{substitute_index, find_cost, make_functor,
                                                                            symbols};
    const expr* xs = pool.make_functor(9, {});
    const expr* f = pool.make_functor(20, {});
    const expr* g = pool.make_functor(21, {});
    const expr* fg = pool.make_functor(22, {});
    const expr* inner = pool.make_functor(5, {pool.make_functor(8, {g}), xs});
    const expr* e = pool.make_functor(5, {pool.make_functor(8, {f}), inner});
    const expr* fused = pool.make_functor(5, {pool.make_functor(8, {fg}), xs});
    std::vector<rewrite> out{};

    void SetUp() override {
        ON_CALL(make_functor, make_functor(_, _)).WillByDefault([this](uint32_t id, const std::vector<const expr*>& a) {
            return pool.make_functor(id, a);
        });
        ON_CALL(substitute_index, substitute(f, 0, g)).WillByDefault(Return(fg));
    }
};

}

TEST_F(ListMapFuserTest, AMapOverAMapBecomesOneMapOfTheSubstitutedBodyWhenSmaller) {
    ON_CALL(find_cost, cost(fused)).WillByDefault(Return(4));
    ON_CALL(find_cost, cost(e)).WillByDefault(Return(7));
    fuser.detect(e, out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0](), fused);
}

TEST_F(ListMapFuserTest, AFusedMapNoSmallerIsNotProposed) {
    ON_CALL(find_cost, cost(fused)).WillByDefault(Return(7));
    ON_CALL(find_cost, cost(e)).WillByDefault(Return(7));
    fuser.detect(e, out);
    EXPECT_TRUE(out.empty());
}

TEST_F(ListMapFuserTest, AMapOverAnythingButAMapIsLeftAlone) {
    fuser.detect(inner, out);
    EXPECT_TRUE(out.empty());
}

TEST_F(ListMapFuserTest, AFunctionThatIsNotALambdaIsLeftAlone) {
    const expr* opaque = pool.make_functor(5, {f, inner});
    fuser.detect(opaque, out);
    EXPECT_TRUE(out.empty());
}
