#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/pw_branch_absorber.hpp"

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

namespace {

struct MockFindUncovered {
    MOCK_METHOD(std::vector<const expr*>, find_uncovered, ((const std::vector<const expr*>&), const expr*));
};

struct MockAssemble {
    MOCK_METHOD(const expr*, assemble, ((const std::vector<const expr*>&), const expr*));
};

struct PwBranchAbsorberTest : ::testing::Test {
    StrictMock<MockFindUncovered> find_uncovered;
    StrictMock<MockAssemble> assemble;
    pw_branch_absorber<MockFindUncovered, MockAssemble> absorber{
        find_uncovered, assemble, pw_symbols{.piecewise = 7, .branch = 8}};
    expr_pool pool{};
    const expr* branch0 = pool.make_functor(8, {pool.make_functor(1, {}), pool.make_functor(3, {})});
    const expr* branch1 = pool.make_functor(8, {pool.make_functor(2, {}), pool.make_functor(4, {})});
    const expr* otherwise = pool.make_functor(5, {});
    const expr* piecewise = pool.make_functor(7, {branch0, branch1, otherwise});
    std::vector<const expr*> out{};
};

}

TEST_F(PwBranchAbsorberTest, EveryCoveredBranchIsDroppedInOneProposal) {
    const expr* absorbed = pool.make_functor(7, {branch1, otherwise});
    EXPECT_CALL(find_uncovered, find_uncovered(std::vector<const expr*>{branch0, branch1}, otherwise))
        .WillOnce(Return(std::vector<const expr*>{branch1}));
    EXPECT_CALL(assemble, assemble(std::vector<const expr*>{branch1}, otherwise)).WillOnce(Return(absorbed));
    absorber.propose(piecewise, out);
    EXPECT_EQ(out, std::vector<const expr*>{absorbed});
}

TEST_F(PwBranchAbsorberTest, NothingIsProposedWhenNoBranchIsCovered) {
    EXPECT_CALL(find_uncovered, find_uncovered(_, otherwise))
        .WillOnce(Return(std::vector<const expr*>{branch0, branch1}));
    absorber.propose(piecewise, out);
    EXPECT_TRUE(out.empty());
}

TEST_F(PwBranchAbsorberTest, AnythingButAPiecewiseIsIgnored) {
    absorber.propose(otherwise, out);
    EXPECT_TRUE(out.empty());
}
