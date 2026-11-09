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
        .input = symbols.intern_symbol("#0"),
        .add = symbols.intern_symbol("add"),
        .sub = symbols.intern_symbol("sub"),
        .mul = symbols.intern_symbol("mul")};
    arith_literals<symbol_table, expr_pool> literals{symbols, pool};
    arith_evaluator<decltype(literals), decltype(literals)> evaluator{literals, literals, vocabulary};
    arith_simplifier<decltype(literals), decltype(literals)> simplifier{literals, literals, vocabulary};
    const expr* x = pool.make_functor(vocabulary.input, {});
    std::vector<rewrite> out{};

    std::vector<const expr*> perform() {
        std::vector<const expr*> results;
        for (const rewrite& detected : out)
            results.push_back(detected());
        return results;
    }

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
    simplifier.detect(mul(lit(6), lit(7)), out);
    EXPECT_EQ(perform(), std::vector<const expr*>{lit(42)});
}

TEST_F(ArithAlgebraIntegrationTest, IdentitiesDropTheNeutralOperand) {
    simplifier.detect(add(x, lit(0)), out);
    simplifier.detect(mul(lit(1), x), out);
    simplifier.detect(mul(x, lit(0)), out);
    simplifier.detect(sub(x, x), out);
    EXPECT_EQ(perform(), (std::vector<const expr*>{x, x, lit(0), lit(0)}));
}

TEST_F(ArithAlgebraIntegrationTest, FoldingPast64BitsIsExact) {
    simplifier.detect(mul(literals.make_literal(integer("9223372036854775807")), lit(2)), out);
    EXPECT_EQ(perform(), std::vector<const expr*>{literals.make_literal(integer("18446744073709551614"))});
}
