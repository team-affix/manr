#include <algorithm>
#include <map>
#include <optional>
#include <variant>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "hypotheses/pw_guarded_hypotheses.hpp"
#include "infrastructure/expr_pool.hpp"
#include "value_objects/integer.hpp"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
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
    // the first condition data read is the default's: 1 where the output is not the default
    EXPECT_CALL(conditions, propose_hypotheses(_, _))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke([&](const std::vector<pw_data_point>& data, std::vector<const expr*>& found) {
            if (seen_conditions.empty())
                seen_conditions = data;
            found.push_back(condition);
        }));
    EXPECT_CALL(values, propose_hypotheses(_, _)).Times(AnyNumber());
    EXPECT_CALL(values, propose_hypotheses(ElementsAre(Field(&pw_data_point::output, five)), _))
        .WillRepeatedly(Invoke([&](const std::vector<pw_data_point>&, std::vector<const expr*>& found) {
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
    // the other readings (each output as a value) are larger, and only the smallest are proposed
    ON_CALL(find_cost, cost(_)).WillByDefault(Return(10));
    ON_CALL(find_cost, cost(guarded_by(model))).WillByDefault(Return(1));
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

TEST_F(PwGuardedHypothesesTest, ACommonOutputIsAlsoAValueUnderAConditionWithTheOtherPointsReadAgain) {
    // 0 at keys 1 and 3, 5 at key 2, 7 at key 4: where the condition holds, 0; elsewhere the reading of the others
    const expr* seven = pool.make_functor(107, {});
    std::vector<std::vector<pw_data_point>> seen_conditions;
    ON_CALL(conditions, propose_hypotheses(_, _))
        .WillByDefault(Invoke([&](const std::vector<pw_data_point>& data, std::vector<const expr*>& found) {
            seen_conditions.push_back(data);
            found.push_back(condition);
        }));
    ON_CALL(values, propose_hypotheses(ElementsAre(Field(&pw_data_point::output, five),
                                                   Field(&pw_data_point::output, seven)), _))
        .WillByDefault(Invoke([&](const std::vector<pw_data_point>&, std::vector<const expr*>& found) {
            found.push_back(model);
        }));
    guarded.propose_hypotheses({point(1, zero), point(2, five), point(3, zero), point(4, seven)}, out);
    EXPECT_THAT(seen_conditions, Contains(ElementsAre(Field(&pw_data_point::output, one),
                                                      Field(&pw_data_point::output, zero),
                                                      Field(&pw_data_point::output, one),
                                                      Field(&pw_data_point::output, zero))));
    const expr* peeled = pool.make_functor(symbols.piecewise, {pool.make_functor(symbols.region, {condition, zero}),
                                                               model});
    EXPECT_THAT(out, Contains(peeled));
}

TEST_F(PwGuardedHypothesesTest, ANestedReadingOfTheOtherPointsIsSplicedIntoOnePiecewiseOfSeveralBranches) {
    // 0 at keys 1, 3, 6; 5 at keys 2, 4; 7 at key 5: the others read as 5 guarding 7, spliced after the branch of 0
    ON_CALL(conditions, propose_hypotheses(_, _))
        .WillByDefault(Invoke([&](const std::vector<pw_data_point>&, std::vector<const expr*>& found) {
            found.push_back(condition);
        }));
    const expr* seven = pool.make_functor(107, {});
    ON_CALL(values, propose_hypotheses(ElementsAre(Field(&pw_data_point::output, seven)), _))
        .WillByDefault(Invoke([&](const std::vector<pw_data_point>&, std::vector<const expr*>& found) {
            found.push_back(model);
        }));
    const expr* spliced = pool.make_functor(symbols.piecewise, {pool.make_functor(symbols.region, {condition, zero}),
                                                                pool.make_functor(symbols.region, {condition, model}),
                                                                five});
    // only the smallest readings are proposed: make it the smallest
    ON_CALL(find_cost, cost(_)).WillByDefault(Return(10));
    ON_CALL(find_cost, cost(spliced)).WillByDefault(Return(1));
    guarded.propose_hypotheses(
        {point(1, zero), point(2, five), point(3, zero), point(4, five), point(5, seven), point(6, zero)}, out);
    EXPECT_THAT(out, Contains(spliced));
}

TEST_F(PwGuardedHypothesesTest, AFewRepeatedOutputsAmongManyAreNotPeeled) {
    // 0 at keys 1, 2; 5 at keys 3, 4; six other outputs: with outputs common from two points, the common outputs
    // cover less than half
    pw_guarded_hypotheses<MockProposeConditions, MockProposeValues, MockEvaluateAt, MockFindCost, MockMakeLiteral,
                          expr_pool>
        sparse{conditions, values, evaluate_at, find_cost, make_literal, pool, symbols, 2, 8, 2, 4};
    std::vector<pw_data_point> points{point(1, zero), point(2, zero), point(3, five), point(4, five)};
    for (uint32_t key = 5; key <= 10; ++key)
        points.push_back(point(key, pool.make_functor(110 + key, {})));
    ON_CALL(conditions, propose_hypotheses(_, _))
        .WillByDefault(Invoke([&](const std::vector<pw_data_point>& data, std::vector<const expr*>&) {
            // a peel of 0 or 5 would read condition data over all ten points with exactly two 1s
            const size_t ones = static_cast<size_t>(
                std::count_if(data.begin(), data.end(), [&](const pw_data_point& p) { return p.output == one; }));
            EXPECT_FALSE(data.size() == 10 && ones == 2);
        }));
    sparse.propose_hypotheses(points, out);
}
