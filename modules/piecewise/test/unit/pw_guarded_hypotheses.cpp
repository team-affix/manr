#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/pw_guarded_hypotheses.hpp"
#include "value_objects/integer.hpp"

using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::SaveArg;

namespace {

struct MockProposeConditions {
    MOCK_METHOD(void, propose_hypotheses, (const std::vector<pw_data_point>&, std::vector<const expr*>&));
};
struct MockProposeValues {
    MOCK_METHOD(void, propose_hypotheses, (const std::vector<pw_data_point>&, std::vector<const expr*>&));
};
struct MockMakeLiteral {
    MOCK_METHOD(const expr*, make_literal, (const integer&));
};

struct PwGuardedHypothesesTest : ::testing::Test {
    PwGuardedHypothesesTest() {
        ON_CALL(make_literal, make_literal(_)).WillByDefault(Invoke([this](const integer& value) {
            return pool.make_functor(static_cast<uint32_t>(100 + value.convert_to<int>()), {});
        }));
    }
    expr_pool pool{};
    ::testing::NiceMock<MockProposeConditions> conditions;
    ::testing::NiceMock<MockProposeValues> values;
    ::testing::NiceMock<MockMakeLiteral> make_literal;
    pw_symbols symbols{.piecewise = 7, .branch = 8, .region = 9};
    pw_guarded_hypotheses<MockProposeConditions, MockProposeValues, MockMakeLiteral, expr_pool> guarded{
        conditions, values, make_literal, pool, symbols, 1};
    const expr* zero = pool.make_functor(100, {});
    const expr* one = pool.make_functor(101, {});
    const expr* five = pool.make_functor(105, {});
    const expr* condition = pool.make_functor(50, {});
    const expr* value = pool.make_functor(51, {});
    pw_data_point point(uint32_t key, const expr* output) {
        return pw_data_point{.input = pool.make_functor(key, {}), .output = output};
    }
    std::vector<const expr*> out;
};

}

TEST_F(PwGuardedHypothesesTest, TheMostCommonOutputIsTheDefaultAndTheRestAreGuarded) {
    std::vector<pw_data_point> seen_conditions;
    std::vector<pw_data_point> seen_values;
    EXPECT_CALL(conditions, propose_hypotheses(_, _))
        .WillOnce(DoAll(SaveArg<0>(&seen_conditions),
                        Invoke([&](const std::vector<pw_data_point>&, std::vector<const expr*>& found) {
                            found.push_back(condition);
                        })));
    EXPECT_CALL(values, propose_hypotheses(_, _))
        .WillOnce(DoAll(SaveArg<0>(&seen_values), Invoke([&](const std::vector<pw_data_point>&,
                                                             std::vector<const expr*>& found) {
                            found.push_back(value);
                        })));
    guarded.propose_hypotheses({point(1, zero), point(2, five), point(3, zero)}, out);
    EXPECT_THAT(seen_conditions, ElementsAre(Field(&pw_data_point::output, zero), Field(&pw_data_point::output, one),
                                             Field(&pw_data_point::output, zero)));
    EXPECT_THAT(seen_values, ElementsAre(Field(&pw_data_point::output, five)));
    const expr* region = pool.make_functor(symbols.region, {condition, value});
    EXPECT_EQ(out, std::vector<const expr*>{pool.make_functor(symbols.piecewise, {region, zero})});
}

TEST_F(PwGuardedHypothesesTest, OneOutputEverywhereHasNoGuard) {
    EXPECT_CALL(conditions, propose_hypotheses(_, _)).Times(0);
    guarded.propose_hypotheses({point(1, zero), point(2, zero)}, out);
    EXPECT_TRUE(out.empty());
}

TEST_F(PwGuardedHypothesesTest, NoConditionMeansNoProposal) {
    EXPECT_CALL(values, propose_hypotheses(_, _)).Times(0);
    guarded.propose_hypotheses({point(1, zero), point(2, five), point(3, zero)}, out);
    EXPECT_TRUE(out.empty());
}
