#include <cstddef>
#include <optional>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/seq_unfolder.hpp"

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::Return;

namespace {

struct MockFindBody {
    MOCK_METHOD(std::optional<const expr*>, find_body, (const expr*, size_t), (const));
};

struct SeqUnfolderTest : ::testing::Test {
    expr_pool pool{};
    ::testing::NiceMock<MockFindBody> find_body;
    seq_symbols symbols{.recurrence = 1, .power = 2, .tabulation = 3, .unfolding = 4, .lambda = 5, .index = 6,
                        .previous = {7}};
    seq_unfolder<MockFindBody, expr_pool> unfolder{find_body, pool, symbols};
    const expr* step = pool.make_functor(20, {});
    const expr* zero = pool.make_functor(30, {});
    const expr* one = pool.make_functor(31, {});
    const expr* index = pool.make_functor(6, {});
    const expr* function = pool.make_functor(40, {});
    std::vector<rewrite> out{};
};

}

TEST_F(SeqUnfolderTest, TheTabulatedRecurrenceAtTheIndexUnfolds) {
    ON_CALL(find_body, find_body(function, 1u))
        .WillByDefault(Return(pool.make_functor(1, {step, zero, one, index})));
    unfolder.detect(pool.make_functor(3, {function}), out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0](), pool.make_functor(4, {step, zero, one}));
}

TEST_F(SeqUnfolderTest, ARecurrenceAtAnotherArgumentIsNotUnfolded) {
    ON_CALL(find_body, find_body(function, 1u)).WillByDefault(Return(pool.make_functor(1, {step, zero, one})));
    unfolder.detect(pool.make_functor(3, {function}), out);
    EXPECT_THAT(out, IsEmpty());
}

TEST_F(SeqUnfolderTest, ATabulationOfAnythingElseStays) {
    ON_CALL(find_body, find_body(function, 1u)).WillByDefault(Return(pool.make_functor(2, {one, index})));
    unfolder.detect(pool.make_functor(3, {function}), out);
    EXPECT_THAT(out, IsEmpty());
}
