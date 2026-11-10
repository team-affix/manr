#include <optional>
#include <variant>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/num_prime_hypotheses.hpp"
#include "value_objects/integer.hpp"

using ::testing::_;
using ::testing::Invoke;

namespace {

struct MockFindLiteralValue {
    MOCK_METHOD(std::optional<integer>, find_value, (const expr*));
};
struct MockFindNthPrime {
    MOCK_METHOD(std::optional<integer>, find_nth_prime, (const integer&));
};
struct MockFindPrimality {
    MOCK_METHOD(std::optional<bool>, find_primality, (const integer&));
};

constexpr uint32_t k_point = 40;
constexpr uint32_t k_prime = 30;
constexpr uint32_t k_is_prime = 31;
constexpr uint32_t k_variable = 1;
const int k_primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29};

// literals are nullary functors whose id is their value plus 100; the primes are the first ten
struct NumPrimeHypothesesTest : ::testing::Test {
    NumPrimeHypothesesTest() {
        ON_CALL(find_literal_value, find_value(_)).WillByDefault(Invoke([](const expr* e) -> std::optional<integer> {
            const expr::functor* f = std::get_if<expr::functor>(&e->content);
            if (f == nullptr || !f->args.empty() || f->id < 100)
                return std::nullopt;
            return integer(static_cast<int>(f->id) - 100);
        }));
        ON_CALL(find_nth_prime, find_nth_prime(_)).WillByDefault(Invoke([](const integer& index) {
            return integer(k_primes[index.convert_to<int>()]);
        }));
        ON_CALL(find_primality, find_primality(_)).WillByDefault(Invoke([](const integer& value) {
            for (const int prime : k_primes)
                if (value == prime)
                    return true;
            return false;
        }));
    }
    const expr* literal(int value) { return pool.make_functor(static_cast<uint32_t>(value + 100), {}); }
    pw_data_point point(int input, int output) {
        return pw_data_point{.input = pool.make_functor(k_point, {literal(input)}), .output = literal(output)};
    }

    expr_pool pool{};
    ::testing::NiceMock<MockFindLiteralValue> find_literal_value;
    ::testing::NiceMock<MockFindNthPrime> find_nth_prime;
    ::testing::NiceMock<MockFindPrimality> find_primality;
    num_prime_hypotheses<MockFindLiteralValue, MockFindNthPrime, MockFindPrimality, expr_pool> detector{
        find_literal_value, find_nth_prime, find_primality, pool,
        num_symbols{.prime = k_prime, .is_prime = k_is_prime}, k_variable};
    std::vector<const expr*> out;
    const expr* variable = pool.make_functor(k_variable, {});
};

}

TEST_F(NumPrimeHypothesesTest, TheNthPrimesAreReadAsPrime) {
    detector.propose_hypotheses({point(0, 2), point(1, 3), point(2, 5), point(3, 7)}, out);
    EXPECT_EQ(out, std::vector<const expr*>{pool.make_functor(k_prime, {variable})});
}

TEST_F(NumPrimeHypothesesTest, TruthValuesAreReadAsPrimality) {
    detector.propose_hypotheses({point(1, 0), point(2, 1), point(3, 1), point(4, 0), point(5, 1)}, out);
    EXPECT_EQ(out, std::vector<const expr*>{pool.make_functor(k_is_prime, {variable})});
}

TEST_F(NumPrimeHypothesesTest, AReadingExplainingMostPointsIsStillProposed) {
    detector.propose_hypotheses({point(0, 2), point(1, 3), point(2, 6), point(3, 7)}, out);
    EXPECT_EQ(out.size(), 1u);
}

TEST_F(NumPrimeHypothesesTest, DataUnlikeThePrimesIsRefuted) {
    detector.propose_hypotheses({point(0, 1), point(1, 4), point(2, 9), point(3, 16)}, out);
    EXPECT_TRUE(out.empty());
}

TEST_F(NumPrimeHypothesesTest, AllZerosAreLeftToTheConstantDetector) {
    detector.propose_hypotheses({point(0, 0), point(1, 0), point(4, 0)}, out);
    EXPECT_TRUE(out.empty());
}

TEST_F(NumPrimeHypothesesTest, KeysOfTwoCoordinatesAreNotRead) {
    const pw_data_point pair{.input = pool.make_functor(k_point, {literal(1), literal(2)}), .output = literal(3)};
    detector.propose_hypotheses({pair}, out);
    EXPECT_TRUE(out.empty());
}
