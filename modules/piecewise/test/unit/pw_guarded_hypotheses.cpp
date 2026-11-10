#include <map>
#include <optional>
#include <variant>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/pw_guarded_hypotheses.hpp"
#include "value_objects/integer.hpp"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Return;

namespace {

struct MockProposeConditions {
    MOCK_METHOD(void, propose_hypotheses, (const std::vector<pw_data_point>&, std::vector<const expr*>&));
};
struct MockProposeValues {
    MOCK_METHOD(void, propose_hypotheses, (const std::vector<pw_data_point>&, std::vector<const expr*>&));
};
struct MockEvaluateAt {
    MOCK_METHOD(std::optional<const expr*>, evaluate_at, (const expr*, const expr*));
};
struct MockFindCost {
    MOCK_METHOD(size_t, cost, (const expr*));
};
struct MockMakeLiteral {
    MOCK_METHOD(const expr*, make_literal, (const integer&));
};

struct PwGuardedHypothesesTest : ::testing::Test {
    PwGuardedHypothesesTest() {
        ON_CALL(make_literal, make_literal(_)).WillByDefault(Invoke([this](const integer& value) {
            return pool.make_functor(static_cast<uint32_t>(100 + value.convert_to<int>()), {});
        }));
        // a guarded reading (a piecewise) is taken to reproduce the data: each point's own output
        ON_CALL(evaluate_at, evaluate_at(_, _)).WillByDefault(Invoke([this](const expr* e, const expr* input) {
            const bool reading = std::get<expr::functor>(e->content).id == symbols.piecewise;
            const auto known = outputs.find(input);
            return reading && known != outputs.end() ? std::optional<const expr*>(known->second) : std::nullopt;
        }));
    }
    expr_pool pool{};
    ::testing::NiceMock<MockProposeConditions> conditions;
    ::testing::NiceMock<MockProposeValues> values;
    ::testing::NiceMock<MockEvaluateAt> evaluate_at;
    ::testing::NiceMock<MockFindCost> find_cost;
    ::testing::NiceMock<MockMakeLiteral> make_literal;
    pw_symbols symbols{.piecewise = 7, .branch = 8, .region = 9};
    pw_guarded_hypotheses<MockProposeConditions, MockProposeValues, MockEvaluateAt, MockFindCost, MockMakeLiteral,
                          expr_pool>
        guarded{conditions, values, evaluate_at, find_cost, make_literal, pool, symbols, 1, 8, 1, 4};
    const expr* zero = pool.make_functor(100, {});
    const expr* one = pool.make_functor(101, {});
    const expr* five = pool.make_functor(105, {});
    const expr* condition = pool.make_functor(50, {});
    const expr* value = pool.make_functor(51, {});
    const expr* model = pool.make_functor(52, {});
    pw_data_point point(uint32_t key, const expr* output) {
        const expr* input = pool.make_functor(key, {});
        outputs[input] = output;
        return pw_data_point{.input = input, .output = output};
    }
    const expr* guarded_by(const expr* fallback) {
        return pool.make_functor(symbols.piecewise, {pool.make_functor(symbols.region, {condition, value}), fallback});
    }
    std::map<const expr*, const expr*> outputs;
    std::vector<const expr*> out;
};

}

TEST_F(PwGuardedHypothesesTest, TheMostCommonOutputIsTheDefaultAndTheRestAreGuarded) {
    std::vector<pw_data_point> seen_conditions;
    EXPECT_CALL(conditions, propose_hypotheses(_, _))
        .WillOnce(Invoke([&](const std::vector<pw_data_point>& data, std::vector<const expr*>& found) {
            seen_conditions = data;
            found.push_back(condition);
        }));
    EXPECT_CALL(values, propose_hypotheses(_, _)).Times(AnyNumber());
    EXPECT_CALL(values, propose_hypotheses(ElementsAre(Field(&pw_data_point::output, five)), _))
        .WillOnce(Invoke([&](const std::vector<pw_data_point>&, std::vector<const expr*>& found) {
            found.push_back(value);
        }));
    guarded.propose_hypotheses({point(1, zero), point(2, five), point(3, zero)}, out);
    EXPECT_THAT(seen_conditions, ElementsAre(Field(&pw_data_point::output, zero), Field(&pw_data_point::output, one),
                                             Field(&pw_data_point::output, zero)));
    EXPECT_THAT(out, Contains(guarded_by(zero)));
}

TEST_F(PwGuardedHypothesesTest, AModelThatReproducesMostPointsIsADefault) {
    // outputs 1, 2, 5 at keys 1, 2, 3: the model reproduces the first two
    const expr* two = pool.make_functor(102, {});
    ON_CALL(values, propose_hypotheses(_, _))
        .WillByDefault(Invoke([&](const std::vector<pw_data_point>& data, std::vector<const expr*>& found) {
            found.push_back(data.size() == 3 ? model : value);
        }));
    ON_CALL(evaluate_at, evaluate_at(model, _)).WillByDefault(Invoke([&](const expr*, const expr* input) {
        return std::optional<const expr*>(input == pool.make_functor(1, {}) ? one : two);
    }));
    ON_CALL(conditions, propose_hypotheses(_, _))
        .WillByDefault(Invoke([&](const std::vector<pw_data_point>&, std::vector<const expr*>& found) {
            found.push_back(condition);
        }));
    guarded.propose_hypotheses({point(1, one), point(2, two), point(3, five)}, out);
    EXPECT_THAT(out, Contains(guarded_by(model)));
}

TEST_F(PwGuardedHypothesesTest, OneOutputEverywhereHasNoGuard) {
    EXPECT_CALL(conditions, propose_hypotheses(_, _)).Times(0);
    guarded.propose_hypotheses({point(1, zero), point(2, zero)}, out);
    EXPECT_TRUE(out.empty());
}

TEST_F(PwGuardedHypothesesTest, NoConditionMeansNoProposal) {
    guarded.propose_hypotheses({point(1, zero), point(2, five), point(3, zero)}, out);
    EXPECT_TRUE(out.empty());
}
