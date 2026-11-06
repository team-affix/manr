#include <stdexcept>
#include "poly_kernel_fixture.hpp"

namespace {

struct PolyTextIntegrationTest : PolyKernelFixture {};

}

TEST_F(PolyTextIntegrationTest, PrecedenceAndAssociativityRoundTrip) {
    for (const std::string text : {"x^2 - 3 * x * (y + 1)", "a - (b - c)", "a + (b + c)", "(a * b)^3", "(x^2)^3",
                                   "-3 * x + (-2)", "2 * (-1)"})
        EXPECT_EQ(print(parse(text)), text);
}

TEST_F(PolyTextIntegrationTest, UnaryMinusOfALiteralIsANegativeLiteralAndOfAnythingElseATimesMinusOne) {
    EXPECT_EQ(parse("-4"), kernel.literals_.make_literal(-4));
    EXPECT_EQ(print(parse("-x^2")), "-1 * x^2");
}

TEST_F(PolyTextIntegrationTest, MalformedTextThrows) {
    EXPECT_THROW(parse("x +"), std::invalid_argument);
    EXPECT_THROW(parse("(x"), std::invalid_argument);
    EXPECT_THROW(parse("x ^ y"), std::invalid_argument);
}
