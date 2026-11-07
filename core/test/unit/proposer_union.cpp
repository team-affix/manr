#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/proposer_union.hpp"

using ::testing::_;
using ::testing::InSequence;

namespace {

struct MockDetector {
    MOCK_METHOD(void, detect, (const expr*, (std::vector<rewrite>&)));
};

struct ProposerUnionTest : ::testing::Test {
    MockDetector first;
    MockDetector second;
    proposer_union<MockDetector, MockDetector> detectors{first, second};
    expr_pool pool{};
    std::vector<rewrite> out{};
};

}

TEST_F(ProposerUnionTest, EveryDetectorIsAskedInOrderWithTheSameOutput) {
    const expr* e = pool.make_var(0);
    InSequence order;
    EXPECT_CALL(first, detect(e, _));
    EXPECT_CALL(second, detect(e, _));
    detectors.detect(e, out);
}
