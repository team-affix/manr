#include <optional>
#include <vector>
#include <gtest/gtest.h>
#include "infrastructure/arith_evaluator.hpp"
#include "infrastructure/arith_literals.hpp"
#include "infrastructure/arith_simplifier.hpp"
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/symbol_table.hpp"

// Literals, evaluation and simplification over real interning: their correctness is pointer identity of
// interned literals, which mocks would have to reimplement.
namespace {

struct ArithAlgebraIntegrationTest : ::testing::Test {
    symbol_table symbols{};
    expr_pool pool{};
    arith_symbols vocabulary{
        .input = symbols.intern_symbol("x"),
        .add = symbols.intern_symbol("add"),
        .sub = symbols.intern_symbol("sub"),
        .mul = symbols.intern_symbol("mul")};
    arith_literals<symbol_table, expr_pool> literals{symbols, pool};
    arith_evaluator<decltype(literals), decltype(literals)> evaluator{literals, literals, vocabulary};
    arith_simplifier<decltype(literals), decltype(literals)> simplifier{literals, literals, vocabulary};
    const expr* x = pool.make_functor(vocabulary.input, {});
    std::vector<const expr*> out{};

    const expr* lit(int64_t value) { return literals.make_literal(value); }
    const expr* add(const expr* l, const expr* r) { return pool.make_functor(vocabulary.add, {l, r}); }
    const expr* sub(const expr* l, const expr* r) { return pool.make_functor(vocabulary.sub, {l, r}); }
    const expr* mul(const expr* l, const expr* r) { return pool.make_functor(vocabulary.mul, {l, r}); }
};

}

TEST_F(ArithAlgebraIntegrationTest, ALiteralIsInternedOnceAndReadsBack) {
    EXPECT_EQ(lit(-3), lit(-3));
    EXPECT_EQ(literals.find_value(lit(-3)), std::optional<int64_t>(-3));
    EXPECT_FALSE(literals.find_value(x).has_value());
}

TEST_F(ArithAlgebraIntegrationTest, EvaluationBindsTheInputAndReturnsAnInternedLiteral) {
    EXPECT_EQ(evaluator.evaluate_at(sub(mul(lit(2), x), lit(1)), lit(5)), std::optional<const expr*>(lit(9)));
}

TEST_F(ArithAlgebraIntegrationTest, AnExpressionHoldingAnExtensionTermHasNoValue) {
    EXPECT_FALSE(evaluator.evaluate_at(add(x, pool.make_var(0)), lit(5)).has_value());
}

TEST_F(ArithAlgebraIntegrationTest, TwoLiteralsAreFolded) {
    simplifier.propose(mul(lit(6), lit(7)), out);
    EXPECT_EQ(out, std::vector<const expr*>{lit(42)});
}

TEST_F(ArithAlgebraIntegrationTest, IdentitiesDropTheNeutralOperand) {
    simplifier.propose(add(x, lit(0)), out);
    simplifier.propose(mul(lit(1), x), out);
    simplifier.propose(mul(x, lit(0)), out);
    simplifier.propose(sub(x, x), out);
    EXPECT_EQ(out, (std::vector<const expr*>{x, x, lit(0), lit(0)}));
}
