#include <optional>
#include <gtest/gtest.h>
#include "infrastructure/num_prime_table.hpp"
#include "value_objects/integer.hpp"

namespace {

struct NumPrimeTableTest : ::testing::Test {
    num_prime_table table{1000};
};

}

TEST_F(NumPrimeTableTest, TheNthPrimeCountsFromZero) {
    EXPECT_EQ(table.find_nth_prime(0), std::optional<integer>(2));
    EXPECT_EQ(table.find_nth_prime(4), std::optional<integer>(11));
    EXPECT_EQ(table.find_nth_prime(30), std::optional<integer>(127));
}

TEST_F(NumPrimeTableTest, ANegativeIndexHasNoPrime) {
    EXPECT_FALSE(table.find_nth_prime(-1).has_value());
}

TEST_F(NumPrimeTableTest, APrimePastTheRangeHasNoAnswer) {
    EXPECT_EQ(table.find_nth_prime(167), std::optional<integer>(997));
    EXPECT_FALSE(table.find_nth_prime(168).has_value());
}

TEST_F(NumPrimeTableTest, PrimalityIsDecidedWithinTheRange) {
    EXPECT_EQ(table.find_primality(97), std::optional<bool>(true));
    EXPECT_EQ(table.find_primality(91), std::optional<bool>(false));
    EXPECT_EQ(table.find_primality(1), std::optional<bool>(false));
    EXPECT_EQ(table.find_primality(-7), std::optional<bool>(false));
    EXPECT_FALSE(table.find_primality(1009).has_value());
}

TEST_F(NumPrimeTableTest, AnswersStayRightAsTheSieveGrows) {
    EXPECT_EQ(table.find_primality(3), std::optional<bool>(true));
    EXPECT_EQ(table.find_primality(997), std::optional<bool>(true));
    EXPECT_EQ(table.find_nth_prime(1), std::optional<integer>(3));
}
