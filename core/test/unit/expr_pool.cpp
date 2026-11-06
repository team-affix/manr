#include <gtest/gtest.h>
#include "infrastructure/expr_pool.hpp"

namespace {

struct ExprPoolTest : ::testing::Test {
    expr_pool pool{};
};

}

TEST_F(ExprPoolTest, StructurallyEqualFunctorsShareOneAddress) {
    const expr* a = pool.make_functor(1, {pool.make_var(0)});
    const expr* b = pool.make_functor(1, {pool.make_var(0)});
    EXPECT_EQ(a, b);
    EXPECT_EQ(pool.size(), 2u);
}

TEST_F(ExprPoolTest, DifferentArgumentsGiveDifferentAddresses) {
    EXPECT_NE(pool.make_functor(1, {pool.make_var(0)}), pool.make_functor(1, {pool.make_var(1)}));
}

TEST_F(ExprPoolTest, AVarAndANullaryFunctorWithTheSameNumberDiffer) {
    EXPECT_NE(pool.make_var(3), pool.make_functor(3, {}));
}
