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

// Open-ended lists, `cons(a0, ..., cons(a(k-1), ?0))`: read as the infinite list of a sequence, then fitted.
struct SeqOpenListsIntegrationTest : ::testing::Test {
    seq_fit_manifest manifest{{}, k_poly_default_limits, k_poly_fit_default_limits, 1};
    seq_fit_rules_manifest& rules = manifest.rule_set_;
    poly_kernel_manifest& kernel = manifest.kernel_;
    std::vector<rewrite> out{};

    const expr* open_list(const std::vector<int>& terms) {
        const uint32_t cons = kernel.symbols_.intern_symbol("cons");
        const expr* list = kernel.exprs_.make_var(0);
        for (auto term = terms.rbegin(); term != terms.rend(); ++term)
            list = kernel.exprs_.make_functor(cons, {kernel.literals_.make_literal(*term), list});
        return list;
    }
    std::string print(const expr* e) {
        std::ostringstream os;
        kernel.printer_.print(os, e);
        return os.str();
    }
    std::string reduced(const expr* start) {
        seq_fit_manifest::search_t search{rules.rules_, kernel.sizes_, start, 20000};
        while (search.rewrites() < 3000 && search.next()) {}
        return print(search.best());
    }
};

}

TEST_F(SeqOpenListsIntegrationTest, AnOpenListIsReadAsTheInfiniteListOfItsTermsData) {
    rules.open_lists_.detect(open_list({0, 1, 4}), out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(print(out[0]()), "tabulate(pw(case(point(0), 0), case(point(1), 1), case(point(2), 4), ?0))");
}

TEST_F(SeqOpenListsIntegrationTest, AClosedListOrOneWithAnUnknownTermIsNotRead) {
    const uint32_t cons = kernel.symbols_.intern_symbol("cons");
    const expr* closed = kernel.exprs_.make_functor(
        cons, {kernel.literals_.make_literal(1), kernel.exprs_.make_functor(kernel.symbols_.intern_symbol("nil"), {})});
    rules.open_lists_.detect(closed, out);
    const expr* unknown = kernel.exprs_.make_functor(cons, {kernel.exprs_.make_var(1), open_list({1, 2, 3})});
    rules.open_lists_.detect(unknown, out);
    EXPECT_THAT(out, IsEmpty());
}

TEST_F(SeqOpenListsIntegrationTest, TheOpenFibonacciListUnfoldsItsRecurrence) {
    // reduce request #16
    EXPECT_EQ(reduced(open_list({0, 1, 1, 2, 3, 5, 8, 13})), "unfold(lams(3, #1 + #2), 0, 1)");
}

TEST_F(SeqOpenListsIntegrationTest, AnOpenListOfAClosedFormTabulatesIt) {
    EXPECT_EQ(reduced(open_list({1, 2, 4, 8, 16, 32})), "tabulate(lam(exp(2, #0)))");
    EXPECT_EQ(reduced(open_list({0, 1, 4, 9, 16, 25})), "tabulate(lam(#0^2))");
}

TEST_F(SeqOpenListsIntegrationTest, AnOpenListOfAStepUsingTheIndexUnfolds) {
    // factorials: a(i) = i * a(i - 1)
    const std::string factorials = reduced(open_list({1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880}));
    EXPECT_EQ(factorials.rfind("unfold(", 0), 0u) << factorials;
}
