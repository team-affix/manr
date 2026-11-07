# manr

A domain-specific symbolic expression reduction engine, aimed at AML (abstractive machine learning): given data, find
the smallest symbolic expression that fits it.

A data set becomes a piecewise function with an open catch-all:

```
pw(case(0, 1), case(1, 3), case(2, 5), case(3, 7), case(10, 99), ?0)
```

`?0` is an *extension term*, a free variable that can be resolved to any expression. Rules then rewrite toward
the smallest expression. High-level detectors propose distant results in one step, so search does not have to find
them by accident. Here a linear-fit detector resolves `?0` to a line and drops every branch the line reproduces:

```
pw(case(10, 99), add(mul(2, x), 1))
```

The long-term goal is the same recipe in every domain: arithmetic, algebra, calculus, lists, graphs. Inputs and
labels can be any expressions of the domain, and free variables leave room for generality. See `docs/vision.md`.

## Build

Requires g++ with C++20 and the submodules (googletest, team-affix/mcts, and Boost.Multiprecision with Boost.Config
for arbitrary-precision integers, all header-only except mcts):

```bash
git submodule update --init --recursive
```

Then:

```bash
make run_tests
```

## Using it

```cpp
#include "infrastructure/arith_fit_runtime.hpp"

arith_fit_runtime fit{{{.input = 0, .output = 1}, {.input = 1, .output = 3}, {.input = 2, .output = 5}}, 1000};
while (fit.next()) {}
fit.best();        // "add(mul(2, x), 1)"
fit.derivation();  // every expression from the data to the best
```

To add a domain, write a module under `modules/` with rules that implement
`void detect(const expr*, std::vector<rewrite>&)`, and compose them in a new manifest. See `CLAUDE.md` for
the layout and conventions.

Polynomial data, over any number of inputs:

```cpp
#include "infrastructure/poly_fit_runtime.hpp"

poly_fit_runtime fit{{"x"}, samples, k_poly_default_limits, k_poly_fit_default_limits, 20000};
while (fit.rewrites() < 3000 && fit.next()) {}
fit.best();        // e.g. "(2 * x - 1)^3", "pw(case(point(3), 17), x^2 + 1)" with an outlier,
                   // or "pw(when(lt(x, 0), x^2), 3 * x + 1)" for data from two polynomials
```

`arith_fit_mcts_runtime` runs the same rules under Monte Carlo tree search, for reductions that only pay off
several rewrites later. See `docs/search.md`.
