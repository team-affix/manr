#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/detection_memo.hpp"
#include "infrastructure/expr_pool.hpp"

using ::testing::_;
using ::testing::StrictMock;

namespace {

struct MockDetector {
    MOCK_METHOD(void, detect, (const expr*, (std::vector<rewrite>&)));
};

struct MockPerform {
    MOCK_METHOD(const expr*, perform, ());
};

struct DetectionMemoTest : ::testing::Test {
    StrictMock<MockDetector> detector;
    detection_memo<StrictMock<MockDetector>> memo{detector};
    expr_pool pool{};
    const expr* first = pool.make_var(0);
    const expr* second = pool.make_var(1);
    const expr* result = pool.make_var(2);
    std::vector<rewrite> out{};
};

}

TEST_F(DetectionMemoTest, AnExpressionIsDetectedOnceAndItsRewritesAreHandedOutEachTime) {
    EXPECT_CALL(detector, detect(first, _)).WillOnce(([this](const expr*, std::vector<rewrite>& found) {
        found.push_back(ready(result));
        found.push_back(ready(second));
    }));
    memo.detect(first, out);
    memo.detect(first, out);
    ASSERT_EQ(out.size(), 4u);
    EXPECT_EQ(out[2](), result);
    EXPECT_EQ(out[3](), second);
}

TEST_F(DetectionMemoTest, DistinctExpressionsAreDetectedSeparately) {
    EXPECT_CALL(detector, detect(first, _)).WillOnce(([this](const expr*, std::vector<rewrite>& found) {
        found.push_back(ready(result));
    }));
    EXPECT_CALL(detector, detect(second, _));
    memo.detect(first, out);
    memo.detect(second, out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0](), result);
}

TEST_F(DetectionMemoTest, AnExpressionWithNoRewritesIsAlsoRemembered) {
    EXPECT_CALL(detector, detect(first, _));
    memo.detect(first, out);
    memo.detect(first, out);
    EXPECT_TRUE(out.empty());
}

TEST_F(DetectionMemoTest, ARewriteIsPerformedOnceHoweverOftenItIsHandedOut) {
    StrictMock<MockPerform> perform;
    EXPECT_CALL(detector, detect(first, _)).WillOnce(([&perform](const expr*, std::vector<rewrite>& found) {
        found.push_back([&perform]() { return perform.perform(); });
    }));
    EXPECT_CALL(perform, perform()).WillOnce(::testing::Return(result));
    memo.detect(first, out);
    memo.detect(first, out);
    EXPECT_EQ(out[0](), result);
    EXPECT_EQ(out[1](), result);
}
