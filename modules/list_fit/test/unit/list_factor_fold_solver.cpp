#include <optional>
#include <vector>
#include <gtest/gtest.h>
#include "infrastructure/list_factor_fold_solver.hpp"

namespace {

struct ListFactorFoldSolverTest : ::testing::Test {
    // every list of one to three elements in [-3, 3], with b times the product of g over its elements
    void fill_lists(const integer& base, integer (*g)(const integer&)) {
        for (int first = -3; first <= 3; ++first) {
            add({first}, base, g);
            for (int second = -3; second <= 3; ++second) {
                add({first, second}, base, g);
                for (int third = -3; third <= 3; third += 2)
                    add({first, second, third}, base, g);
            }
        }
    }
    void add(const std::vector<integer>& list, const integer& base, integer (*g)(const integer&)) {
        integer product = base;
        for (const integer& value : list)
            product *= g(value);
        lists.push_back(list);
        outputs.push_back(product);
    }

    list_factor_fold_solver solver;
    std::vector<std::vector<integer>> lists;
    std::vector<integer> outputs;
};

integer squared(const integer& value) {
    return value * value;
}

integer shifted(const integer& value) {
    return value - 1;
}

integer squared_plus_one(const integer& value) {
    return value * value + 1;
}

}

TEST_F(ListFactorFoldSolverTest, SquaresArePinnedWithZeroAtTheirRoot) {
    fill_lists(1, squared);
    const std::optional<list_factor_fold> fold = solver.solve(lists, outputs);
    ASSERT_TRUE(fold.has_value());
    EXPECT_EQ(fold.value(), (list_factor_fold{.factors = {{-3, 9}, {-2, 4}, {-1, 1}, {0, 0}, {1, 1}, {2, 4}, {3, 9}},
                                              .base = 1}));
}

TEST_F(ListFactorFoldSolverTest, NegativeFactorsAreReadFromTheSignsOfTheOutputs) {
    fill_lists(-2, shifted);
    const std::optional<list_factor_fold> fold = solver.solve(lists, outputs);
    ASSERT_TRUE(fold.has_value());
    EXPECT_EQ(fold.value(),
              (list_factor_fold{.factors = {{-3, -4}, {-2, -3}, {-1, -2}, {0, -1}, {1, 0}, {2, 1}, {3, 2}}, .base = -2}));
}

TEST_F(ListFactorFoldSolverTest, AFoldReproducesTheListsItWasSolvedFrom) {
    fill_lists(3, squared_plus_one);
    const std::optional<list_factor_fold> fold = solver.solve(lists, outputs);
    ASSERT_TRUE(fold.has_value());
    for (size_t index = 0; index < lists.size(); ++index)
        EXPECT_EQ(solver.multiply(fold.value(), lists[index]), outputs[index]);
}

TEST_F(ListFactorFoldSolverTest, AllZeroOutputsGiveNothing) {
    lists = {{0, 1}, {2, 0}, {0}};
    outputs = {0, 0, 0};
    EXPECT_FALSE(solver.solve(lists, outputs).has_value());
}

TEST_F(ListFactorFoldSolverTest, NoMoreListsThanUnknownsGiveNothing) {
    lists = {{1, 2}, {2, 3}, {3, 1}};
    outputs = {2, 6, 3};
    EXPECT_FALSE(solver.solve(lists, outputs).has_value());
}
