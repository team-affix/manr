# MANR — project memory for Claude Code

## What this is

MANR is a domain-specific symbolic expression reduction engine in C++20, the most direct attempt at AML
(abstractive machine learning): find the minimal-description-length symbolic expression that fits a data set.

- **Extension algebra.** Any algebra plus *extension terms*: free variables (`expr::var`, printed `?n`) that may be
  resolved to any expression content.
- **Data as a piecewise.** A data set is turned mechanically into `pw(case(in, out), ..., ?0)`: one branch per point,
  and an extension term as the catch-all for every unseen input. Reduction then tries to make the piecewise disappear.
- **Objective.** Smallest node count (`expr_size`) for the fixed functor set.
- **Rules come in two kinds**, both behind the same `propose(e, out)` capability:
  low-level local rewrites (e.g. `arith_simplifier`), and high-level *detectors* that read the expression or data
  and propose a far-away result in one step (e.g. `arith_linear_hypotheses` feeding `pw_extension_binder`).
  Detectors are the point of the project: search should not have to stumble into a reducible form.
- It is a library. Users write rules in C++ and compose them in a manifest. There is no CLI or parser yet.

## Layout

```
core/hpp/{infrastructure,value_objects}       domain-free: expr, pool, symbols, size, printer, rule plumbing, search
core/cpp/...                                  .cpp for non-templated types
core/test/{unit,integration}, main.cpp
modules/<name>/{hpp,cpp,test}/...             one directory per rule module; prefix every file with the module's
                                              domain prefix, since every module's hpp/ is on the include path
  piecewise/  (pw_)                           the extension algebra over data sets, generic over the domain
  arith/      (arith_)                        integer arithmetic: literals, evaluator, simplifier
  arith_fit/  (arith_)                        composition: linear-fit detector, manifest, runtime
  poly/       (poly_)                         polynomial algebra over Z: normal form oracle, rules, detectors
  poly_reduce/ (poly_)                        composition: kernel, rule sets, four solver manifests
bench/                                        release benchmarks (`make poly_bench`), not tests
modules/test_main.cpp                         GTest entry for the modules binary
googletest/ mcts/                             git submodules (github), not ours to edit; mcts/core/cpp is built into core
multiprecision/ boost_config/                 git submodules: header-only Boost.Multiprecision, standalone (needs Boost.Config)
debug_assert.hpp                              DEBUG_ASSERT(cond): throws std::logic_error under -DDEBUG
docs/                                         design notes
```

A module depends on core and on other modules' headers; core never depends on a module.

## Architecture

```
<name>_runtime            non-templated facade: next() pull loop, best(), derivation(). Owns one manifest_.
  <name>_manifest         composition root: names component types, owns every component by value
    mcts_search           probing simulations (UCT in-tree, random past it), rewarded by smallest cost reached
    | best_first_search   or: bounded cost-ordered frontier, interned dedup
      subterm_proposer    lifts a local rule set to every position of the expression
        proposer_union    fans propose() out to the rules of several modules
          rules           pw_extension_binder, pw_branch_absorber, arith_simplifier, ...
```

- Expressions are hash-consed by `expr_pool`: pointer equality is structural equality. Search dedup, the
  derivation map and rules all rely on this.
- A proposer appends whole candidate expressions to `out`; it never mutates. Local proposers only look at the node
  they are handed; `subterm_proposer` rebuilds the path to the root.
- Domains plug into the piecewise module through capabilities: `IEvaluateAt` (value of an expression at an input,
  as an interned expression, or nullopt) and `IProposeHypotheses` (candidate resolutions of an extension term given
  the data points).
- **Every rule and detector follows `docs/rule-design.md`**: exit on the first failed requirement, run cheap
  necessary-condition filters (cached signatures, modular arithmetic) before any exact or big-integer work, and let
  filters only reject, never accept. Read it before writing or changing a rule.
- Integers are `integer` (`core/hpp/value_objects/integer.hpp`, Boost's `cpp_int`): arbitrary precision, so literal
  and coefficient arithmetic is exact and never overflows. Don't use built-in integer types for literal values.
- Polynomial algebra is the first full domain. Read `docs/poly.md` for its rules, detectors and benchmark.
- The search strategy is a manifest choice: `arith_fit_manifest` (best-first) and `arith_fit_mcts_manifest` share
  every rule and differ only in search. Read `docs/search.md` before touching either search.

## Build & test

- `make core_debug_fast && ./build/core_debug_fast`, `make modules_debug_fast && ./build/modules_debug_fast`.
  `make run_tests` builds and runs both. `--gtest_filter=...` narrows a run.
- The makefile `find`s every `.cpp` under `core/` and `modules/*/{cpp,test}`, and every `modules/*/hpp` is on the
  include path. **A new file or module is picked up automatically; don't register it in the makefile.**

## Code style

Carried over from Atlas unchanged. The rules live in `.claude/rules/*.md` and load automatically.
Core of it: simplest thing that works. `struct` only. Collaborators are injected by reference as template params
named per invoked capability. No lambdas or `std::function` (tests excepted), no overloads, no default args, no
default member initializers. Paired operations are mirror images.

Conventions from Atlas that MANR keeps:
- Out-of-line template definitions use a `#define X_TEMPLATE` / `#define X` macro pair, `#undef`'d at the end.
- Manifests name component types as nested structs inheriting the instantiation
  (`struct search_t : best_first_search<...> { using best_first_search<...>::best_first_search; };`), and declare
  members (trailing `_`) in dependency order.
- Comma-first constructor init lists. `std::optional` read with `.has_value()` / `.value()`. Locals `const`.
- Value objects use `auto operator<=>(const T&) const = default;` and are built with designated initializers.
- Tests: mocks and the `<Component>Test` fixture in an anonymous namespace; behavior-sentence test names.

MANR-specific:
- `proposer_union` / `pw_hypotheses_union` are variadic: the indexed-capability rule (`IEater0, IEater1`) for any arity.
- Rules that only make sense with real interning (literal identity, evaluation) are tested as integration tests.

## Working agreements

- New public behavior needs a unit test (one real SUT, everything else mocked), or an integration test when
  correctness depends on real interning.
- When asked only to write tests, don't touch non-test source. Never weaken a test to make it pass.
- Be terse. Commit messages short, lowercase, past tense. Branches `feature/`, `enhancement/`, `bug-fix/`.
