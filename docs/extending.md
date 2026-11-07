# Extending without regressing

MANR grows by adding rules, detectors and domains. Each addition can make the solvers worse at what they already
do, even when the new rule is correct. This document is the method for adding capability safely. It has four parts:

- the ways an addition does harm
- the corpus that defines what the solvers can do
- the gate that compares every change against that corpus
- design rules that keep additions cheap for search

## Three ways an addition does harm

| harm | what happens | caught by | prevented by |
|---|---|---|---|
| **Wrong** | a rewrite changes what the expression means | the domain's soundness fuzz test | detection is certain (`rule-design.md`) |
| **Dilution** | more rewrites per expression, so the search budget is spread thinner and the productive move is found later or never | a cost getting worse in the gate | proposing only non-dominated rewrites; detectors instead of chains |
| **Drag** | detection runs at every position of every expression the search visits, so a slow rule slows every solver | timings in the gate, as warnings | cheap necessary-condition filters first (`rule-design.md`) |

Wrong is the only harm a unit test catches. Dilution and drag only show up when the whole rule set runs under a
search with a budget, on many problems. That is why the corpus and the gate exist.

Both kinds have already happened:

- **Dilution.** `pw_constant_hypotheses` proposed one binding for every distinct output, so the 1000-point data set
  offered about 1000 constant bindings beside the one interpolated cubic. Best-first performs them all and is fine.
  MCTS, given 150 simulations, never tried the cubic and finished at cost 3998 instead of 9. (Fixed; see the worked
  example.)
- **Drag.** `poly_split_hypotheses` runs on every open piecewise. MCTS detects again at every step of every
  simulation, so on 49 two-variable points it takes 30 s, where best-first takes 0.24 s.

## The corpus is the specification

What the solvers can do is defined by two benchmarks. Both are deterministic in cost: best-first has no randomness
and MCTS is seeded. So a cost that changes is a real change, not noise.

- `bench/poly_bench.cpp`: 42 polynomial simplification problems under four solvers (full or low-level rules, each
  under best-first and MCTS).
- `bench/fit_bench.cpp`: 20 data-fitting problems (one of them open): polynomials, outliers, piecewise polynomials, up to three variables
  and 1000 points. Two solvers (best-first and MCTS).

Each problem has a target cost: the cost of a known compact form. A problem is solved when a solver reaches a cost no
larger than the target. Problems marked `(open)` have a target no solver reaches yet. They are kept so the gate reports
the change that first solves one.

The results of the current solvers are committed in `bench/baseline/*.tsv`, one line per problem and solver. In a
commit that changes capability, the diff of these files is the record of what changed.

## The gate

```
make run_tests          # unit, integration and soundness fuzz tests
make bench_check        # fast tier: best-first everywhere, MCTS except the largest problems
make bench_check_full   # every problem under every solver (about 20 minutes)
make bench_baseline     # record the current results as the baseline
```

`bench_check` compares a run with the baseline (`bench/bench_gate.hpp`):

- **Fails** on any unsound result. For polynomials, sound means the normal form is unchanged. For fits, it means
  every sample is still reproduced.
- **Fails** on any problem whose cost is larger than its baseline cost under the same solver. One cost getting worse
  is enough, even if everything else improved: the guarantee is that nothing gets worse.
- **Reports** improved costs, newly solved problems, problems or solvers that are new, and results more than 1.5×
  slower than the baseline (and slower by more than 0.25 s). Timings depend on the machine and its load, so they warn
  and never fail. Read them, and investigate anything consistent.

## The loop for a new capability

1. **Write the problems first.** Add to a corpus the problems the new capability should solve, with their target
   costs, plus at least one it should not change. Run the gate. The new problems appear as `new` and unsolved. This
   is test-first development at the level of the corpus.
2. **Design the rule.** Follow `rule-design.md`: certain detection, filters before exact work, limits as parameters.
   Prefer a detector that proposes the final result in one step (see the design rules below).
