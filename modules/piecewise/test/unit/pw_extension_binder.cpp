#include <cstdint>
#include <map>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "rules/pw_extension_binder.hpp"

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::SetArgReferee;
using ::testing::StrictMock;

namespace {

struct MockProposeHypotheses {
    MOCK_METHOD(void, propose_hypotheses, ((const std::vector<pw_data_point>&), (std::vector<const expr*>&)));
};

struct MockFindUncovered {
    MOCK_METHOD(pw_coverage, find_uncovered, ((const std::vector<const expr*>&), const expr*));
};

struct MockAssemble {
    MOCK_METHOD(const expr*, assemble, ((const std::vector<const expr*>&), const expr*));
};

struct MockSubstituteHoles {
    MOCK_METHOD(const expr*, substitute_holes, (const expr*, (const std::map<uint32_t, const expr*>&)));
};

struct PwExtensionBinderTest : ::testing::Test {
    StrictMock<MockProposeHypotheses> propose_hypotheses;
    StrictMock<MockFindUncovered> find_uncovered;
    StrictMock<MockAssemble> assemble;
    ::testing::NiceMock<MockSubstituteHoles> substitute_holes;
    pw_extension_binder<MockProposeHypotheses, MockFindUncovered, MockAssemble, MockSubstituteHoles> binder{
        propose_hypotheses, find_uncovered, assemble, substitute_holes,
        pw_symbols{.piecewise = 7, .branch = 8, .region = 9}};
    expr_pool pool{};
    const expr* key0 = pool.make_functor(1, {});
    const expr* key1 = pool.make_functor(2, {});
    const expr* value0 = pool.make_functor(3, {});
    const expr* value1 = pool.make_functor(4, {});
    const expr* branch0 = pool.make_functor(8, {key0, value0});
    const expr* branch1 = pool.make_functor(8, {key1, value1});
    const expr* hypothesis = pool.make_functor(5, {});
    const expr* useless = pool.make_functor(6, {});
    std::vector<rewrite> out{};
    std::vector<const expr*> branches{branch0, branch1};
    std::vector<pw_data_point> points{{.input = key0, .output = value0}, {.input = key1, .output = value1}};

    PwExtensionBinderTest() {
        ON_CALL(substitute_holes, substitute_holes(_, _)).WillByDefault(ReturnArg<0>());
    }
};

}

TEST_F(PwExtensionBinderTest, EachUsefulHypothesisIsARewriteThatResolvesTheCatchAllAndDropsItsBranches) {
    const expr* bound = pool.make_functor(12, {});
    EXPECT_CALL(propose_hypotheses, propose_hypotheses(points, _))
        .WillOnce(SetArgReferee<1>(std::vector<const expr*>{hypothesis, useless}));
    EXPECT_CALL(find_uncovered, find_uncovered(branches, hypothesis))
        .WillOnce(Return(pw_coverage{.uncovered = {branch1}, .bindings = {}}));
    EXPECT_CALL(find_uncovered, find_uncovered(branches, useless))
        .WillOnce(Return(pw_coverage{.uncovered = branches, .bindings = {}}));
    EXPECT_CALL(assemble, assemble(std::vector<const expr*>{branch1}, hypothesis)).WillOnce(Return(bound));
    binder.detect(pool.make_functor(7, {branch0, branch1, pool.make_var(0)}), out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0](), bound);
}

TEST_F(PwExtensionBinderTest, AClosedCatchAllIsNotRebound) {
    binder.detect(pool.make_functor(7, {branch0, branch1, hypothesis}), out);
    EXPECT_TRUE(out.empty());
}

TEST_F(PwExtensionBinderTest, APiecewiseWithARegionBranchIsNotBound) {
    const expr* region = pool.make_functor(9, {key0, value0});
    binder.detect(pool.make_functor(7, {branch0, region, pool.make_var(0)}), out);
    EXPECT_TRUE(out.empty());
}

TEST_F(PwExtensionBinderTest, TheBindingOfTheDroppedBranchesIsCarriedIntoTheResult) {
    // a hole is one value everywhere: the kept branches get what the dropped ones bound it to
    const expr* bound = pool.make_functor(12, {});
    const expr* settled = pool.make_functor(13, {});
    const std::map<uint32_t, const expr*> bindings{{1, value0}};
    EXPECT_CALL(propose_hypotheses, propose_hypotheses(points, _))
        .WillOnce(SetArgReferee<1>(std::vector<const expr*>{hypothesis}));
    EXPECT_CALL(find_uncovered, find_uncovered(branches, hypothesis))
        .WillOnce(Return(pw_coverage{.uncovered = {branch1}, .bindings = bindings}));
    EXPECT_CALL(assemble, assemble(std::vector<const expr*>{branch1}, hypothesis)).WillOnce(Return(bound));
    EXPECT_CALL(substitute_holes, substitute_holes(bound, bindings)).WillOnce(Return(settled));
    binder.detect(pool.make_functor(7, {branch0, branch1, pool.make_var(0)}), out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0](), settled);
}
