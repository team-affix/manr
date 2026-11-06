---
description: Unit vs integration testing — definitions, rules, and how to choose
---

# Testing: Unit vs Integration

Uses Google Test + Google Mock. For timely local dev prefer the `_fast` debug
variant (e.g. `make core_debug_fast`).

## Layout

- `<package>/test/unit/` — unit tests (one real component per file)
- `<package>/test/integration/` — several real components wired together
- `<package>/test/main.cpp` — GTest entry point for that package's binary

## Unit tests

Verify ONE class/function (the SUT). The SUT is the only real production type;
every collaborator is a GMock mock passed as a template parameter.

1. Exactly one real implementation (the SUT).
2. Mock every dependency with GMock.
3. Do not wire multiple real infrastructure components — if you need two real
   types together, it belongs in integration.
4. Prefer observable outcomes; use `EXPECT_CALL` for delegation contracts.
   Avoid `Times(0)` on unrelated methods unless the test is about "must not call".
5. Call the concrete SUT directly (`detector.detect()`), not via an alias.

Stack value types (`expr`, `rule`, lineages) are inputs, not collaborators.

## Integration tests

Check behavior that only appears when TWO OR MORE real components work together.

1. Use real implementations for each type in the slice under test.
2. Still mock types OUTSIDE the slice with GMock.
3. Assert end-state / cross-component invariants.
4. Do not duplicate unit coverage.

## Choosing

| Question | Unit | Integration |
|----------|------|-------------|
| Can collaborators be fully specified with `EXPECT_CALL`? | Yes | — |
| Correctness depends on real interning / undo / bind chaining? | — | Yes |
| One class's logic is the focus? | Yes | — |
| A pipeline or factory stack is the focus? | — | Yes |

When in doubt: unit test with mocks first; add integration when mocks would
have to reimplement another component to be meaningful.

## Naming

- Unit: `<package>/test/unit/<area>/<component>.cpp` — `ComponentTest`.
- Integration: `<package>/test/integration/<component>.cpp` — `*IntegrationTest`.
- Mock types: `Mock` + capability name (`MockBindMap`, `MockTrail`).