3. **Test it.** Write a unit test for its logic and an integration test where it depends on real interning. Make sure
   the domain's soundness fuzz covers it. A rule added to a domain's rule union is covered automatically. A new
   domain needs its own fuzz test with its own oracle (`poly_soundness.cpp`, `poly_fit_soundness.cpp`).
4. **Run `make bench_check`.** If anything gets worse, the fix belongs in the rule, never in the baseline. Look at
   what the losing solver now does on that problem (the benchmark prints every best form) and apply a design rule.
5. **Run `make bench_check_full` before landing.** MCTS on the large problems only runs here.
6. **Record and commit.** Run `make bench_baseline` and commit the code and the new baseline together.
7. **Every bug becomes a problem.** A wrong result, a regression or a surprising failure found later becomes a
   corpus problem or a test before it is fixed.

Raising a solver's budget to make the gate pass hides a regression instead of fixing it. If a budget really needs to
change, change it in its own commit with a fresh baseline, so its effect is visible on its own.

## Design rules that keep additions cheap for search

1. **Propose only non-dominated rewrites.** A proposal is dominated when another proposal from the same rule, on the
   same expression, gives a result no larger, and nothing further is reachable from the dominated one. Drop it during
   detection. Each extra sibling costs MCTS simulations and costs best-first frontier space.
2. **Prefer one-step detectors to chains.** A detector that proposes the final form adds one sibling. A chain of
   enabling local steps adds a level at each step where search can be diverted, and the steps that make the expression
   larger are the ones best-first ranks last.
3. **Local rules should not make things larger.** A local rule proposes only rewrites that keep the expression the
   same size or smaller. When a larger intermediate step is needed, write a detector that proposes the step's payoff
   directly.
4. **Earn the detection cost.** A rule that runs at every position must reject a non-match in a few operations,
   ideally from cached signatures, before any exact work.
5. **Composition decides, not the rule.** Rules are opt-in per manifest. A capability that hurts one solver can ship
   in a separate manifest, and be compared against the current one in the benchmark, until it is fixed.
6. **Keep searches deterministic.** Seed all randomness. Never let the iteration order of a pointer-keyed unordered
   container decide anything the search does. The gate's exact comparisons depend on this.

## Worked example: removing dilution

`pw_constant_hypotheses` proposed every distinct output of the data as a binding for the extension term. The fix
follows design rule 1. Binding to a constant `c` drops the branches whose output is `c` and leaves `c` as the
catch-all, with nothing further to reduce. So the constant that saves the most nodes dominates every other one, and
now only that one is proposed.

1. The problems already existed: `cubic 1000` and `two pieces 400` in `fit_bench`, unsolved by MCTS.
2. The unit tests changed with the behavior. `make run_tests`, including both soundness fuzz tests, passes.
3. `make bench_check`: 0 failures. The fast tier skips MCTS on these problems, so nothing else changed.
4. `make bench_check_full`:

   ```
   gate: 168 compared, 0 failures                                     (polynomials)
     better  cubic 1000 / mcts: cost 3998 -> 9  (newly solved)
     better  two pieces 400 / mcts: cost 1598 -> 13  (newly solved)
   gate: 40 compared, 0 failures                                      (fitting)
   ```

   Best-first on `cubic 1000` also dropped from 0.47 s to 0.03 s, because it no longer performs 1000 dead-end
   bindings.
5. `make bench_baseline`, committed with the change.

The gate was also checked against regressions it should catch:

- A baseline with one cost edited lower fails with `FAIL worse` and exit code 1.
- Cutting the fit solver's budget from 3000 rewrites to 30 fails on the one problem it hurts.

## Planned defenses on the search side

- **Memoize detection in MCTS.** Expressions are interned, so the rewrites of an expression can be cached by its
  address. That removes most of the drag in the example above.
- **Per-rule counters in `proposer_union`:** calls, detection time, proposals, and proposals on the best derivation.
  A rule that never appears on a best derivation in the corpus is costing the search and contributing nothing.
- **A rollout policy that prefers detector proposals** (see `search.md`).
