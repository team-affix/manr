---
description: Ban std::function and lambdas except a genuinely generic std::visit
---

# No std::function / Lambdas

Never use `std::function` or lambdas when it can be avoided. Prefer named
functions, template parameters, and injected collaborators.

## Only allowed exception

A `std::visit()` whose visitor is ACTUALLY GENERIC — it treats every
alternative uniformly and does not need to know the concrete argument type.

```cpp
// OK: visitor is generic, same body for every alternative
std::visit([](const auto& node) { node.render(out); }, variant_);
```

Forbidden: a `std::visit()` whose visitor immediately re-splits on the type
(via `if constexpr`, `std::get_if`, overload sets, type traits, etc.). If you
need per-type behavior, that invalidates the visitor use case — dispatch some
other way instead.

```cpp
// BAD: "generic" visitor that just branches on the type again
std::visit([](const auto& n) {
    if constexpr (std::is_same_v<decltype(n), const Leaf&>) { ... }
    else { ... }
}, variant_);
```
