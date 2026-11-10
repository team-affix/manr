# Sequences: recurrences and closed forms

The sequence domain fits integer sequences, data over one index variable, with a recurrence or a closed form
(`docs/vision.md`). It is the third domain after polynomials and lists, built for reduce requests #6 (Fibonacci) and
#7 (powers of two).

```
pw(case(point(0), 0), case(point(1), 1), case(point(2), 1), ..., ?0)   ->   lam(rec(lams(3, #1 + #2), 0, 1, #0))
pw(case(point(0), 1), case(point(1), 2), case(point(2), 4), ..., ?0)   ->   lam(exp(2, #0))
```

**Notation.** There are no named variables: inputs and parameters are de Bruijn indices `#k`, and every fitted model
is a closed function of the data piecewise's inputs, `lam(b)` or `lams(k, b)` (`plans.md`, bound variables). Older
examples below that name an input (`x`, `xs`, `n`, `a`) mean its index: one input is `#0`; with k inputs the first is
`#(k-1)` and the last `#0`.

## Modules

- `modules/seq` (`seq_`): the vocabulary, its evaluator, the recurrence detector, and the rules (the closing rule,
  the unfolder and the open-list reader). The step of a recurrence belongs to an injected step domain, and anything
  that is not a recurrence or a power to an injected outer domain.
- `solvers/seq_fit`: the composition. The rule set, the solver manifests and the runtime. Both injected domains are
  polynomial: `fit_` over the index variable does everything polynomial fitting does, and
  one `poly_fit_hypotheses_manifest` per order fits steps.

## Vocabulary

| expression | meaning |
|---|---|
| `rec(f, b0, ..., b(k-1), n)` | the term at index `n` of the sequence `a` with `a(j) = bj` for `j < k`, and from `k` on `a(j) = s`, where `f` is the closed step function `lam(...lam(s))`, `k + 1` lambdas deep: `s` sees the index `j` as `#0` (innermost) and the terms `a(j-1)`, ..., `a(j-k)` as `#1`, ..., `#k` (0 before the start). Order `k` is the number of bases, 1 to 3. |
| `exp(b, n)` | `b` to the power `n` |
| `tabulate(f)` | the infinite list `f(0), f(1), f(2), ...` of a function of the index |
| `unfold(f, b0, ..., b(k-1))` | the infinite list of a recurrence's terms, `rec(f, b0, ..., b(k-1), n)` for `n >= 0` |

The step's variables are de Bruijn indices like every bound variable (`plans.md`, bound variables). The binders
are ordered so that each index means the same thing at every order: `#0` is always the index, `#1` the previous term.
In the prose below, `i` is the index `#0` and `p1`, `p2`, `p3` the previous terms `#1`, `#2`, `#3`.

Both have no value at a negative index. `seq_evaluator` runs a recurrence by unwrapping its step function's `k + 1`
lambdas and handing the body to the step domain at `point(i, p1, p2, p3)`, binding `#0` to `#3`. It evaluates `add`, `sub`, `mul` and `pow` over recurrences and powers itself (as in
`5 * exp(3, n) + 1`), and leaves everything else to the polynomial evaluator. Conditions (`lt`) go through it too, so
regions can compare recurrences.

## Detectors and rules

