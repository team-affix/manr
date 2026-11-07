#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/pw_assembler.hpp"

using ::testing::Return;
using ::testing::StrictMock;

namespace {

struct MockMakeFunctor {
    MOCK_METHOD(const expr*, make_functor, (uint32_t, (const std::vector<const expr*>&)));
};

struct PwAssemblerTest : ::testing::Test {
    StrictMock<MockMakeFunctor> make_functor;
    pw_assembler<MockMakeFunctor> assembler{make_functor, pw_symbols{.piecewise = 7, .branch = 8, .region = 9}};
    expr_pool pool{};
    const expr* branch = pool.make_functor(8, {pool.make_functor(1, {}), pool.make_functor(2, {})});
    const expr* otherwise = pool.make_var(0);
};

}

TEST_F(PwAssemblerTest, BranchesAndOtherwiseBecomeOnePiecewiseWithTheOtherwiseLast) {
    const expr* piecewise = pool.make_functor(7, {branch, otherwise});
    EXPECT_CALL(make_functor, make_functor(7, std::vector<const expr*>{branch, otherwise})).WillOnce(Return(piecewise));
    EXPECT_EQ(assembler.assemble({branch}, otherwise), piecewise);
}

TEST_F(PwAssemblerTest, APiecewiseWithNoBranchesIsItsOtherwise) {
    EXPECT_EQ(assembler.assemble({}, otherwise), otherwise);
}

TEST_F(PwAssemblerTest, APiecewiseOtherwiseIsSplicedIn) {
    const expr* inner_branch = pool.make_functor(9, {pool.make_functor(3, {}), pool.make_functor(4, {})});
    const expr* inner_otherwise = pool.make_functor(5, {});
    const expr* inner = pool.make_functor(7, {inner_branch, inner_otherwise});
    const expr* flat = pool.make_functor(7, {branch, inner_branch, inner_otherwise});
    EXPECT_CALL(make_functor, make_functor(7, std::vector<const expr*>{branch, inner_branch, inner_otherwise}))
        .WillOnce(Return(flat));
    EXPECT_EQ(assembler.assemble({branch}, inner), flat);
}
