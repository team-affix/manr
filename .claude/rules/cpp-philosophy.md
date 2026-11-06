---
description: Overarching code philosophy — simplicity, symmetry, organization
---

# Code Philosophy

- PREFER SIMPLER CODE above all. Reach for the least abstraction that works.
- DRY is a tool, not a goal. Sometimes de-duplicate; other times (e.g. tests)
  choose less abstraction and accept some repetition for clarity.
- Keep code as symmetrical as possible. Operations that come in pairs should
  mirror each other — e.g. `push_frame` acquires in one order and `pop_frame`
  releases in the exact reverse order.
- Keep things very organized: consistent ordering, naming, and file layout so
  related code reads the same way everywhere.

```cpp
void push_frame() {
    depth_tracker_.push();
    goal_exprs_.push_frame();
    mhu_.push_frame();
}

void pop_frame() {         // mirror image of push_frame
    mhu_.pop_frame();
    goal_exprs_.pop_frame();
    depth_tracker_.pop();
}
```
