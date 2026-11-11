#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "composition/list_fit_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"

using ::testing::Contains;

namespace {

// The laws of list programs on real expressions: map and filter fusion, the identity map, reverses cancelling, and
// holes in a map's body bound by the polynomial hole binder.
struct ListLawsIntegrationTest : ::testing::Test {
    list_fit_manifest manifest{{}, k_poly_default_limits, k_poly_fit_default_limits, 20000};
    list_fit_rules_manifest& rules = manifest.rule_set_;
    const list_symbols& symbols = rules.list_symbols_;
    const expr* xs = parse("#0");
    std::vector<rewrite> out{};

    const expr* parse(const std::string& text) {
        return manifest.kernel_.parser_.parse(text);
    }
    const expr* make(uint32_t id, const std::vector<const expr*>& args) {
        return manifest.kernel_.exprs_.make_functor(id, args);
    }
    const expr* lam(const std::string& body) {
        return make(symbols.lambda, {parse(body)});
    }
    std::string print(const expr* e) {
        std::ostringstream os;
        manifest.kernel_.printer_.print(os, e);
        return os.str();
    }
    std::vector<std::string> detected(const expr* e) {
        out.clear();
        rules.local_rules_.detect(e, out);
        std::vector<std::string> texts;
        for (const rewrite& r : out)
            texts.push_back(print(r()));
        return texts;
    }
    std::string reduced(const expr* e) {
        list_fit_manifest::search_t search{rules.rules_, manifest.kernel_.sizes_, e, 20000};
        while (search.rewrites() < 2000 && search.next()) {}
        return print(search.best());
    }
};

}

TEST_F(ListLawsIntegrationTest, AMapOverAMapIsFusedIntoOneBody) {
    const expr* e = make(symbols.map, {lam("#0 + 1"), make(symbols.map, {lam("2 * #0"), xs})});
    EXPECT_THAT(detected(e), Contains("map(lam(2 * #0 + 1), #0)"));
}

TEST_F(ListLawsIntegrationTest, FusionSubstitutesUnderTheBindersOfTheOuterBody) {
    // the outer body holds a fold whose step reads the element as #2: the inner body is shifted to meet it, and
    // the outer index #1 (the fold's list) stays
    const expr* step = make(symbols.lambda, {make(symbols.lambda, {parse("#0 + #2")})});
    const expr* outer = make(symbols.lambda, {make(symbols.fold, {step, parse("0"), parse("#1")})});
    const expr* e = make(symbols.map, {outer, make(symbols.map, {lam("#0 + 1"), parse("#1")})});
    EXPECT_THAT(detected(e), Contains("map(lam(fold(lam(lam(#0 + (#2 + 1))), 0, #1)), #1)"));
}

TEST_F(ListLawsIntegrationTest, ReversesCancelThroughMapsAndFilters) {
    const expr* e = make(symbols.reverse, {make(symbols.map, {lam("#0^2"), make(symbols.reverse, {xs})})});
    EXPECT_EQ(reduced(make(symbols.lambda, {e})), "lam(map(lam(#0^2), #0))");
}

TEST_F(ListLawsIntegrationTest, AFilterTwiceIsOneFilter) {
    const expr* positive = make(symbols.lambda, {make(rules.less_symbol_, {parse("0"), parse("#0")})});
    const expr* e = make(symbols.filter, {positive, make(symbols.filter, {positive, xs})});
    EXPECT_THAT(detected(e), Contains("filter(lam(lt(0, #0)), #0)"));
}

TEST_F(ListLawsIntegrationTest, AHoleInAFusedBodyIsBoundToMakeTheMapTheIdentity) {
    // reduce request #53: ?0 = 1
    const expr* e = make(symbols.map, {lam("#0 + ?0"), make(symbols.map, {lam("#0 - 1"), xs})});
    EXPECT_EQ(reduced(make(symbols.lambda, {e})), "lam(#0)");
}
