#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/mcts_search.hpp"

using ::testing::_;
using ::testing::DoubleEq;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgReferee;

namespace {

struct MockPropose {
    MOCK_METHOD(void, propose, (const expr*, (std::vector<const expr*>&)));
};

struct MockGetCost {
    MOCK_METHOD(size_t, cost, (const expr*));
};

struct MockChoose {
    MOCK_METHOD(const expr*, choose, ((const std::vector<const expr*>&), (const std::vector<const expr*>&)));
};

struct MockSetReward {
    MOCK_METHOD(void, set_value, (double));
};

struct MockTerminate {
    MOCK_METHOD(void, terminate, ());
};

struct MctsSearchTest : ::testing::Test {
    expr_pool pool{};
    const expr* start = pool.make_functor(0, {});
    const expr* small = pool.make_functor(1, {});
    const expr* large = pool.make_functor(2, {});
    NiceMock<MockPropose> propose;
    NiceMock<MockGetCost> get_cost;
    NiceMock<MockChoose> choose;
    NiceMock<MockSetReward> set_reward;
    NiceMock<MockTerminate> terminate;

    void SetUp() override {
        ON_CALL(get_cost, cost(start)).WillByDefault(Return(10));
        ON_CALL(get_cost, cost(small)).WillByDefault(Return(4));
        ON_CALL(get_cost, cost(large)).WillByDefault(Return(8));
    }

    mcts_search<MockPropose, MockGetCost, MockChoose, MockSetReward, MockTerminate> make_search(
        size_t probing_depth, size_t max_simulations) {
        return {propose, get_cost, choose, set_reward, terminate, start, probing_depth, max_simulations};
    }
};

}

TEST_F(MctsSearchTest, ASimulationIsRewardedByTheSmallestCostItReachedNotTheOneItEndedOn) {
    ON_CALL(propose, propose(start, _)).WillByDefault(SetArgReferee<1>(std::vector<const expr*>{small}));
    ON_CALL(propose, propose(small, _)).WillByDefault(SetArgReferee<1>(std::vector<const expr*>{large}));
    EXPECT_CALL(choose, choose(_, _)).WillOnce(Return(small)).WillOnce(Return(large));
    auto search = make_search(100, 1);
    InSequence order;
    EXPECT_CALL(set_reward, set_value(DoubleEq(0.6)));
    EXPECT_CALL(terminate, terminate());
    ASSERT_TRUE(search.next());
    EXPECT_EQ(search.best(), small);
    EXPECT_EQ(search.best_cost(), 4u);
    EXPECT_EQ(search.derivation(), (std::vector<const expr*>{start, small}));
}

TEST_F(MctsSearchTest, ASimulationStopsAtTheProbingDepth) {
    ON_CALL(propose, propose(_, _)).WillByDefault(SetArgReferee<1>(std::vector<const expr*>{large}));
    ON_CALL(choose, choose(_, _)).WillByDefault(Return(large));
    auto search = make_search(3, 1);
    EXPECT_CALL(choose, choose(_, _)).Times(3);
    ASSERT_TRUE(search.next());
}

TEST_F(MctsSearchTest, NothingBetterThanTheInitialExpressionKeepsItBestWithNoReward) {
    auto search = make_search(3, 1);
    EXPECT_CALL(set_reward, set_value(DoubleEq(0.0)));
    ASSERT_TRUE(search.next());
    EXPECT_EQ(search.best(), start);
    EXPECT_EQ(search.derivation(), std::vector<const expr*>{start});
}

TEST_F(MctsSearchTest, TheSearchEndsOnceItsSimulationsAreSpent) {
    auto search = make_search(3, 2);
    EXPECT_TRUE(search.next());
    EXPECT_TRUE(search.next());
    EXPECT_FALSE(search.next());
    EXPECT_EQ(search.simulations(), 2u);
}
