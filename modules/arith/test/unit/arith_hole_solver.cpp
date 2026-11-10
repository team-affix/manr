#include <cstdint>
#include <map>
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
                                                                    k_mul, k_pow};
    expr_pool pool{};
    const expr* hole = pool.make_var(2);
    std::map<uint32_t, const expr*> bindings{};

    using solution_t = std::optional<std::map<uint32_t, const expr*>>;
    solution_t solve(const expr* pattern, const expr* value) { return solver.solve_holes(pattern, value, bindings); }
    solution_t only(int value) { return std::map<uint32_t, const expr*>{{2, literal(value)}}; }
};

}

TEST_F(ArithHoleSolverTest, AScaledHoleIsSolvedByDivision) {
    EXPECT_EQ(solve(pool.make_functor(k_mul, {literal(2), hole}), literal(10)), only(5));
}

TEST_F(ArithHoleSolverTest, AnAffineFormIsSolvedThroughSumsAndDifferences) {
    // 3 - (2 * ?2 + 1) = -8  gives  ?2 = 5
    const expr* inner = pool.make_functor(k_add, {pool.make_functor(k_mul, {hole, literal(2)}), literal(1)});
    EXPECT_EQ(solve(pool.make_functor(k_sub, {literal(3), inner}), literal(-8)), only(5));
}

TEST_F(ArithHoleSolverTest, NoIntegerSolutionGivesNothing) {
    EXPECT_FALSE(solve(pool.make_functor(k_mul, {literal(2), hole}), literal(7)).has_value());
}

TEST_F(ArithHoleSolverTest, AQuadraticIsSolvedByItsPositiveRootOfLeastMagnitude) {
    // ?2^2 + 1 = 10  gives  ?2 = 3 (or -3)
    const expr* pattern = pool.make_functor(k_add, {pool.make_functor(k_pow, {hole, literal(2)}), literal(1)});
    EXPECT_EQ(solve(pattern, literal(10)), only(3));
}

TEST_F(ArithHoleSolverTest, AProductOfHoledFactorsIsSolvedAsAPolynomial) {
    // ?2 * (?2 + 1) = 12  gives  ?2 = 3 (or -4)
    const expr* pattern = pool.make_functor(k_mul, {hole, pool.make_functor(k_add, {hole, literal(1)})});
    EXPECT_EQ(solve(pattern, literal(12)), only(3));
}

TEST_F(ArithHoleSolverTest, APolynomialWithoutIntegerRootsGivesNothing) {
    EXPECT_FALSE(solve(pool.make_functor(k_mul, {hole, hole}), literal(10)).has_value());
}

TEST_F(ArithHoleSolverTest, AHoleOfVanishingCoefficientsIsFreeAndBoundToZero) {
    // ?2 - ?2 + 4 = 4 holds for every ?2
    const expr* pattern = pool.make_functor(k_add, {pool.make_functor(k_sub, {hole, hole}), literal(4)});
    EXPECT_EQ(solve(pattern, literal(4)), only(0));
}

TEST_F(ArithHoleSolverTest, OtherUnboundHolesAreSetToZeroAndTheLastSolvedFor) {
    // ?1 + ?2 = 10  gives  ?1 = 0, ?2 = 10
    const expr* pattern = pool.make_functor(k_add, {pool.make_var(1), hole});
    EXPECT_EQ(solve(pattern, literal(10)), (solution_t{{{1, literal(0)}, {2, literal(10)}}}));
}

TEST_F(ArithHoleSolverTest, OtherUnboundHolesAreSetToOneWhenZeroLeavesNoSolution) {
    // ?1 * ?2 = 6: zero leaves 0 = 6, one gives ?2 = 6
    const expr* pattern = pool.make_functor(k_mul, {pool.make_var(1), hole});
    EXPECT_EQ(solve(pattern, literal(6)), (solution_t{{{1, literal(1)}, {2, literal(6)}}}));
}

TEST_F(ArithHoleSolverTest, BoundHolesTakeTheirValues) {
    bindings.emplace(1, literal(4));
    const expr* pattern = pool.make_functor(k_add, {pool.make_var(1), hole});
    EXPECT_EQ(solve(pattern, literal(10)), only(6));
}

TEST_F(ArithHoleSolverTest, APatternWhoseHolesAreAllBoundChecksItsValue) {
    bindings.emplace(2, literal(4));
    const expr* pattern = pool.make_functor(k_mul, {literal(2), hole});
    EXPECT_EQ(solve(pattern, literal(8)), (solution_t{std::map<uint32_t, const expr*>{}}));
    EXPECT_FALSE(solve(pattern, literal(9)).has_value());
}

TEST_F(ArithHoleSolverTest, AFunctorOutsideThePolynomialVocabularyGivesNothing) {
    EXPECT_FALSE(solve(pool.make_functor(77, {hole, literal(2)}), literal(16)).has_value());
}

TEST_F(ArithHoleSolverTest, AValueThatIsNotALiteralGivesNothing) {
    const expr* pattern = pool.make_functor(k_mul, {literal(2), hole});
    EXPECT_FALSE(solve(pattern, pool.make_functor(k_add, {literal(1), literal(1)})).has_value());
}
