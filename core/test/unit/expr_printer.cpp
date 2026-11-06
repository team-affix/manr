#include <sstream>
#include <string>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_printer.hpp"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {

struct MockGetSymbolName {
    MOCK_METHOD(const std::string&, name, (uint32_t), (const));
};

struct ExprPrinterTest : ::testing::Test {
    std::string f_name{"f"};
    std::string a_name{"a"};
    NiceMock<MockGetSymbolName> get_symbol_name;
    expr_printer<MockGetSymbolName> printer{get_symbol_name};
    expr_pool pool{};
    std::ostringstream os{};

    void SetUp() override {
        ON_CALL(get_symbol_name, name(0)).WillByDefault(ReturnRef(f_name));
        ON_CALL(get_symbol_name, name(1)).WillByDefault(ReturnRef(a_name));
    }
};

}

TEST_F(ExprPrinterTest, PrintsFunctorsWithArgumentsAndExtensionTermsWithAQuestionMark) {
    printer.print(os, pool.make_functor(0, {pool.make_functor(1, {}), pool.make_var(2)}));
    EXPECT_EQ(os.str(), "f(a, ?2)");
}
