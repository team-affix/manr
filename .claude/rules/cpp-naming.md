---
description: Naming conventions for files, types, members, and template parameters
---

# Naming Conventions

- Files, types, and functions are `snake_case`. One outermost type per file; the
  file name matches the type (`dbuct_goal_exprs` → `dbuct_goal_exprs.hpp`).
- Private and injected member variables end in a trailing `_` (`exprs_`,
  `frame_stack_`, `depth_tracker_`). This applies universally, including injected
  collaborator references. Some older templates omit it — treat that as debt to be
  cleaned up, not a precedent.
- Template parameters for injected capabilities are prefixed `I`
  (`IGoalExprs`, `IPushFrame`, `IGetGoalCandidateRuleIds`). Out-of-line template
  definitions may use abbreviated aliases of those params (`IFDT`, `IGE`, `ICDCL`).
- Compile-time constants are prefixed `k_` (`k_functor_names`).
- Local type aliases inside a behavior type describe the container's role
  (`map_t`, `set_t`, `vec_t`), declared `private`.

```cpp
template<typename IGoalExprs, typename IMhu>
struct frame_hub {
    frame_hub(IGoalExprs& goal_exprs, IMhu& mhu)
        : goal_exprs_(goal_exprs), mhu_(mhu) {}
private:
    using map_t = std::unordered_map<const goal_lineage*, framed_expr>;

    IGoalExprs& goal_exprs_;   // injected ref: trailing underscore
    IMhu&       mhu_;
    map_t       exprs_;
};
```
