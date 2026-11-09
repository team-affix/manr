#include <cstdint>
#include <optional>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/arith_hole_solver.hpp"
#include "infrastructure/expr_pool.hpp"
#include "value_objects/integer.hpp"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace {

struct MockFindLiteralValue {
    MOCK_METHOD(std::optional<integer>, find_value, (const expr*));
};

struct MockMakeLiteral {
    MOCK_METHOD(const expr*, make_literal, (const integer&));
};

constexpr uint32_t k_add = 20;
constexpr uint32_t k_sub = 21;
constexpr uint32_t k_mul = 22;
constexpr uint32_t k_pow = 23;

// literals are nullary functors whose id is their value plus 100
struct ArithHoleSolverTest : ::testing::Test {
    ArithHoleSolverTest() {
        ON_CALL(find_literal_value, find_value(_)).WillByDefault(Invoke([](const expr* e) -> std::optional<integer> {
            const expr::functor* f = std::get_if<expr::functor>(&e->content);
            if (f == nullptr || !f->args.empty())
                return std::nullopt;
            return integer(static_cast<int>(f->id) - 100);
        }));
        ON_CALL(make_literal, make_literal(_)).WillByDefault(Invoke([this](const integer& value) {
            return literal(value.convert_to<int>());
        }));
    }
    const expr* literal(int value) { return pool.make_functor(static_cast<uint32_t>(value + 100), {}); }

    ::testing::NiceMock<MockFindLiteralValue> find_literal_value;
    ::testing::NiceMock<MockMakeLiteral> make_literal;
    arith_hole_solver<MockFindLiteralValue, MockMakeLiteral> solver{find_literal_value, make_literal, k_add, k_sub,
                                                                    k_mul};
    expr_pool pool{};
    const expr* hole = pool.make_var(2);
};

}

TEST_F(ArithHoleSolverTest, AScaledHoleIsSolvedByDivision) {
    const expr* pattern = pool.make_functor(k_mul, {literal(2), hole});
    EXPECT_EQ(solver.solve_hole(pattern, 2, literal(10)), std::optional<const expr*>(literal(5)));
}

TEST_F(ArithHoleSolverTest, AnAffineFormIsSolvedThroughSumsAndDifferences) {
    // 3 - (2 * ?2 + 1) = -8  gives  ?2 = 5
    const expr* inner = pool.make_functor(k_add, {pool.make_functor(k_mul, {hole, literal(2)}), literal(1)});
    const expr* pattern = pool.make_functor(k_sub, {literal(3), inner});
    EXPECT_EQ(solver.solve_hole(pattern, 2, literal(-8)), std::optional<const expr*>(literal(5)));
}

TEST_F(ArithHoleSolverTest, NoIntegerSolutionGivesNothing) {
    const expr* pattern = pool.make_functor(k_mul, {literal(2), hole});
    EXPECT_FALSE(solver.solve_hole(pattern, 2, literal(7)).has_value());
}

TEST_F(ArithHoleSolverTest, AProductOfTwoHoledFactorsGivesNothing) {
    const expr* pattern = pool.make_functor(k_mul, {hole, hole});
    EXPECT_FALSE(solver.solve_hole(pattern, 2, literal(16)).has_value());
}

TEST_F(ArithHoleSolverTest, AFunctorOutsideTheLinearVocabularyGivesNothing) {
    const expr* pattern = pool.make_functor(k_pow, {hole, literal(2)});
    EXPECT_FALSE(solver.solve_hole(pattern, 2, literal(16)).has_value());
}

TEST_F(ArithHoleSolverTest, AValueThatIsNotALiteralGivesNothing) {
    const expr* pattern = pool.make_functor(k_mul, {literal(2), hole});
    EXPECT_FALSE(solver.solve_hole(pattern, 2, pool.make_functor(k_add, {literal(1), literal(1)})).has_value());
}
