#include <stdexcept>
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

namespace {

struct MockDetect {
    MOCK_METHOD(void, detect, (const expr*, (std::vector<rewrite>&)));
};

struct MockGetCost {
    MOCK_METHOD(size_t, cost, (const expr*));
};

struct MockChoose {
    MOCK_METHOD(size_t, choose, ((const std::vector<size_t>&), (const std::vector<size_t>&)));
};

struct MockSetReward {
    MOCK_METHOD(void, set_value, (double));
};

struct MockTerminate {
    MOCK_METHOD(void, terminate, ());
};

struct MockRewrite {
    MOCK_METHOD(const expr*, apply, ());
};

struct MctsSearchTest : ::testing::Test {
    expr_pool pool{};
    const expr* start = pool.make_functor(0, {});
    const expr* small = pool.make_functor(1, {});
    const expr* large = pool.make_functor(2, {});
    NiceMock<MockDetect> detect;
    NiceMock<MockGetCost> get_cost;
    NiceMock<MockChoose> choose;
    NiceMock<MockSetReward> set_reward;
    NiceMock<MockTerminate> terminate;

    void SetUp() override {
        ON_CALL(get_cost, cost(start)).WillByDefault(Return(10));
        ON_CALL(get_cost, cost(small)).WillByDefault(Return(4));
        ON_CALL(get_cost, cost(large)).WillByDefault(Return(8));
    }

    void detect_from(const expr* from, std::vector<rewrite> rewrites) {
        ON_CALL(detect, detect(from, _)).WillByDefault([rewrites](const expr*, std::vector<rewrite>& o) {
            o.insert(o.end(), rewrites.begin(), rewrites.end());
        });
    }

    mcts_search<MockDetect, MockGetCost, MockChoose, MockSetReward, MockTerminate> make_search(
        size_t probing_depth, size_t max_simulations) {
        return {detect, get_cost, choose, set_reward, terminate, start, probing_depth, max_simulations};
    }
};

}

TEST_F(MctsSearchTest, ASimulationIsRewardedByTheSmallestCostItReachedNotTheOneItEndedOn) {
    detect_from(start, {ready(small)});
    detect_from(small, {ready(large)});
    ON_CALL(choose, choose(_, _)).WillByDefault(Return(0));
    auto search = make_search(100, 1);
    InSequence order;
    EXPECT_CALL(set_reward, set_value(DoubleEq(0.6)));
    EXPECT_CALL(terminate, terminate());
    ASSERT_TRUE(search.next());
    EXPECT_EQ(search.best(), small);
    EXPECT_EQ(search.derivation(), (std::vector<const expr*>{start, small}));
}

TEST_F(MctsSearchTest, OnlyTheChosenRewriteIsPerformed) {
    MockRewrite skipped;
    MockRewrite chosen;
    detect_from(start, {[&skipped]() { return skipped.apply(); }, [&chosen]() { return chosen.apply(); }});
    EXPECT_CALL(choose, choose(std::vector<size_t>{0, 1}, std::vector<size_t>{0, 1})).WillOnce(Return(1));
    EXPECT_CALL(chosen, apply()).WillOnce(Return(small));
    EXPECT_CALL(skipped, apply()).Times(0);
    auto search = make_search(1, 1);
    ASSERT_TRUE(search.next());
    EXPECT_EQ(search.best(), small);
}

TEST_F(MctsSearchTest, ARewriteThatFailsBreaksTheContract) {
    detect_from(start, {ready(nullptr)});
    ON_CALL(choose, choose(_, _)).WillByDefault(Return(0));
    auto search = make_search(3, 1);
    EXPECT_THROW(search.next(), std::logic_error);
}

TEST_F(MctsSearchTest, ASimulationStopsAtAnExpressionWithNoRewrites) {
    auto search = make_search(3, 1);
    EXPECT_CALL(choose, choose(_, _)).Times(0);
    ASSERT_TRUE(search.next());
}

TEST_F(MctsSearchTest, TheSearchEndsOnceItsSimulationsAreSpent) {
    auto search = make_search(3, 2);
    EXPECT_TRUE(search.next());
    EXPECT_TRUE(search.next());
    EXPECT_FALSE(search.next());
    EXPECT_EQ(search.simulations(), 2u);
}
