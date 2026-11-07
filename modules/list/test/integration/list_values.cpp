#include <optional>
#include <vector>
#include <gtest/gtest.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/list_values.hpp"
#include "value_objects/list_symbols.hpp"

namespace {

struct ListValuesIntegrationTest : ::testing::Test {
    expr_pool exprs;
    list_symbols symbols{.nil = 0, .cons = 1, .map = 2, .filter = 3, .fold = 4, .input = 5, .element = 6, .head = 7,
                         .accumulator = 8};
    list_values<expr_pool> values{exprs, symbols};
    const expr* one = exprs.make_functor(100, {});
    const expr* two = exprs.make_functor(101, {});
};

}

TEST_F(ListValuesIntegrationTest, AListIsAConsChainEndingInNil) {
    const expr* nil = exprs.make_functor(symbols.nil, {});
    EXPECT_EQ(values.make_list({one, two}),
              exprs.make_functor(symbols.cons, {one, exprs.make_functor(symbols.cons, {two, nil})}));
}

TEST_F(ListValuesIntegrationTest, TheElementsOfAListAreReadBackInOrder) {
    EXPECT_EQ(values.find_elements(values.make_list({one, two, one})),
              std::optional<std::vector<const expr*>>({one, two, one}));
}

TEST_F(ListValuesIntegrationTest, TheEmptyListHasNoElements) {
    EXPECT_EQ(values.find_elements(values.make_list({})), std::optional<std::vector<const expr*>>(std::vector<const expr*>{}));
}

TEST_F(ListValuesIntegrationTest, AChainNotEndingInNilIsNotAList) {
    EXPECT_EQ(values.find_elements(exprs.make_functor(symbols.cons, {one, two})), std::nullopt);
}
