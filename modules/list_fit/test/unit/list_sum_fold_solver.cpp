#include <optional>
#include <vector>
#include <gtest/gtest.h>
#include "infrastructure/list_sum_fold_solver.hpp"

namespace {

struct ListSumFoldSolverTest : ::testing::Test {
    // every list of one to three elements in [-2, 2], with b plus the sum of g over its elements
    void fill_lists(const integer& base, integer (*g)(const integer&)) {
        for (int first = -2; first <= 2; ++first) {
            add({first}, base, g);
            for (int second = -2; second <= 2; ++second) {
                add({first, second}, base, g);
                for (int third = -2; third <= 2; third += 2)
                    add({first, second, third}, base, g);
            }
        }
    }
    void add(const std::vector<integer>& list, const integer& base, integer (*g)(const integer&)) {
        integer total = base;
        for (const integer& value : list)
            total += g(value);
        lists.push_back(list);
        outputs.push_back(total);
    }

    list_sum_fold_solver solver;
    std::vector<std::vector<integer>> lists;
    std::vector<integer> outputs;
};

integer is_even(const integer& value) {
    return value % 2 == 0 ? 1 : 0;
}

integer square(const integer& value) {
    return value * value;
}

}

TEST_F(ListSumFoldSolverTest, RecoversACountOnAnyCondition) {
    fill_lists(0, is_even);
    const std::optional<list_sum_fold> fold = solver.solve(lists, outputs, 0);
    ASSERT_TRUE(fold.has_value());
    EXPECT_EQ(fold.value(), (list_sum_fold{.terms = {{-2, 1}, {-1, 0}, {0, 1}, {1, 0}, {2, 1}}, .base = 0}));
}

TEST_F(ListSumFoldSolverTest, RecoversTheBase) {
    fill_lists(7, square);
    const std::optional<list_sum_fold> fold = solver.solve(lists, outputs, 3);
    ASSERT_TRUE(fold.has_value());
    EXPECT_EQ(fold.value().base, integer(7));
    EXPECT_EQ(fold.value().terms.at(-2), integer(4));
}

TEST_F(ListSumFoldSolverTest, SumsAListWithItsFold) {
    const list_sum_fold fold{.terms = {{1, 10}, {2, 20}}, .base = 3};
    EXPECT_EQ(solver.sum(fold, {1, 2, 2}), integer(53));
}

TEST_F(ListSumFoldSolverTest, FindsNothingWithNoMoreListsThanUnknowns) {
    add({1, 2}, 0, square);
    add({2, 3}, 0, square);
    EXPECT_FALSE(solver.solve(lists, outputs, 0).has_value());
}

TEST_F(ListSumFoldSolverTest, FindsNothingWhenTheListsDoNotPinEveryValue) {
    // 1 and 2 always occur together, so only their sum is known
    for (int count = 1; count <= 6; ++count) {
        std::vector<integer> list;
        for (int repeat = 0; repeat < count; ++repeat) {
            list.push_back(1);
            list.push_back(2);
        }
        add(list, 0, square);
    }
    EXPECT_FALSE(solver.solve(lists, outputs, 0).has_value());
}
