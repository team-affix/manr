---
description: Google Mock conventions for test doubles
---

# Test Mocks (Google Mock)

All collaborators in tests are GMock mocks. Mocks are `struct`s (match repo
style) that expose `MOCK_METHOD`s and are supplied as the SUT's template
parameters — the code is duck-typed on templates, so mocks do NOT inherit an
interface and do NOT use `(override)`.

Do NOT write hand-rolled `fake_*`, `recording_*`, or `noop_*` doubles, and do
NOT add mutable recording fields (`last_gl`, `std::set removed`) when
`EXPECT_CALL` can assert the same contract.

```cpp
struct MockGetGoalCandidateRuleIds {
    MOCK_METHOD(MockRuleIdSet&, get, (const goal_lineage*));
};
using test_conflict_detector_t = conflict_detector<MockGetGoalCandidateRuleIds>;
```

Specify behavior with `EXPECT_CALL` (strict) or `ON_CALL` (default stubs), and
control returns with `WillOnce` / `WillRepeatedly` / `Return` / `ReturnRef`:

```cpp
EXPECT_CALL(rules, size()).WillOnce(Return(0));
EXPECT_CALL(ggcr, get(&gl)).WillOnce(ReturnRef(rules));
```

## NiceMock vs StrictMock

| Wrapper | Use when |
|---------|----------|
| `NiceMock<MockT>` | Most tests: ignore uninteresting calls |
| `StrictMock<MockT>` | Prove an exact sequence or count of calls |
| plain `MockT` | Same as strict — prefer `NiceMock` unless asserting specific calls |

## Overloaded methods

Give each a distinct mock name and forward (we don't overload):

```cpp
MOCK_METHOD(void, pin_goal, (const goal_lineage*), ());
void pin(const goal_lineage* gl) { pin_goal(gl); }
```

## `unique_ptr` parameters

Use double parentheses:

```cpp
MOCK_METHOD(void, log, ((std::unique_ptr<i_backtrackable>)), ());
```
