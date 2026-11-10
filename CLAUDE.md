# MANR — project memory for Claude Code

## What this is

MANR is a domain-specific symbolic expression reduction engine in C++20, the most direct attempt at AML
(abstractive machine learning): find the minimal-description-length symbolic expression that fits a data set.

- **Extension algebra.** Any algebra plus *extension terms*: free variables (`expr::var`, printed `?n`) that may be
  resolved to any expression content. They are holes at the **syntax level**, not opaque symbols: reduction may bind
  a `?n` to any expression, and binds it to whatever makes the whole smallest (`(?0 + 1)^2 - ?0^2` reduces to `1`
  with `?0 = 0`). This is what turns minimization into generalization. A result is sound when it means the same as
  the input under some binding.
- **Data as a piecewise.** A data set is turned mechanically into `pw(case(in, out), ..., ?0)`: one branch per point,
  and an extension term as the catch-all for every unseen input. This is the main use of free variables: binding the
  catch-all to a model is generalization, and holes inside the data (an unknown label `?1`, a list holding `?2`) lead
  to free-variable reductions in the domain itself. Reduction then tries to make the piecewise disappear,
  or to shrink it into regions (`when(condition, value)` branches) with one compact expression each.
- **Objective.** Smallest node count (`expr_size`) for the fixed functor set.
- **Rules come in two kinds**, both behind the same `detect(e, out)` capability:
  low-level local rewrites (e.g. `arith_simplifier`), and high-level *detectors* that read the expression or data
  and propose a far-away result in one step (e.g. `arith_linear_hypotheses` feeding `pw_extension_binder`).
  Detectors are the point of the project: search should not have to stumble into a reducible form.
- It is a library. Users write rules in C++ and compose them in a manifest. There is no CLI or parser yet.
- **Long-term goal** (`docs/vision.md`): the same recipe in every domain (algebra, calculus, lists, graphs, ...),
  with inputs and labels as any expressions of the domain and free variables for generality.

## Layout

```
core/hpp/{infrastructure,value_objects}       domain-free: expr, pool, symbols, size, printer, rule plumbing, search
core/cpp/...                                  .cpp for non-templated types
core/test/{unit,integration}, main.cpp
modules/<name>/{hpp,cpp,test}/...             one directory per rule module; prefix every file with the module's
                                              domain prefix, since every module's hpp/ is on the include path
  piecewise/  (pw_)                           the extension algebra over data sets, generic over the domain; the
                                              branch filter matches labels holding holes under one binding
  abstraction/ (abs_)                         binders and helpers, generic over the domain: `lam`, `lams`, `app`,
                                              `let`, de Bruijn indices, a reducer (beta-reduction with shifting),
                                              closing of fitted bodies, applying evaluation, the binder merger
  arith/      (arith_)                        integer arithmetic: literals, evaluator, simplifier, the linear hole
                                              solver (a label linear in one hole, solved for it)
  arith_fit/  (arith_)                        composition: linear-fit detector, manifest, runtime
  poly/       (poly_)                         polynomial algebra over Z: normal form oracle, rules, detectors
  poly_reduce/ (poly_)                        composition: kernel, rule sets, four solver manifests
  poly_fit/   (poly_)                         composition: polynomial data fitting over piecewises (interpolation
                                              detector, point evaluator, rule set, solver manifests, runtime;
                                              `poly_fit_hypotheses_manifest` lends polynomial fitting to other domains)
  list/       (list_)                         lists and list programs (map, filter, fold): vocabulary, evaluator
  list_fit/   (list_)                         composition: program detectors over list data (generic, and integer
                                              cases; affine, extremum, product and count folds from whole lists,
                                              generic selection and factor folds over an inner function g),
                                              delegating bodies to polynomial fitting; rule set, solver manifests,
                                              runtime
  seq/        (seq_)                          integer sequences: recurrences (`rec`), powers (`exp`), evaluator, the
                                              closing rule from affine recurrences to powers
  seq_fit/    (seq_)                          composition: the recurrence detector (steps from consecutive terms,
                                              handed to polynomial fitting), rule set, solver manifests, runtime
  number/     (num_)                          number-theory primitives over another domain: `prime(n)`, `is_prime(n)`,
                                              a sieve-backed prime table, their evaluator, and a detector comparing
                                              data with the primes (wired into poly, seq and list fitting)
  logic/      (logic_)                        boolean functions: and, or, xor, not over 0 and 1, evaluator
  logic_fit/  (logic_)                        composition: parity (exact solve mod 2) and factored sum-of-products
                                              detectors over truth tables, rule set, solver manifests, runtime
bench/                                        release benchmarks and the regression gate (`make bench_check`), not
                                              tests; committed baselines in bench/baseline/
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
    | best_first_search   or: expands the cheapest reached expression; bounded frontier, interned dedup
      subterm_proposer    lifts a local rule set to every position of the expression
        proposer_union    fans detect() out to the rules of several modules
          rules           pw_extension_binder, pw_branch_absorber, arith_simplifier, ...
```

- Expressions are hash-consed by `expr_pool`: pointer equality is structural equality. Search dedup, the
  derivation map and rules all rely on this.
- Rules detect, then rewrite: `detect(e, out)` does cheap checks and appends `rewrite` thunks
  (`std::function<const expr*()>`). Detection is certain: performing one always returns the whole result, never
  nullptr. MCTS performs only the rewrite it takes; best-first performs all rewrites of the expression it expands. Local rules only look at the node they are handed; `subterm_proposer` wraps their rewrites to
  rebuild the path to the root. See `docs/rule-design.md`.
- Domains plug into the piecewise module through capabilities: `IEvaluateAt` (value of an expression at an input,
  as an interned expression, or nullopt) and `IProposeHypotheses` (candidate resolutions of an extension term given
  the data points).
