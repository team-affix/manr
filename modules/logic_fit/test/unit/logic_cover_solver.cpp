#include <cstdint>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/logic_cover_solver.hpp"

using ::testing::UnorderedElementsAre;

namespace {

struct LogicCoverSolverTest : ::testing::Test {
    logic_truth_table majority() {
        logic_truth_table table{.width = 3, .rows = {}};
        for (uint64_t row = 0; row < 8; ++row)
            table.rows.emplace(row, std::popcount(row) >= 2);
        return table;
    }

    logic_cover_solver solver{k_logic_default_max_cover_width};
};

}

TEST_F(LogicCoverSolverTest, CoversMajorityWithItsThreePairs) {
    EXPECT_THAT(solver.find_cover(majority(), true),
                UnorderedElementsAre(logic_implicant{.mask = 0b011, .bits = 0b011},
                                     logic_implicant{.mask = 0b101, .bits = 0b101},
                                     logic_implicant{.mask = 0b110, .bits = 0b110}));
}

TEST_F(LogicCoverSolverTest, CoversTheZerosWithNegatedLiterals) {
    EXPECT_THAT(solver.find_cover(majority(), false),
                UnorderedElementsAre(logic_implicant{.mask = 0b011, .bits = 0b000},
                                     logic_implicant{.mask = 0b101, .bits = 0b000},
                                     logic_implicant{.mask = 0b110, .bits = 0b000}));
}

TEST_F(LogicCoverSolverTest, GeneralizesOverUnseenInputs) {
    // a and b known everywhere but at (1, 1), where the cover is free to include it
    const logic_truth_table table{.width = 2, .rows = {{0b00, false}, {0b01, true}, {0b10, false}}};
    EXPECT_THAT(solver.find_cover(table, true), UnorderedElementsAre(logic_implicant{.mask = 0b01, .bits = 0b01}));
}

TEST_F(LogicCoverSolverTest, FindsNothingWithoutARowOfThatOutput) {
    const logic_truth_table table{.width = 2, .rows = {{0b00, false}, {0b11, false}}};
    EXPECT_TRUE(solver.find_cover(table, true).empty());
}

TEST_F(LogicCoverSolverTest, FindsNothingPastItsWidth) {
    const logic_cover_solver narrow{2};
    EXPECT_TRUE(narrow.find_cover(majority(), true).empty());
}
