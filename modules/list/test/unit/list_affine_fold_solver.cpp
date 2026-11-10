#include <optional>
#include <vector>
#include <gtest/gtest.h>
#include "infrastructure/list_affine_fold_solver.hpp"

namespace {

struct ListAffineFoldSolverTest : ::testing::Test {
    // every list of three elements in [-2, 2], with the value of `fold(h + multiplier * acc, base, xs)` on it
    void fill_three_element_lists(const integer& multiplier, const integer& base) {
        for (int first = -2; first <= 2; ++first)
            for (int second = -2; second <= 2; ++second)
                for (int third = -2; third <= 2; ++third) {
                    lists.push_back({first, second, third});
                    outputs.push_back(first + multiplier * (second + multiplier * (third + multiplier * base)));
                }
    }

    list_affine_fold_solver solver;
    std::vector<std::vector<integer>> lists;
    std::vector<integer> outputs;
};

}

TEST_F(ListAffineFoldSolverTest, ASumIsPinnedByListsOfOneLengthWithoutTheirTails) {
    fill_three_element_lists(1, 0);
    const std::optional<list_affine_fold> fold = solver.solve(lists, outputs, 1, 6, 0);
    ASSERT_TRUE(fold.has_value());
    EXPECT_EQ(fold.value(), (list_affine_fold{.multiplier = 1, .coefficients = {0, 1, 0, 0, 0}, .base = 0}));
}

TEST_F(ListAffineFoldSolverTest, AMultiplierWeighsEachElementByItsPosition) {
    lists = {{3}, {1, 2}, {4, 0, 7}, {9, 9}, {2, 5, 1, 8}, {6, 3, 0}, {7}, {0, 4, 4, 2}};
    for (const std::vector<integer>& list : lists) {
        integer number = 0;
        for (size_t index = list.size(); index-- > 0;)
            number = list[index] + 10 * number;
        outputs.push_back(number);
    }
    const std::optional<list_affine_fold> fold = solver.solve(lists, outputs, 10, 1, 0);
    ASSERT_TRUE(fold.has_value());
    EXPECT_EQ(fold.value(), (list_affine_fold{.multiplier = 10, .coefficients = {0, 1}, .base = 0}));
}

TEST_F(ListAffineFoldSolverTest, ListsOfSeveralLengthsSeparateTheBaseFromTheStepsConstant) {
    lists = {{}, {1}, {2, -1}, {0, 3, 1}, {-2, 2}, {1, 1, 1, 1}, {3}};
    for (const std::vector<integer>& list : lists) {
        integer total = 5;
        for (const integer& value : list)
            total += value + 2;
        outputs.push_back(total);
    }
    const std::optional<list_affine_fold> fold = solver.solve(lists, outputs, 1, 1, 0);
    ASSERT_TRUE(fold.has_value());
    EXPECT_EQ(fold.value(), (list_affine_fold{.multiplier = 1, .coefficients = {2, 1}, .base = 5}));
}

TEST_F(ListAffineFoldSolverTest, ABaseThatIsAProperFractionGivesNothing) {
    fill_three_element_lists(2, 0);
    for (integer& output : outputs)
        output += 4;
    EXPECT_FALSE(solver.solve(lists, outputs, 2, 1, 0).has_value());
}

TEST_F(ListAffineFoldSolverTest, TooFewListsToCheckOneBeyondTheUnknownsGiveNothing) {
    lists = {{1, 2}, {3, 4}, {5, 6}};
    outputs = {3, 7, 11};
    EXPECT_FALSE(solver.solve(lists, outputs, 1, 6, 0).has_value());
}

TEST_F(ListAffineFoldSolverTest, AccumulatingFoldsEverySuffixDownToTheBase) {
    const list_affine_fold digits{.multiplier = 10, .coefficients = {0, 1}, .base = 0};
    EXPECT_EQ(solver.accumulate(digits, {1, 2, 3}), (std::vector<integer>{321, 32, 3, 0}));
}