- **Every rule and detector follows `docs/rule-design.md`**: exit on the first failed requirement, run cheap
  necessary-condition filters (cached signatures, modular arithmetic) before any exact or big-integer work, and let
  filters only reject, never accept. Read it before writing or changing a rule.
- Integers are `integer` (`core/hpp/value_objects/integer.hpp`, Boost's `cpp_int`): arbitrary precision, so literal
  and coefficient arithmetic is exact and never overflows. Don't use built-in integer types for literal values.
- Polynomial algebra is the first full domain. Read `docs/poly.md` for its rules, detectors and benchmark.
- **No named variables: every variable is a de Bruijn index** (`docs/plans.md`, bound variables), inputs included.
  An index is a nullary symbol `#k` that a body's domain treats as a variable, so polynomial fitting and rules work on
  bodies unchanged. Binders are explicit and local: `lam(b)` binds one, `lams(k, b)` binds k at once (the first
  argument `#(k-1)`, the last `#0`), and a rule merges chains of 3+ `lam`s into one `lams`. **A piecewise is a
  function of its keys**: there is no root or global scope, and what replaces a piecewise's catch-all is closed over
  the piecewise's own inputs (`abs_closing_hypotheses`), so a fitted piecewise becomes e.g. `lam(#0^2 + 1)`,
  `lams(2, (#1 + #0)^2)`, `lam(map(lam(#0^2), #0))`, `lam(fold(lam(lam(#0 + #1)), 0, #0))` (head `#0`,
  accumulator `#1`), `lam(rec(lams(3, #1 + #2), 0, 1, #0))`. Evaluating a function at an input applies it
  (`abs_applying_evaluator`). Free variables `?n` are unchanged. Don't add a named variable.
- Lists are the first program domain. Read `docs/list.md`: detectors recognize a program's shape and hand the rest
  to polynomial fitting.
- Sequences are the third domain. Read `docs/seq.md`: a recurrence's steps are read off consecutive terms and fitted
  as polynomials, and a closing rule turns affine recurrences into powers.
- Number theory (`docs/number.md`) is vocabulary on top of the other domains: primitives for the primes, evaluated over
  a domain's evaluator, and a detector that also serves list filters as a condition domain.
- Logic is the fourth domain. Read `docs/logic.md`: parities by an exact solve mod 2, everything else by a factored
  Quine-McCluskey cover, with unseen inputs as don't cares.
- **Detecting a hard shape is decomposed** (`docs/decomposing-detection.md`): split it into subproblems, each made
  easy by one assumption, and keep each solution generic by handing the rest to another domain's detectors (e.g. a
  fold step as an easy outer combiner over an unknown inner function). Read it before adding a detector.
- The search strategy is a manifest choice: `arith_fit_manifest` (best-first) and `arith_fit_mcts_manifest` share
  every rule and differ only in search. Read `docs/search.md` before touching either search.

- **Plans** (`docs/plans.md`): what we intend to build next, below the level of `docs/vision.md`. Its first section,
  abstraction, is designed, not built: helper functions as a domain-free module (de Bruijn `lam`/`arg`/`app`/`let`).
  A helper is any connected cut of the computation graph (its cut edges are its inputs), not only a subterm; paired
  rules abstract a recurring fragment or inline it, firing only when they shrink. Read it before adding helpers,
  composition or recursion.
- **Showcase** (`docs/showcase.md`): **"Perform the showcase workflow"** invents realistic problems MANR should solve,
  confirms them, measures how far each generalizes, adds them to the benchmarks, and posts a capability report as a
  GitHub Discussion (category Show and tell). The positive counterpart of scout.
- **Two workflows** run on reduce-request issues. **"Perform the scout workflow"** (`docs/scout.md`): survey MANR, invent
  hard problems (in and beyond its domains), confirm they fail, diagnose the cause, file each as an issue.
  **"Perform the forge workflow"** (`docs/forge.md`): pull the issues, choose one, design the most general efficient
  fix (a rule, a family of rules, vocabulary or a new module), build and prove it general on the gate, report on the
  issue.
- **Reduce requests drive the roadmap** (`docs/reduce-requests.md`): a GitHub issue posts an input (an expression, or
  data, which becomes a piecewise) and the form it should reduce to. The form is a guide, not expected output:
  success is a sound result no larger than it. Any domain is welcome, including unsupported ones, which forge then
  builds modules for.

## Build & test

- `make core_debug_fast && ./build/core_debug_fast`, `make modules_debug_fast && ./build/modules_debug_fast`.
  `make run_tests` builds and runs both. `--gtest_filter=...` narrows a run.
- **Regression gate.** `make bench_check` (fast) and `make bench_check_full` compare every solver's cost on the
  polynomial and fitting corpora (`bench/`) with the committed baseline (`bench/baseline/*.tsv`), and fail on any
  cost that got worse or any unsound result. Any change to rules or search must pass it. When a change is accepted,
  run `make bench_baseline` and commit the baseline with the change. Follow `docs/extending.md` to add capability.
- The makefile `find`s every `.cpp` under `core/` and `modules/*/{cpp,test}`, and every `modules/*/hpp` is on the
  include path. **A new file or module is picked up automatically; don't register it in the makefile.**

## Code style

Carried over from Atlas unchanged. The rules live in `.claude/rules/*.md` and load automatically.
Core of it: simplest thing that works. `struct` only. Collaborators are injected by reference as template params
named per invoked capability. No lambdas or `std::function` (except rewrite thunks and tests), no overloads, no
default args (except rules' limit parameters), no default member initializers. Paired operations are mirror images.

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
