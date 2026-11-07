#include <stdexcept>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/best_first_search.hpp"
#include "infrastructure/expr_pool.hpp"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

struct MockDetect {
    MOCK_METHOD(void, detect, (const expr*, (std::vector<rewrite>&)));
};

struct MockGetCost {
    MOCK_METHOD(size_t, cost, (const expr*));
};

struct BestFirstSearchTest : ::testing::Test {
    expr_pool pool{};
    const expr* start = pool.make_functor(0, {});
    const expr* dear = pool.make_functor(1, {});
    const expr* cheap = pool.make_functor(2, {});
    const expr* after_dear = pool.make_functor(3, {});
    const expr* after_cheap = pool.make_functor(4, {});
    NiceMock<MockDetect> detect;
    NiceMock<MockGetCost> get_cost;

    void SetUp() override {
        ON_CALL(get_cost, cost(start)).WillByDefault(Return(10));
        ON_CALL(get_cost, cost(dear)).WillByDefault(Return(20));
        ON_CALL(get_cost, cost(cheap)).WillByDefault(Return(5));
        ON_CALL(get_cost, cost(after_dear)).WillByDefault(Return(1));
        ON_CALL(get_cost, cost(after_cheap)).WillByDefault(Return(2));
    }

    void detect_from(const expr* from, std::vector<rewrite> rewrites) {
        ON_CALL(detect, detect(from, _)).WillByDefault([rewrites](const expr*, std::vector<rewrite>& o) {
            o.insert(o.end(), rewrites.begin(), rewrites.end());
        });
    }
};

}

TEST_F(BestFirstSearchTest, TheInitialExpressionIsTheBestUntilSomethingCheaperIsReached) {
    best_first_search<MockDetect, MockGetCost> search{detect, get_cost, start, 8};
    EXPECT_EQ(search.best(), start);
    EXPECT_EQ(search.best_cost(), 10u);
    EXPECT_TRUE(search.next());
    EXPECT_FALSE(search.next());
    EXPECT_EQ(search.best(), start);
}

TEST_F(BestFirstSearchTest, EveryRewriteOfTheExpandedExpressionIsCostedInOneStep) {
    detect_from(start, {ready(dear), ready(cheap)});
    best_first_search<MockDetect, MockGetCost> search{detect, get_cost, start, 8};
    ASSERT_TRUE(search.next());
    EXPECT_EQ(search.best(), cheap);
    EXPECT_EQ(search.rewrites(), 2u);
}

TEST_F(BestFirstSearchTest, TheCheapestReachedExpressionIsExpandedFirst) {
    detect_from(start, {ready(dear), ready(cheap)});
    detect_from(dear, {ready(after_dear)});
    detect_from(cheap, {ready(after_cheap)});
    best_first_search<MockDetect, MockGetCost> search{detect, get_cost, start, 8};
    EXPECT_CALL(detect, detect(start, _));
    EXPECT_CALL(detect, detect(cheap, _));
    EXPECT_CALL(detect, detect(dear, _)).Times(0);
    for (int step = 0; step < 2; ++step)
        ASSERT_TRUE(search.next());
    EXPECT_EQ(search.best(), after_cheap);
    EXPECT_EQ(search.derivation(after_cheap), (std::vector<const expr*>{start, cheap, after_cheap}));
}

TEST_F(BestFirstSearchTest, AnExpressionReachedTwiceHasItsRewritesDetectedOnce) {
    detect_from(start, {ready(cheap), ready(cheap), ready(start)});
    best_first_search<MockDetect, MockGetCost> search{detect, get_cost, start, 8};
    EXPECT_CALL(detect, detect(_, _)).Times(AnyNumber());
    EXPECT_CALL(detect, detect(cheap, _)).Times(1);
    while (search.next()) {}
    EXPECT_EQ(search.rewrites(), 3u);
}

TEST_F(BestFirstSearchTest, ARewriteThatFailsBreaksTheContract) {
    detect_from(start, {ready(nullptr)});
    best_first_search<MockDetect, MockGetCost> search{detect, get_cost, start, 8};
    EXPECT_THROW(search.next(), std::logic_error);
}

TEST_F(BestFirstSearchTest, OverCapacityTheMostExpensiveExpressionIsEvicted) {
    detect_from(start, {ready(dear), ready(cheap)});
    best_first_search<MockDetect, MockGetCost> search{detect, get_cost, start, 1};
    EXPECT_CALL(detect, detect(_, _)).Times(AnyNumber());
    EXPECT_CALL(detect, detect(dear, _)).Times(0);
    while (search.next()) {}
}
