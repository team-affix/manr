#include <vector>
#include <gtest/gtest.h>
#include "infrastructure/expr_pool.hpp"
#include "rules/list_identity_map_remover.hpp"

namespace {

struct ListIdentityMapRemoverTest : ::testing::Test {
    expr_pool pool{};
    list_symbols symbols{.nil = 1, .cons = 2, .append = 3, .reverse = 4, .map = 5, .filter = 6, .fold = 7,
                         .lambda = 8, .input = 9, .element = 9, .head = 9, .accumulator = 10};
    list_identity_map_remover remover{symbols};
    const expr* element = pool.make_functor(9, {});
    const expr* xs = pool.make_functor(30, {});
    std::vector<rewrite> out{};
};

}

TEST_F(ListIdentityMapRemoverTest, AMapOfTheIdentityBecomesItsList) {
    remover.detect(pool.make_functor(5, {pool.make_functor(8, {element}), xs}), out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0](), xs);
}

TEST_F(ListIdentityMapRemoverTest, AMapOfAnotherBodyIsLeftAlone) {
    remover.detect(pool.make_functor(5, {pool.make_functor(8, {pool.make_functor(10, {})}), xs}), out);
    EXPECT_TRUE(out.empty());
}
