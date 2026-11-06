---
description: Header/implementation split for templated vs non-templated types
---

# Headers, Templates & Implementation Hiding

Do not expose implementation unless necessary.

## Include guards & file layout

- Use `#ifndef` / `#define` include guards, never `#pragma once`. The guard macro
  is the file name in `UPPER_SNAKE_CASE` with the extension (e.g.
  `dbuct_decision_memory.hpp` → `DBUCT_DECISION_MEMORY_HPP`).
- Include order: standard-library headers (`#include <…>`) first, then project
  headers (`"value_objects/…"`, `"infrastructure/…"`), with `"debug_assert.hpp"`
  last. A `.cpp` includes its own matching header first.
- Every file ends with a single empty newline after the closing `#endif`.

```cpp
#ifndef DBUCT_DECISION_MEMORY_HPP
#define DBUCT_DECISION_MEMORY_HPP

#include <stack>
#include <unordered_set>
#include "value_objects/lineage.hpp"
#include "debug_assert.hpp"

struct dbuct_decision_memory { /* … */ };

#endif
⏎
```

## Non-templated types

Member functions are declared in the `.hpp` and defined in an accompanying
`.cpp`. Implementations must NOT be visible in the header.

```cpp
// goal_candidate_rules.hpp  — declarations only
struct goal_candidate_rules {
    goal_candidate_rules(ra_rule_id_set_factory&);
    ra_rule_id_set& get(const goal_lineage*);
private:
    ra_rule_id_set_factory& factory_;
    std::unordered_map<const goal_lineage*, ra_rule_id_set> by_goal_;
};
```

```cpp
// goal_candidate_rules.cpp  — definitions live here
ra_rule_id_set& goal_candidate_rules::get(const goal_lineage* gl) {
    return by_goal_.at(gl);
}
```

## Templated types

Everything stays in the header, but do NOT inline definitions at the
declaration site. Declare each member with a trailing `;` inside the struct,
then define them AFTER the struct closes (same header).

```cpp
template<typename NodeId>
struct dbuct_series_reduced_tree {
    bool insert(NodeId node);          // declared only
    bool link(NodeId parent, child_set_t children);
private:
    void try_reduce(NodeId node);
};

template<typename NodeId>
bool dbuct_series_reduced_tree<NodeId>::insert(NodeId node) {  // defined after
    ...
}
```
