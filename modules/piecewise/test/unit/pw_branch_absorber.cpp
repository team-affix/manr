#include <cstdint>
#include <map>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "rules/pw_branch_absorber.hpp"

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::StrictMock;

namespace {

struct MockFindUncovered {
    MOCK_METHOD(pw_coverage, find_uncovered, ((const std::vector<const expr*>&), const expr*));
};

struct MockAssemble {
    MOCK_METHOD(const expr*, assemble, ((const std::vector<const expr*>&), const expr*));
};

struct MockSubstituteHoles {
    MOCK_METHOD(const expr*, substitute_holes, (const expr*, (const std::map<uint32_t, const expr*>&)));
};

struct PwBranchAbsorberTest : ::testing::Test {
    StrictMock<MockFindUncovered> find_uncovered;
    StrictMock<MockAssemble> assemble;
    ::testing::NiceMock<MockSubstituteHoles> substitute_holes;
    pw_branch_absorber<MockFindUncovered, MockAssemble, MockSubstituteHoles> absorber{
        find_uncovered, assemble, substitute_holes, pw_symbols{.piecewise = 7, .branch = 8, .region = 9}};
    expr_pool pool{};
    const expr* key0 = pool.make_functor(1, {});
    const expr* branch0 = pool.make_functor(8, {key0, pool.make_functor(3, {})});
    const expr* branch1 = pool.make_functor(8, {pool.make_functor(2, {}), pool.make_functor(4, {})});
    const expr* region = pool.make_functor(9, {pool.make_functor(10, {}), pool.make_functor(11, {})});
    const expr* otherwise = pool.make_functor(5, {});
    const expr* piecewise = pool.make_functor(7, {branch0, branch1, otherwise});
    std::vector<rewrite> out{};

    PwBranchAbsorberTest() {
        ON_CALL(substitute_holes, substitute_holes(_, _)).WillByDefault(ReturnArg<0>());
    }
};

}

TEST_F(PwBranchAbsorberTest, EveryCoveredBranchIsDroppedInOneRewrite) {
    const expr* absorbed = pool.make_functor(7, {branch1, otherwise});
    EXPECT_CALL(assemble, assemble(std::vector<const expr*>{}, otherwise)).WillOnce(Return(otherwise));
    EXPECT_CALL(find_uncovered, find_uncovered(std::vector<const expr*>{branch0, branch1}, otherwise))
        
        .WillOnce(Return(pw_coverage{.uncovered = {branch1}, .bindings = {}}));
    EXPECT_CALL(assemble, assemble(std::vector<const expr*>{branch1}, otherwise)).WillOnce(Return(absorbed));
    absorber.detect(piecewise, out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0](), absorbed);
}

TEST_F(PwBranchAbsorberTest, NothingIsDetectedWhenNoBranchIsCovered) {
    EXPECT_CALL(assemble, assemble(std::vector<const expr*>{}, otherwise)).WillOnce(Return(otherwise));
    EXPECT_CALL(find_uncovered, find_uncovered(_, otherwise))
        .WillOnce(Return(pw_coverage{.uncovered = {branch0, branch1}, .bindings = {}}));
    absorber.detect(piecewise, out);
    EXPECT_TRUE(out.empty());
}

TEST_F(PwBranchAbsorberTest, ACaseIsCheckedAgainstTheRegionsAndTheOtherwiseAndRegionsAreKept) {
    // the tail the case is checked against is also what is left once it is dropped
    const expr* tail = pool.make_functor(7, {region, otherwise});
    EXPECT_CALL(assemble, assemble(std::vector<const expr*>{region}, otherwise)).Times(2).WillRepeatedly(Return(tail));
    EXPECT_CALL(find_uncovered, find_uncovered(std::vector<const expr*>{branch0}, tail))
        .WillOnce(Return(pw_coverage{.uncovered = {}, .bindings = {}}));
    absorber.detect(pool.make_functor(7, {branch0, region, otherwise}), out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0](), tail);
}

TEST_F(PwBranchAbsorberTest, ACaseWhoseKeyALaterCaseRepeatsIsKept) {
    const expr* repeat = pool.make_functor(8, {key0, pool.make_functor(6, {})});
    EXPECT_CALL(assemble, assemble(std::vector<const expr*>{}, otherwise)).WillOnce(Return(otherwise));
    EXPECT_CALL(find_uncovered, find_uncovered(std::vector<const expr*>{repeat}, otherwise))
        .WillOnce(Return(pw_coverage{.uncovered = {repeat}, .bindings = {}}));
    absorber.detect(pool.make_functor(7, {branch0, repeat, otherwise}), out);
    EXPECT_TRUE(out.empty());
}

TEST_F(PwBranchAbsorberTest, AnythingButAPiecewiseIsIgnored) {
    absorber.detect(otherwise, out);
    EXPECT_TRUE(out.empty());
}

TEST_F(PwBranchAbsorberTest, TheBindingOfTheDroppedCasesIsCarriedIntoTheKeptOnes) {
    const expr* absorbed = pool.make_functor(7, {branch1, otherwise});
    const expr* settled = pool.make_functor(13, {});
    const std::map<uint32_t, const expr*> bindings{{1, otherwise}};
    EXPECT_CALL(assemble, assemble(std::vector<const expr*>{}, otherwise)).WillOnce(Return(otherwise));
    EXPECT_CALL(find_uncovered, find_uncovered(std::vector<const expr*>{branch0, branch1}, otherwise))
        .WillOnce(Return(pw_coverage{.uncovered = {branch1}, .bindings = bindings}));
    EXPECT_CALL(assemble, assemble(std::vector<const expr*>{branch1}, otherwise)).WillOnce(Return(absorbed));
    EXPECT_CALL(substitute_holes, substitute_holes(absorbed, bindings)).WillOnce(Return(settled));
    absorber.detect(piecewise, out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0](), settled);
}