- **`seq_recurrence_hypotheses`** (one per order, 1 to 3): the split of `decomposing-detection.md`, the same as folds
  with tails. Consecutive terms make every step visible: each index `j` whose `k` previous terms are in the data gives
  the step point `(j, a(j-1), ..., a(j-k)) -> a(j)`. These points go to polynomial interpolation over `i, p1, ..., pk`.
  Each step body it proposes, closed under `k + 1` lambdas, becomes `rec(lam(...lam(step)), a(0), ..., a(k-1), n)`. The step may use the index, so `i * p1`
  (factorial) is found as easily as `p1 + p2`. Interpolation starts from several places, so a wrong label costs only
  a branch. Before any interpolation the detector rules data out cheaply: too few terms for its order (`2k + 3`), or
  a constant sequence (left to the constant detector). Bases the data does not have are solved by running each step
  backwards from the first `k` observed terms (#11), up to 64 indices: a step back solves
  `a(j) = s(j, a(j-1), ..., a(j-k))` for the oldest term, read as a line through its values at 0 and 1 and checked
  exactly. So Fibonacci observed from index 10 reads as `rec(lam(lam(lam(#1 + #2))), 0, 1, n)`. A step with no
  integer inverse there (`p1^2 + 1`) gives no recurrence.
- **`seq_power_closer`** (local rule): closes an affine recurrence of order one, step `r * p1 + d` (`r` not 0 or 1),
  into `a * exp(r, n) + b`, with `b = -d / (r - 1)` the fixed point and `a = a(0) - b`. It fires only when `b` is an
  integer, and drops the trivial parts. It always shrinks the expression. So `rec(lam(lam(2 * #1)), 1, n)` (cost 8)
  becomes `exp(2, n)` (3), and `rec(lam(lam(2 * #1 + 1)), 0, n)` (10) becomes `exp(2, n) - 1` (5).
- **`seq_linear_closer`** (local rule): closes a linear recurrence with constant coefficients of any order, step
  `c1 * #1 + ... + ck * #k + p(#0)`, when its characteristic polynomial splits into integer roots, each other than 1
  simple (#35): `5 * #1 - 6 * #2` from 2, 5 is `exp(2, #0) + exp(3, #0)`, and `3 * #1 - 2 * #2 + 1` from 1, 1 is
  `exp(2, #0) - #0`. The closed form is a power per root other than 1 plus a polynomial of degree deg p plus the
  multiplicity of 1, its coefficients solved exactly from the first terms (`arith_linear_solver`) on more terms than
  they need, which proves it. Only integer coefficients, only when the result holds a power and is smaller.

- **`seq_open_list_reader`** (at the root, since it rebinds a hole): an open-ended list `cons(a0, ..., cons(a(k-1),
  ?n))` of at least 3 literal terms is the infinite list of a sequence, `tabulate(pw(case(point(0), a0), ...,
  case(point(k-1), a(k-1)), ?n))`: the terms become data over the index and the tail's hole the catch-all, which the
  binder fits like any sequence (#16). Under a binding of the tail the two are the same list. A term holding a hole
  becomes a label holding it (#37): `0, 1, 1, 2, ?1, 5, 8, 13, 21, ?0` reduces to `unfold(lams(3, #1 + #2), 0, 1)`
  with `?1 = 3`. When the windows a hole breaks leave the data one step point short, the recurrence detector fits the
  step without the index (over `#1`, ..., `#k`), which has one unknown fewer.
- **`seq_unfolder`** (local rule): `tabulate(lam(rec(f, b0, ..., #0)))` is `unfold(f, b0, ...)`. So the open
  Fibonacci list `0, 1, 1, 2, 3, 5, 8, 13, ?0` reduces to `unfold(lams(3, #1 + #2), 0, 1)` (cost 8), the open powers
  of two to `tabulate(lam(exp(2, #0)))`, and open squares to `tabulate(lam(#0^2))`. Nothing evaluates infinite
  lists yet: list programs over them (`map` over `unfold`) are not read.

Polynomial data still goes to polynomial fitting first. The polynomial rule set compresses steps like any polynomial.

## Benchmark

`bench/seq_bench.cpp` (`make seq_bench`, gated in `make bench_check`, baseline `bench/baseline/seq.tsv`): 13
problems, 12 solved by both solvers.

| problem | target | form |
|---|---|---|
| #7 powers of two | 3 | `exp(2, n)` |
| #6 Fibonacci | 10 | `rec(lam(lam(lam(#1 + #2))), 0, 1, n)` |
| five times powers of 3 | 5 | `5 * exp(3, n)` |
| powers of -2 | 3 | `exp(-2, n)` |
| Lucas numbers | 10 | `rec(lam(lam(lam(#1 + #2))), 2, 1, n)` |
| Pell numbers | 12 | `rec(lam(lam(lam(2 * #1 + #2))), 0, 1, n)` |
| tribonacci | 14 | `rec(lam(lam(lam(lam(#1 + #2 + #3)))), 0, 0, 1, n)` |
| factorial | 8 | `rec(lam(lam(#0 * #1)), 1, n)` |
| Fibonacci, wrong label | 15 | `pw(case(point(10), 56), rec(lam(lam(lam(#1 + #2))), 0, 1, n))` |
| squares (polynomial fitting alone) | 3 | `n^2` |
| powers of two, 61 terms | 3 | `exp(2, n)` |
| Mersenne numbers | 5 | `exp(2, n) - 1` |
| #15 Fibonacci, unknown term `?1` | 10 | `lam(rec(lams(3, #1 + #2), 0, 1, #0))`, `?1 = 13` |
| Pell numbers, two unknown terms | 12 | `lam(rec(lams(3, 2*#1 + #2), 0, 1, #0))` |
| powers of two, a term `4 * ?1` | 4 | `lam(exp(2, #0))`, `?1 = 256` |
| #11 Fibonacci from index 10, and from index 1 | 10 | `lam(rec(lams(3, #1 + #2), 0, 1, #0))` |
| Pell from index 6, tribonacci from index 5 | 12, 13 | bases solved backwards |
| factorial from index 4, powers of 3 from index 8 | 9, 4 | `lam(rec(lam(lam(#0 * #1)), 1, #0))`, `lam(exp(3, #0))` |
| `p1^2 + 1` from index 2 | 27 | no recurrence: the step has no inverse |

Best-first spends 6.7 s on powers of -2, though it reaches `exp(-2, n)` in its first rewrites. Plain polynomial
fitting spends its whole budget on that data too (9.7 s, ending at cost 66): best-first keeps expanding split fits of
the alternating terms (`plans.md`, search: stopping at a form nothing can improve).

## Gaps (the open problems)

- **Data that starts late with a step that has no inverse** (`p1^2 + 1`): its bases could be unknowns of the fit
  instead, or the recurrence could carry its start index.
- **Closed forms of higher order** close when the characteristic roots are integers and the coefficients too
  (#35). Not yet: repeated roots other than 1 (`n * 2^n`), rational coefficients (`(n^2 - 3n) / 2 + 2^n`), and
  irrational roots (Fibonacci's Binet form), which stay recurrences.
- **Recurrences inside larger expressions** are evaluated, but nothing proposes one as part of a larger model, e.g.
  a piecewise of recurrences, or `rec(...) + n^2`.
- **Steps the polynomial domain cannot express**: `a(n) = a(n-1) + a(n-1) mod 3`, `a(n) = a(a(n-1))`.
- **Sequences of other values** (lists, rationals), and the index as part of a tuple, wait on the core (`plans.md`).
