#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/pw_builder.hpp"

using ::testing::Return;

namespace {

struct MockMakeFunctor {
    MOCK_METHOD(const expr*, make_functor, (uint32_t, (const std::vector<const expr*>&)));
};

struct MockMakeVar {
    MOCK_METHOD(const expr*, make_var, (uint32_t));
};

struct MockAssemble {
    MOCK_METHOD(const expr*, assemble, ((const std::vector<const expr*>&), const expr*));
};

struct PwBuilderTest : ::testing::Test {
    MockMakeFunctor make_functor;
    MockMakeVar make_var;
    MockAssemble assemble;
    pw_builder<MockMakeFunctor, MockMakeVar, MockAssemble> builder{
        make_functor, make_var, assemble, pw_symbols{.piecewise = 7, .branch = 8, .region = 9}};
    expr_pool pool{};
    const expr* in0 = pool.make_functor(1, {});
    const expr* out0 = pool.make_functor(2, {});
    const expr* in1 = pool.make_functor(3, {});
    const expr* out1 = pool.make_functor(4, {});
};

}

TEST_F(PwBuilderTest, EveryPointBecomesABranchAndTheCatchAllIsTheExtensionTerm) {
    const expr* branch0 = pool.make_functor(8, {in0, out0});
    const expr* branch1 = pool.make_functor(8, {in1, out1});
    const expr* extension = pool.make_var(5);
    const expr* piecewise = pool.make_functor(12, {});
    EXPECT_CALL(make_functor, make_functor(8, std::vector<const expr*>{in0, out0})).WillOnce(Return(branch0));
    EXPECT_CALL(make_functor, make_functor(8, std::vector<const expr*>{in1, out1})).WillOnce(Return(branch1));
    EXPECT_CALL(make_var, make_var(5)).WillOnce(Return(extension));
    EXPECT_CALL(assemble, assemble(std::vector<const expr*>{branch0, branch1}, extension)).WillOnce(Return(piecewise));
    EXPECT_EQ(builder.build({{.input = in0, .output = out0}, {.input = in1, .output = out1}}, 5), piecewise);
}
