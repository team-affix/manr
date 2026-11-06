---
description: TEST_F fixture conventions for C++ tests
---

# Test Fixtures (`TEST_F`)

Use a `struct … : public ::testing::Test` fixture per component (or integration
slice). Factor shared resources into the fixture — do not reconstruct the same
objects in every test body. GTest builds a fresh fixture per `TEST_F`, so tests
stay isolated without local re-declaration.

Put on the fixture:

- The SUT (concrete type)
- Mocks for collaborators
- Shared inputs reused across cases (`expr`, `rule`, lineages, maps, …)

```cpp
struct SolutionDetectorTest : public ::testing::Test {
    MockActiveGoals ag;
    solution_detector detector{ag};
};

TEST_F(SolutionDetectorTest, EmptyActiveGoalsMeansSolution) {
    EXPECT_CALL(ag, empty()).WillOnce(Return(true));
    EXPECT_TRUE(detector.detect());
}
```

When the SUT must vary per case, keep stable deps on the fixture and add a small
helper (e.g. `make_visitor(const goal_lineage*)`) instead of duplicating the
full constructor list per test.

Prefer `TEST_F` over bare `TEST` when a fixture is natural.

## Local-on-purpose exceptions

- A second SUT instance when the test is about independence.
- `StrictMock` + a dedicated SUT when one case needs a tight call-count contract
  unlike its siblings.
