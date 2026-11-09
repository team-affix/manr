# Sequences: recurrences and closed forms

The sequence domain fits integer sequences, data over one index variable, with a recurrence or a closed form
(`docs/vision.md`). It is the third domain after polynomials and lists, built for reduce requests #6 (Fibonacci) and
#7 (powers of two).

```
pw(case(point(0), 0), case(point(1), 1), case(point(2), 1), ..., ?0)   ->   rec(p1 + p2, 0, 1, n)
pw(case(point(0), 1), case(point(1), 2), case(point(2), 4), ..., ?0)   ->   exp(2, n)
```

## Modules

- `seq` (`seq_`): the vocabulary, its evaluator and the closing rule. The step of a recurrence belongs to an injected
  step domain, and anything that is not a recurrence or a power to an injected outer domain.
- `seq_fit` (`seq_`): the composition. The recurrence detector, the rule set, the solver manifests and the runtime.
  Both injected domains are polynomial: `fit_` over the index variable does everything polynomial fitting does, and
  one `poly_fit_hypotheses_manifest` per order fits steps.

## Vocabulary

| expression | meaning |
|---|---|
| `rec(s, b0, ..., b(k-1), n)` | the term at index `n` of the sequence `a` with `a(j) = bj` for `j < k`, and from `k` on `a(j) = s`, with `i` the index `j` and `p1`, `p2`, `p3` the terms `a(j-1)`, `a(j-2)`, `a(j-3)` (0 before the start). Order `k` is the number of bases, 1 to 3. |
| `exp(b, n)` | `b` to the power `n` |

The step variables `i` and `p1` to `p3` are fixed names, to become de Bruijn indices like every bound variable
(`plans.md`, bound variables).

Both have no value at a negative index. `seq_evaluator` runs a recurrence by handing the step to the step domain at
`point(i, p1, p2, p3)`. It evaluates `add`, `sub`, `mul` and `pow` over recurrences and powers itself (as in
`5 * exp(3, n) + 1`), and leaves everything else to the polynomial evaluator. Conditions (`lt`) go through it too, so
regions can compare recurrences.

## Detectors and rules

- **`seq_recurrence_hypotheses`** (one per order, 1 to 3): the split of `decomposing-detection.md`, the same as folds
  with tails. Consecutive terms make every step visible: each index `j` whose `k` previous terms are in the data gives
  the step point `(j, a(j-1), ..., a(j-k)) -> a(j)`. These points go to polynomial interpolation over `i, p1, ..., pk`.
  Each step it proposes becomes `rec(step, a(0), ..., a(k-1), n)`. The step may use the index, so `i * p1`
  (factorial) is found as easily as `p1 + p2`. Interpolation starts from several places, so a wrong label costs only
  a branch. Before any interpolation the detector rules data out cheaply: too few terms for its order (`2k + 3`), a
  constant sequence (left to the constant detector), or a missing base.
- **`seq_power_closer`** (local rule): closes an affine recurrence of order one, step `r * p1 + d` (`r` not 0 or 1),
  into `a * exp(r, n) + b`, with `b = -d / (r - 1)` the fixed point and `a = a(0) - b`. It fires only when `b` is an
  integer, and drops the trivial parts. It always shrinks the expression. So `rec(2 * p1, 1, n)` (cost 6) becomes
  `exp(2, n)` (3), and `rec(2 * p1 + 1, 0, n)` (8) becomes `exp(2, n) - 1` (5).

Polynomial data still goes to polynomial fitting first. The polynomial rule set compresses steps like any polynomial.

## Benchmark

`bench/seq_bench.cpp` (`make seq_bench`, gated in `make bench_check`, baseline `bench/baseline/seq.tsv`): 13
problems, 12 solved by both solvers.

| problem | target | form |
|---|---|---|
| #7 powers of two | 3 | `exp(2, n)` |
| #6 Fibonacci | 7 | `rec(p1 + p2, 0, 1, n)` |
| five times powers of 3 | 5 | `5 * exp(3, n)` |
| powers of -2 | 3 | `exp(-2, n)` |
| Lucas numbers | 7 | `rec(p1 + p2, 2, 1, n)` |
| Pell numbers | 9 | `rec(2 * p1 + p2, 0, 1, n)` |
| tribonacci | 10 | `rec(p1 + p2 + p3, 0, 0, 1, n)` |
| factorial | 6 | `rec(i * p1, 1, n)` |
| Fibonacci, wrong label | 12 | `pw(case(point(10), 56), rec(p1 + p2, 0, 1, n))` |
| squares (polynomial fitting alone) | 3 | `n^2` |
| powers of two, 61 terms | 3 | `exp(2, n)` |
| Mersenne numbers | 5 | `exp(2, n) - 1` |
| Fibonacci from index 1 (open) | 7 | `rec(p1 + p2, 0, 1, n)` |

Best-first spends 6.7 s on powers of -2, though it reaches `exp(-2, n)` in its first rewrites. Plain polynomial
fitting spends its whole budget on that data too (9.7 s, ending at cost 66): best-first keeps expanding split fits of
the alternating terms (`plans.md`, search: stopping at a form nothing can improve).

## Gaps (the open problems)

- **Data that does not start at index 0.** The bases are read from the data's first terms, so Fibonacci from index 1
  has no reading. Bases could be solved for instead: run the step backwards, or treat them as unknowns of the fit.
- **Closed forms of higher order.** Only order-one affine steps close. Fibonacci's closed form (Binet) is irrational.
  An integer one, like `a(n) = 2^n + 3^n` from a second-order step, needs the roots of the characteristic polynomial.
- **Recurrences inside larger expressions** are evaluated, but nothing proposes one as part of a larger model, e.g.
  a piecewise of recurrences, or `rec(...) + n^2`.
- **Steps the polynomial domain cannot express**: `a(n) = a(n-1) + a(n-1) mod 3`, `a(n) = a(a(n-1))`.
- **Sequences of other values** (lists, rationals), and the index as part of a tuple, wait on the core (`plans.md`).
