#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "rules/list_filter_fuser.hpp"

using ::testing::_;
using ::testing::NiceMock;

namespace {

struct MockMakeFunctor {
    MOCK_METHOD(const expr*, make_functor, (uint32_t, (const std::vector<const expr*>&)));
};

struct ListFilterFuserTest : ::testing::Test {
    expr_pool pool{};
    list_symbols symbols{.nil = 1, .cons = 2, .append = 3, .reverse = 4, .map = 5, .filter = 6, .fold = 7,
                         .lambda = 8, .input = 9, .element = 9, .head = 9, .accumulator = 10};
    NiceMock<MockMakeFunctor> make_functor;
    list_filter_fuser<MockMakeFunctor> fuser{make_functor, symbols, 40};
    const expr* xs = pool.make_functor(30, {});
    const expr* p = pool.make_functor(31, {});
    const expr* q = pool.make_functor(32, {});
    std::vector<rewrite> out{};

    void SetUp() override {
        ON_CALL(make_functor, make_functor(_, _)).WillByDefault([this](uint32_t id, const std::vector<const expr*>& a) {
            return pool.make_functor(id, a);
        });
    }

    const expr* filter(const expr* body, const expr* list) {
        return pool.make_functor(6, {pool.make_functor(8, {body}), list});
    }
};

}

TEST_F(ListFilterFuserTest, TwoFiltersBecomeOneOnTheConjunctionOfTheirConditions) {
    fuser.detect(filter(p, filter(q, xs)), out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0](), filter(pool.make_functor(40, {q, p}), xs));
}

TEST_F(ListFilterFuserTest, AFilterTwiceOnOneConditionIsOneFilter) {
    fuser.detect(filter(p, filter(p, xs)), out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0](), filter(p, xs));
}

TEST_F(ListFilterFuserTest, AFilterOverAnythingButAFilterIsLeftAlone) {
    fuser.detect(filter(p, xs), out);
    EXPECT_TRUE(out.empty());
}
