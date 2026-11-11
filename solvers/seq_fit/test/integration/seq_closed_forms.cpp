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

TEST_F(SeqClosedFormsIntegrationTest, ARepeatedRootMultipliesItsPowerByPowersOfTheIndex) {
    // reduce request #45: n * 2^n
    EXPECT_EQ(closed("4 * #1 - 4 * #2", {0, 2}), std::vector<std::string>{"#0 * exp(2, #0)"});
    // the repeat's amplitude may be 0: 2^n
    EXPECT_EQ(closed("4 * #1 - 4 * #2", {1, 2}), std::vector<std::string>{"exp(2, #0)"});
    // a triple root: n^2 * 2^n
    EXPECT_EQ(closed("6 * #1 - 12 * #2 + 8 * #3", {0, 2, 16}), std::vector<std::string>{"#0^2 * exp(2, #0)"});
}

TEST_F(SeqClosedFormsIntegrationTest, IrrationalRootsOrFractionsStayARecurrence) {
    EXPECT_THAT(closed("#1 + #2", {0, 1}), IsEmpty());
    // a(n) = 2^n + (n^2 - 3n) / 2
    EXPECT_THAT(closed("#0 + 3 * #1 - 2 * #2 - 1", {1, 1}), IsEmpty());
}

TEST_F(SeqClosedFormsIntegrationTest, AnAffineStepOfOrderOneIsLeftToThePowerCloser) {
    EXPECT_THAT(closed("2 * #1 + 1", {0}), IsEmpty());
}


namespace {

// The laws of powers, the power closer with a hole as its ratio, and the atom unifier, on real expressions.
struct SeqPowerLawsIntegrationTest : SeqClosedFormsIntegrationTest {
    const expr* make(const std::string& name, const std::vector<const expr*>& args) {
        return kernel.exprs_.make_functor(kernel.symbols_.intern_symbol(name), args);
    }
    const expr* parse(const std::string& text) {
        return kernel.parser_.parse(text);
    }
    std::string print(const expr* e) {
        std::ostringstream os;
        kernel.printer_.print(os, e);
        return os.str();
    }
    template<typename Rule>
    std::vector<std::string> detected(Rule& rule, const expr* e) {
        out.clear();
        rule.detect(e, out);
        std::vector<std::string> texts;
        for (const rewrite& r : out)
            texts.push_back(print(r()));
        return texts;
    }
};

}

TEST_F(SeqPowerLawsIntegrationTest, PowersOfOneExponentMultiplyTheirBases) {
    const expr* n = parse("#0");
    EXPECT_EQ(detected(rules.power_merger_, make("mul", {make("exp", {parse("2"), n}), make("exp", {parse("3"), n})})),
              std::vector<std::string>{"exp(6, #0)"});
}

TEST_F(SeqPowerLawsIntegrationTest, ALiteralPowerOfAPowerRaisesItsBase) {
    // reduce request #48
    EXPECT_EQ(detected(rules.power_merger_, make("pow", {make("exp", {parse("2"), parse("#0")}), parse("2")})),
              std::vector<std::string>{"exp(4, #0)"});
}

TEST_F(SeqPowerLawsIntegrationTest, PowersOfOneBaseAddTheirExponents) {
    const expr* two = parse("2");
    const expr* product = make("mul", {make("exp", {two, parse("#0")}), make("exp", {two, parse("#1")})});
    EXPECT_EQ(detected(rules.power_merger_, product),
              std::vector<std::string>{"exp(2, #0 + #1)"});
}

TEST_F(SeqPowerLawsIntegrationTest, ARecurrenceWithAHoleAsItsRatioClosesToAPowerOfTheHole) {
    const expr* step = make("lam", {make("lam", {parse("?0 * #1")})});
    EXPECT_EQ(detected(rules.closer_, make("rec", {step, parse("3"), parse("#0")})),
              std::vector<std::string>{"3 * exp(?0, #0)"});
}

TEST_F(SeqPowerLawsIntegrationTest, AHoleInsideAnAtomIsBoundToMakeItAnotherAtom) {
    // reduce request #49: ?0 = 2
    const expr* n = parse("#0");
    EXPECT_EQ(detected(rules.atoms_, make("sub", {make("exp", {parse("2"), n}), make("exp", {parse("?0"), n})})),
              std::vector<std::string>{"0"});
    // an argument linear in the hole: ?0 = 0
    EXPECT_EQ(detected(rules.atoms_, make("sub", {make("prime", {parse("#0 + ?0")}), make("prime", {n})})),
              std::vector<std::string>{"0"});
}

TEST_F(SeqPowerLawsIntegrationTest, AtomsThatDoNotUnifyAreLeftAlone) {
    EXPECT_THAT(detected(rules.atoms_, make("sub", {make("exp", {parse("2"), parse("#0")}),
                                                    make("exp", {parse("?0"), parse("#0 + 1")})})),
                IsEmpty());
    const expr* other_head = make("sub", {make("exp", {parse("?0"), parse("#0")}), make("prime", {parse("#0")})});
    EXPECT_THAT(detected(rules.atoms_, other_head),
                IsEmpty());
}
