---
description: Struct and member design conventions for C++ types
---

# Type & Member Design

- Use `struct`, never `class`.
- One outermost type per file (source code). Nested subtypes of that type are
  fine; do not define unrelated types in the same file. The ONLY exception is a
  genuine circular dependency between two types that cannot otherwise be broken —
  then they may share a file. Test code is exempt from this rule.
- No public member variables, EXCEPT value objects (plain data + `operator<=>`).
  Value objects may only have member functions that assist construction.
- No default member initializers. Do NOT set member values in the class/struct
  declaration. Initialize members in the constructor's member initializer list
  when possible, or, as a last-resort fallback, in the constructor body.
- Keep the public surface minimal: only the functions callers actually invoke are
  `public` (alongside the constructor and destructor). Everything else — helpers,
  internal steps, undo/log routines — is `private`. Default to `private` and
  promote a function to `public` only when it is part of the type's API.
- Prefer a single constructor and single destructor. Do not overload them.
- No function overloading. Two functions with distinct purposes get distinct
  names (e.g. `reduce_nullary` / `reduce_unary`, not two `reduce`).
- No default parameters, ever. The caller must specify every argument.
- No optional arguments and no optional template arguments.
- Remove `explicit` on constructors; we don't keep it (it's usually unnecessary).

## Value objects

A type is a **value object** when it is meant to be regularly constructed, copied
and passed around by value, and destroyed, and it exists to carry data rather than
behavior. Value objects are the exception to the member rules above: their data
members are `public`, and they define `operator<=>`. They may only have member
functions that assist their own construction — no injected collaborators, no
lifecycle/orchestration behavior. Behavior types (private state, injected
collaborators, methods that do work) are NOT value objects.

```cpp
// value object: public data is allowed
struct goal_lineage {
    const resolution_lineage* parent;
    subgoal_id idx;
    auto operator<=>(const goal_lineage&) const = default;
};

// behavior type: private state, private helpers, distinct names
struct dbuct_unit_goals {
    void push(const goal_lineage* gl);
    std::optional<const goal_lineage*> pop();
private:
    void log(unit_goals_action action);
    void undo_action(const unit_goals_action& action);
    vec_t queue_;
};
```

## Member initialization

```cpp
// BAD: default member initializers in the declaration
struct dbuct_frame_bump_allocator {
    uint32_t next_frame_offset_ = 0;
    std::stack<frame> frame_stack_{std::deque<frame>{frame{}}};
};

// GOOD: initialize in the constructor's member initializer list
struct dbuct_frame_bump_allocator {
    dbuct_frame_bump_allocator();
private:
    uint32_t next_frame_offset_;
    std::stack<frame> frame_stack_;
};

dbuct_frame_bump_allocator::dbuct_frame_bump_allocator()
    : next_frame_offset_(0)
    , frame_stack_(std::deque<frame>{frame{}}) {}
```
