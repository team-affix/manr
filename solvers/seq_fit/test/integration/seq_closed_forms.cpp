#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "composition/seq_fit_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"

using ::testing::IsEmpty;

namespace {

// Linear recurrences with constant coefficients, closed into sums of powers and a polynomial of the index.
struct SeqClosedFormsIntegrationTest : ::testing::Test {
    seq_fit_manifest manifest{{}, k_poly_default_limits, k_poly_fit_default_limits, 1};
    seq_fit_rules_manifest& rules = manifest.rule_set_;
    poly_kernel_manifest& kernel = manifest.kernel_;
    std::vector<rewrite> out{};

    // rec(lams(order + 1, step), bases..., #0)
    const expr* recurrence(const std::string& step, const std::vector<int>& bases) {
        std::vector<const expr*> args{kernel.exprs_.make_functor(
            rules.abs_symbols_.lambdas,
            {kernel.literals_.make_literal(static_cast<int>(bases.size()) + 1), kernel.parser_.parse(step)})};
        for (const int base : bases)
            args.push_back(kernel.literals_.make_literal(base));
        args.push_back(kernel.parser_.parse("#0"));
        return kernel.exprs_.make_functor(rules.seq_symbols_.recurrence, args);
    }
    std::vector<std::string> closed(const std::string& step, const std::vector<int>& bases) {
        out.clear();
        rules.linear_closer_.detect(recurrence(step, bases), out);
        std::vector<std::string> texts;
        for (const rewrite& r : out) {
            std::ostringstream os;
            kernel.printer_.print(os, r());
            texts.push_back(os.str());
        }
        return texts;
    }
};

}

TEST_F(SeqClosedFormsIntegrationTest, ASecondOrderRecurrenceWithIntegerRootsIsASumOfPowers) {
    // reduce request #35
    EXPECT_EQ(closed("5 * #1 - 6 * #2", {2, 5}), std::vector<std::string>{"exp(2, #0) + exp(3, #0)"});
}

TEST_F(SeqClosedFormsIntegrationTest, AStepUsingTheIndexAddsAPolynomial) {
    // 2^n - n: the root 1 of the characteristic polynomial and the constant 1 make the polynomial linear
    EXPECT_EQ(closed("3 * #1 - 2 * #2 + 1", {1, 1}), std::vector<std::string>{"exp(2, #0) - #0"});
    EXPECT_EQ(closed("2 * #1 - #0 + 2", {1}), std::vector<std::string>{"exp(2, #0) + #0"});
}

TEST_F(SeqClosedFormsIntegrationTest, ANegativeAmplitudeComesAfterAPositiveOne) {
    EXPECT_EQ(closed("5 * #1 - 6 * #2", {0, 1}), std::vector<std::string>{"exp(3, #0) - exp(2, #0)"});
}

TEST_F(SeqClosedFormsIntegrationTest, IrrationalRootsRepeatedOnesOrFractionsStayARecurrence) {
    EXPECT_THAT(closed("#1 + #2", {0, 1}), IsEmpty());
    EXPECT_THAT(closed("4 * #1 - 4 * #2", {1, 2}), IsEmpty());
    // a(n) = 2^n + (n^2 - 3n) / 2
    EXPECT_THAT(closed("#0 + 3 * #1 - 2 * #2 - 1", {1, 1}), IsEmpty());
}

TEST_F(SeqClosedFormsIntegrationTest, AnAffineStepOfOrderOneIsLeftToThePowerCloser) {
    EXPECT_THAT(closed("2 * #1 + 1", {0}), IsEmpty());
}

