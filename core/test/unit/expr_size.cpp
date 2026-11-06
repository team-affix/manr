#include <gtest/gtest.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_size.hpp"

namespace {

struct ExprSizeTest : ::testing::Test {
    expr_pool pool{};
    expr_size sizes{};
};

}

TEST_F(ExprSizeTest, ALeafCostsOne) {
    EXPECT_EQ(sizes.cost(pool.make_var(0)), 1u);
    EXPECT_EQ(sizes.cost(pool.make_functor(0, {})), 1u);
}

TEST_F(ExprSizeTest, ASharedSubtermCountsOncePerOccurrence) {
    const expr* leaf = pool.make_functor(0, {});
    const expr* pair = pool.make_functor(1, {leaf, leaf});
    EXPECT_EQ(sizes.cost(pool.make_functor(2, {pair, pair})), 7u);
}
