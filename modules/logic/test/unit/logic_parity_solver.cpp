#include <cstdint>
#include <optional>
#include <gtest/gtest.h>
#include "infrastructure/logic_parity_solver.hpp"

namespace {

struct LogicParitySolverTest : ::testing::Test {
    // the full table of width `width` whose output is the parity of the bits in `variables`, negated when `inverted`
    logic_truth_table table_of(size_t width, uint64_t variables, bool inverted) {
        logic_truth_table table{.width = width, .rows = {}};
        for (uint64_t row = 0; row < (uint64_t(1) << width); ++row)
            table.rows.emplace(row, (std::popcount(row & variables) % 2 == 1) != inverted);
        return table;
    }

    logic_parity_solver solver;
};

}

TEST_F(LogicParitySolverTest, SolvesTheXorOfSomeVariables) {
    const std::optional<logic_parity> parity = solver.solve(table_of(4, 0b1011, false));
    ASSERT_TRUE(parity.has_value());
    EXPECT_EQ(parity.value(), (logic_parity{.variables = 0b1011, .inverted = false}));
}

TEST_F(LogicParitySolverTest, SolvesANegatedXor) {
    const std::optional<logic_parity> parity = solver.solve(table_of(2, 0b11, true));
    ASSERT_TRUE(parity.has_value());
    EXPECT_EQ(parity.value(), (logic_parity{.variables = 0b11, .inverted = true}));
}

TEST_F(LogicParitySolverTest, RefutesATableThatIsNoParity) {
    logic_truth_table table = table_of(3, 0b111, false);
    table.rows[0b011] = true;
    EXPECT_FALSE(solver.solve(table).has_value());
}

TEST_F(LogicParitySolverTest, LeavesOutVariablesThePartialTableDoesNotDetermine) {
    logic_truth_table table{.width = 2, .rows = {{0b00, false}, {0b01, true}}};
    const std::optional<logic_parity> parity = solver.solve(table);
    ASSERT_TRUE(parity.has_value());
    EXPECT_EQ(parity.value(), (logic_parity{.variables = 0b01, .inverted = false}));
}
