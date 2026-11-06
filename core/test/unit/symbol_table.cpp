#include <gtest/gtest.h>
#include "infrastructure/symbol_table.hpp"

namespace {

struct SymbolTableTest : ::testing::Test {
    symbol_table symbols{};
};

}

TEST_F(SymbolTableTest, InterningANameTwiceGivesOneId) {
    EXPECT_EQ(symbols.intern_symbol("add"), symbols.intern_symbol("add"));
}

TEST_F(SymbolTableTest, IdsAreDenseInInterningOrderAndNameBack) {
    EXPECT_EQ(symbols.intern_symbol("add"), 0u);
    EXPECT_EQ(symbols.intern_symbol("mul"), 1u);
    EXPECT_EQ(symbols.name(1), "mul");
}
