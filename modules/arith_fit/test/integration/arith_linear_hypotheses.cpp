#include <vector>
#include <gtest/gtest.h>
#include "infrastructure/arith_linear_hypotheses.hpp"
#include "infrastructure/arith_literals.hpp"
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/symbol_table.hpp"

namespace {

struct ArithLinearHypothesesIntegrationTest : ::testing::Test {
    symbol_table symbols{};
    expr_pool pool{};
    arith_symbols vocabulary{
        .input = symbols.intern_symbol("#0"),
        .add = symbols.intern_symbol("add"),
        .sub = symbols.intern_symbol("sub"),
        .mul = symbols.intern_symbol("mul")};
    arith_literals<symbol_table, expr_pool> literals{symbols, pool};
    arith_linear_hypotheses<decltype(literals), decltype(literals), expr_pool> lines{
        literals, literals, pool, vocabulary};
    const expr* x = pool.make_functor(vocabulary.input, {});
    std::vector<const expr*> out{};

    pw_data_point point(int64_t input, int64_t output) {
        return pw_data_point{.input = literals.make_literal(input), .output = literals.make_literal(output)};
    }
};

}

TEST_F(ArithLinearHypothesesIntegrationTest, ThePairsOfOneLineGiveThatLineOnce) {
    lines.propose_hypotheses({point(0, 1), point(1, 3), point(2, 5)}, out);
    const expr* line = pool.make_functor(
        vocabulary.add, {pool.make_functor(vocabulary.mul, {literals.make_literal(2), x}), literals.make_literal(1)});
    EXPECT_EQ(out, std::vector<const expr*>{line});
}

TEST_F(ArithLinearHypothesesIntegrationTest, UnitSlopeAndZeroInterceptAreLeftOut) {
    lines.propose_hypotheses({point(1, 1), point(4, 4)}, out);
    EXPECT_EQ(out, std::vector<const expr*>{x});
}

TEST_F(ArithLinearHypothesesIntegrationTest, AFractionalSlopeOrARepeatedInputGivesNoLine) {
    lines.propose_hypotheses({point(0, 0), point(2, 1), point(2, 5)}, out);
    EXPECT_TRUE(out.empty());
}
