---
description: Rules for simplifying and cleaning up code — naming, helpers, indentation, symmetry, and type placement
---

# Code Simplification

## Name by action, not by precondition or mechanism

A function's name answers *what does calling this do*, not *when should this be called*.

```cpp
// BAD: names describe precondition or temporal position
sole_unvisited_leaf(...)   // "there is exactly one" — precondition
check_nand(...)            // "check" — describes checking, not acting
after_increment(...)       // "after X" — temporal position
ensure_generation(...)     // "ensure" — guard-style
arm_catchup(...)           // internal mechanism name

// GOOD: names describe the action
find_unvisited_leaf(...)   // traverses and finds
try_fire(...)              // attempts to fire
propagate_visit(...)       // propagates upward
refresh(...)               // resets to current epoch
visit_chosen_leaves(...)   // visits matching leaves
```

## Verb prefixes carry precise meaning

- `find_` — searches and returns what it found (may return null/empty)
- `try_` — attempts an action that may or may not have an effect; does not assert success
- `intern_` — hash-conses: inserts if absent, returns the canonical pointer either way
- `visit_` — traverses and records/counts; side-effectful walk
- `propagate_` — fans an event outward to dependents
- `refresh_` / `refresh` — brings a lazy field up to the current epoch
- `log_` / `log` — records an undo entry; paired with `undo_action`

Avoid verbs that describe position in time (`after_`, `before_`), guards (`ensure_`, `check_`), or preconditions (`sole_`, `existing_`).

## Extract a function only when justified

A helper is justified when:
1. It is called from **2 or more call sites**, OR
2. It is **recursive** (C++ cannot inline a recursive call), OR
3. It is a **complex subfeature** that warrants being tested in isolation — the prospective test counts as a second caller even though helpers are private and tests can't invoke them directly. If you can imagine a focused test for a piece of logic, that logic has earned its own name.

A single-call-site, non-recursive body that does not warrant its own test belongs inline in its caller — it is a sequence of steps, not a concept.

## Minimize the sum of indentation depth

Test the *opposite* condition and exit immediately. Never nest the happy path when you can invert and early-return/continue/break.

```cpp
// BAD
for (auto& x : xs) {
    if (condition(x)) {
        do_work(x);
    }
}

// GOOD
for (auto& x : xs) {
    if (!condition(x)) continue;
    do_work(x);
}
```

### No single-letter or cryptic variable names

Single-letter names (`n`, `i`, `k`, `t`) are only acceptable in genuine mathematical algorithms where the letter is the established notation. Everywhere else, use a 1–2 word descriptive name.

```cpp
// BAD
const size_t n = end - begin;
if (n == 1) ...

// BAD: "count" of what?
const size_t count = end - begin;

// GOOD: name says what is being counted
const size_t member_count = end - begin;
if (member_count == 1) ...
```

### Name non-obvious boolean conditions

Raw arithmetic or bit predicates used directly as `if` conditions are unreadable. Bind them to a named `const bool` first.

```cpp
// BAD
if ((n % 2) == 1) { ... }

// GOOD
const bool odd = (n % 2) == 1;
if (odd) { ... }
```

### Never put a `for` loop inside an `if` block

When a condition is loop-invariant, do not gate a whole `for` on it. Hoist the loop and branch inside with an early-continue.

```cpp
// BAD: two loops, one buried inside an if
if (condition) {
    for (auto& x : xs) { A(x); }
    return;
}
for (auto& x : xs) { B(x); }

// GOOD: one loop, branch inside
for (auto& x : xs) {
    if (condition) { A(x); continue; }
    B(x);
}
```

## Symmetry is a design constraint

Paired operations must be mirror images — same participants, exact reverse order.
If `push_frame` touches A then B then C, `pop_frame` must touch C then B then A.
Asymmetry is a signal that the design is wrong.

Two types that play the same role in the system must have the same shape: same public surface, same private helpers, same names for analogous operations.

## Types live at the right level

- **Value objects** (carry data, no behaviour) live in their own header with public fields. No behaviour, no private state.
- **Behaviour types** keep everything private except the minimal public API.
- Nested types are only acceptable when they are genuinely private implementation details of the enclosing type (e.g. `frame` inside a DBUCT stack type). Not merely because co-location is convenient.
- `friend` is never acceptable as a workaround for misplaced privacy.
