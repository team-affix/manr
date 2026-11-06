#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/best_first_search.hpp"
#include "infrastructure/expr_pool.hpp"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

struct MockPropose {
    MOCK_METHOD(void, propose, (const expr*, (std::vector<const expr*>&)));
};

struct MockGetCost {
    MOCK_METHOD(size_t, cost, (const expr*));
};

struct BestFirstSearchTest : ::testing::Test {
    expr_pool pool{};
    const expr* start = pool.make_functor(0, {});
    const expr* cheap = pool.make_functor(1, {});
    const expr* dear = pool.make_functor(2, {});
    const expr* cheapest = pool.make_functor(3, {});
    NiceMock<MockPropose> propose;
    NiceMock<MockGetCost> get_cost;

    void SetUp() override {
        ON_CALL(get_cost, cost(start)).WillByDefault(Return(10));
        ON_CALL(get_cost, cost(cheap)).WillByDefault(Return(5));
        ON_CALL(get_cost, cost(dear)).WillByDefault(Return(20));
        ON_CALL(get_cost, cost(cheapest)).WillByDefault(Return(1));
    }

    void propose_from(const expr* from, std::vector<const expr*> to) {
        ON_CALL(propose, propose(from, _)).WillByDefault((
            [to](const expr*, std::vector<const expr*>& o) { o.insert(o.end(), to.begin(), to.end()); }));
    }
};

}

TEST_F(BestFirstSearchTest, TheInitialExpressionIsTheBestUntilSomethingCheaperIsReached) {
    best_first_search<MockPropose, MockGetCost> search{propose, get_cost, start, 8};
    EXPECT_EQ(search.best(), start);
    EXPECT_EQ(search.best_cost(), 10u);
}

TEST_F(BestFirstSearchTest, TheCheapestFrontierEntryIsExpandedFirst) {
    propose_from(start, {dear, cheap});
    best_first_search<MockPropose, MockGetCost> search{propose, get_cost, start, 8};
    ASSERT_TRUE(search.next());
    EXPECT_CALL(propose, propose(cheap, _));
    ASSERT_TRUE(search.next());
    EXPECT_EQ(search.best(), cheap);
}

TEST_F(BestFirstSearchTest, AnExpressionReachedTwiceIsExpandedOnce) {
    propose_from(start, {cheap, cheap, start});
    best_first_search<MockPropose, MockGetCost> search{propose, get_cost, start, 8};
    ASSERT_TRUE(search.next());
    ASSERT_TRUE(search.next());
    EXPECT_FALSE(search.next());
    EXPECT_EQ(search.expansions(), 2u);
}

TEST_F(BestFirstSearchTest, OverCapacityTheMostExpensiveEntryIsEvicted) {
    propose_from(start, {cheap, dear});
    best_first_search<MockPropose, MockGetCost> search{propose, get_cost, start, 1};
    ASSERT_TRUE(search.next());
    EXPECT_CALL(propose, propose(cheap, _));
    EXPECT_CALL(propose, propose(dear, _)).Times(0);
    ASSERT_TRUE(search.next());
    EXPECT_FALSE(search.next());
}

TEST_F(BestFirstSearchTest, TheDerivationRunsFromTheInitialExpressionToTheOneReached) {
    propose_from(start, {cheap});
    propose_from(cheap, {cheapest});
    best_first_search<MockPropose, MockGetCost> search{propose, get_cost, start, 8};
    while (search.next()) {}
    EXPECT_EQ(search.best(), cheapest);
    EXPECT_EQ(search.derivation(cheapest), (std::vector<const expr*>{start, cheap, cheapest}));
}
