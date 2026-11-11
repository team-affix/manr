#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "rules/list_reverse_canceller.hpp"

using ::testing::_;
using ::testing::NiceMock;

namespace {

struct MockMakeFunctor {
    MOCK_METHOD(const expr*, make_functor, (uint32_t, (const std::vector<const expr*>&)));
};

struct ListReverseCancellerTest : ::testing::Test {
    expr_pool pool{};
    list_symbols symbols{.nil = 1, .cons = 2, .append = 3, .reverse = 4, .map = 5, .filter = 6, .fold = 7,
                         .lambda = 8, .input = 9, .element = 9, .head = 9, .accumulator = 10};
    NiceMock<MockMakeFunctor> make_functor;
    list_reverse_canceller<MockMakeFunctor> canceller{make_functor, symbols};
    const expr* xs = pool.make_functor(30, {});
    const expr* f = pool.make_functor(31, {});
    const expr* p = pool.make_functor(32, {});
    std::vector<rewrite> out{};

    void SetUp() override {
        ON_CALL(make_functor, make_functor(_, _)).WillByDefault([this](uint32_t id, const std::vector<const expr*>& a) {
            return pool.make_functor(id, a);
        });
    }
};

}

TEST_F(ListReverseCancellerTest, AReverseOfAReverseIsItsList) {
    canceller.detect(pool.make_functor(4, {pool.make_functor(4, {xs})}), out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0](), xs);
}

TEST_F(ListReverseCancellerTest, AReverseCancelsThroughMapsAndFilters) {
    const expr* reversed = pool.make_functor(4, {xs});
    const expr* steps = pool.make_functor(5, {f, pool.make_functor(6, {p, reversed})});
    canceller.detect(pool.make_functor(4, {steps}), out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0](), pool.make_functor(5, {f, pool.make_functor(6, {p, xs})}));
}

TEST_F(ListReverseCancellerTest, AReverseWithoutAnotherBelowIsLeftAlone) {
    canceller.detect(pool.make_functor(4, {pool.make_functor(5, {f, xs})}), out);
    EXPECT_TRUE(out.empty());
}

TEST_F(ListReverseCancellerTest, AStepThatIsNotElementWiseStopsTheSearch) {
    const expr* folded = pool.make_functor(7, {f, p, pool.make_functor(4, {xs})});
    canceller.detect(pool.make_functor(4, {folded}), out);
    EXPECT_TRUE(out.empty());
}
