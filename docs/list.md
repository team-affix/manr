# Lists: fitting programs to data

The list domain is the first step toward fitting programs rather than formulas (`docs/vision.md`). Data is a
piecewise over lists, and the model is a small program that reads the input list.

```
pw(case(nil, 0), case(cons(1, nil), 1), case(cons(2, cons(1, nil)), 3), ..., ?0)   ->   fold(h + acc, 0, xs)
```

## Modules

- `list` (`list_`): the vocabulary and its evaluator. Domain-free apart from lists themselves: the bodies of programs
  belong to an injected element domain.
- `list_fit` (`list_`): the composition. The detectors, the sample converter, the rule set and the solver manifests.
  The element domain is polynomial fitting, through two `poly_fit_hypotheses_manifest`s.

To improve how programs are detected, work in `list_fit`. To add list operations, work in `list`.

## Vocabulary

| expression | meaning |
|---|---|
| `nil`, `cons(h, t)` | list values |
| `xs` | the input list |
| `map(f, l)` | `f`, in the element variable `e`, applied to every element of `l` |
| `filter(c, l)` | the elements of `l` where the condition `c`, in `e`, holds |
| `fold(s, b, l)` | right fold: `b` for `nil`; for `cons(h, t)`, `s` with `h` the head and `acc` the fold of `t` |

`list_evaluator` evaluates a program at an input list. It hands bodies to the element domain at `point(e)` (map,
filter) or `point(h, acc)` (fold), so a body is anything that domain evaluates: a polynomial, or a piecewise
polynomial with regions. Programs compose (`fold(h + acc, 0, map(e^2, xs))` evaluates), and the evaluator also
decides `lt(a, b)` between two integer-valued programs, for regions over lists.

## Detectors

Each detector recognizes the shape of a program and reduces the rest to a smaller fitting problem in the element
domain. The element domain proposes bodies (constants, interpolated polynomials, piecewise polynomials), and the
detector wraps each one as a program. As with every hypothesis, the binder keeps the data points the program does
not reproduce as branches.

`list_fit_rules_manifest` uses both kinds, side by side, following design rule 7 in `docs/extending.md`
(generalize beside, not instead of):

- **Generic, for elements of any domain.** They hand the element domain data points of expressions.
  - `list_map_hypotheses`: every list-to-list point whose lists have the same length gives one element data point
    per position, `point(e) -> output element`. Proposes `map(f, xs)`.
  - `list_fold_hypotheses`: the base is the output for `nil`. Every point `cons(h, t)` whose tail `t` is also in the
    data gives a step data point `point(h, output of t) -> output`, whatever the outputs are (numbers, lists).
    Proposes `fold(s, b, xs)`.
- **Integer.** The same reading for integer elements, with the samples ordered by
  value, plus the threshold filter.
  - `list_integer_map_hypotheses`, `list_integer_fold_hypotheses`.
  - `list_filter_hypotheses`: a point is a filtering when its output is its input with every occurrence of some
    values removed. When every kept value is above every removed one, proposes `filter(lt(d, e), xs)`; when below,
    `filter(lt(e, d), xs)`.
- **Affine folds, from whole lists.** `list_affine_fold_hypotheses` needs no tails and no empty list. It assumes the
  step is affine in the accumulator, `p(h) + c * acc`. Then a list's output is `sum_i c^(i-1) p(x_i) + c^n b`, so
  the value of every tail drops out: once `c` is fixed, the output is linear in `p`'s coefficients and the base `b`,
  and `list_affine_fold_solver` solves for them exactly (Gauss-Jordan over the rationals). Multipliers are tried by
  magnitude up to 10, from two starting lists so that one wrong label cannot spoil both. The fold that reproduces
  the most lists (more than half, and more than its unknowns) is proposed. It covers sum, length, sum of squares,
  alternating sums and digits; it does not cover steps where `acc` is multiplied by a function of `h` (product) or
  steps in pieces (maximum).
- **Extremum folds, from whole lists.** `list_extremum_fold_hypotheses` reads the largest or smallest element with a
  base, `fold(pw(when(lt(acc, h), h), acc), b, xs)` or with `lt(h, acc)`. A base only shows where an output lies
  beyond every element of its list (always for `nil`), so each such output is a candidate, beside one base that
  leaves every list to its elements (0 when it qualifies). The direction and base reproducing the most lists (more
  than half) are proposed, so a wrong label above its list cannot become the base. It is a direct check, linear in
  the data, and needs no tails.
- **Product folds, from whole lists.** `list_product_fold_hypotheses` reads `fold((a * h + k) * acc, b, xs)`: the
  output is `b` times one factor per element. Factors are tried simplest first (`a` up to 2 in size, `a = 0` left to
  the affine detector; `k` up to 3), the base is a starting list's output over its product when it divides, and a
  factor is abandoned at its first failure past what the best so far allows. Two starting lists, as for affine.
- **Count folds, from whole lists.** `list_count_fold_hypotheses` reads a base plus the number of elements above or
  below a threshold, `fold(pw(when(lt(d, h), acc + 1), acc), b, xs)` or with `lt(h, d)`. Every element value is tried
  as `d` in both directions on lists sorted once (each count is a binary search); the base is a starting list's
  output less its count, with the same early abandoning.

The polynomial rules then compress the bodies, at every position, as they do for polynomial fitting.

On integer data the generic and integer map and fold detectors find the same programs (identical costs on every
benchmark problem), so on integer data the pair proposes each program twice. Against the integer detectors alone,
running both made MCTS slower on every problem it spends time on (it detects again at every simulation step), and
made best-first faster on the long product (37 s to 0.9 s), where the generic order of samples suits the
interpolation. The difference is the order the samples reach the element domain.

## Benchmark

`bench/list_bench.cpp`, gated by `make bench_check` with its baseline in `bench/baseline/list.tsv`. 31 problems, 30
solved by both solvers:

- folds: sum, length, product, sum of squares, alternating sum, digits read as a number, sum over all 781 lists of
  length up to 4, and a sum with one wrong label (it stays as the only branch)
- maps: square, `2e + 1`, absolute value (a piecewise body), and a map with one wrong label
- filters: keep the positives, keep the values below one
- long lists, 20 to 40 elements with values up to 50 in size: sum, length, sum of squares, product (values reach
  about 9^40), a sum with one wrong label, two maps and a filter. These folds are given every list's tails; maps and
  filters need none, so 30 random lists are enough.
- sums without tails: every list of exactly three elements, and 40 random lists of 20 to 40 elements (affine fold)
- extremes: the maximum of short lists, the minimum of long lists without tails, and the maximum of long lists with
  one wrong label
- products and counts: a product of 30 lists without tails, a long product with one wrong label, and the count of
  elements above 10 in long lists without tails

Every result is checked against every sample. The soundness fuzz (`list_fit_soundness.cpp`) performs every rewrite
along random walks and checks the same.

## Fold families and how to detect them

Folds over integer lists to integers, grouped by the shape of their step, with the reading that finds each one from
whole lists:

| family | step | examples | detection | status |
|---|---|---|---|---|
| affine | `p(h) + c * acc` | sum, length, sum of squares, alternating sum, digits | linear in `p` and `b` once `c` is fixed; exact solve | `list_affine_fold_hypotheses` |
| selection | `pw(when(lt(acc, h), h), acc)` | maximum, minimum | output is an element or the base; direct check | `list_extremum_fold_hypotheses` |
| multiplicative | `q(h) * acc` | product, product of squares, `2^length` | output is `b` times a product of per-value factors: zeros mark the values with `q = 0`; on the rest, each prime's exponent in the output is linear in the per-value exponents (or, cheaper, `log|out|` by least squares, then rounded and checked exactly); signs are linear mod 2. The factors per value then go to the element domain. Built: linear factors `a * h + k` by enumeration | `list_product_fold_hypotheses` (linear factors; product of squares open) |
| counting | `pw(when(c(h), acc + 1), acc)` | count of positives, count above a threshold | output is the number of elements passing a threshold: try the thresholds between element values, as the filter detector does | `list_count_fold_hypotheses` (one threshold) |
| selection of a value | `pw(when(lt(acc, p(h)), p(h)), acc)` | largest square, largest absolute value | output is `p` of some element: fit `p` from the elements that attain it. Which element attains it is unknown, so this is a search over element choices per list (or a guess of `p` checked directly) | open |
| boolean | `and`/`or` of a condition | all positive, any zero | a counting fold compared with 0 or the length | open (needs booleans) |
| list-valued | `cons(h, acc)`, `append(acc, cons(h, nil))` | copy, reverse, append | the output's elements are a permutation of the input's: compare positions | open (needs a list step domain) |

## Gaps (the open problems)

- **Steps in pieces in general.** The extremum detector covers the largest and smallest element. A step in pieces
  that compares the two variables some other way (`lt(acc, p(h))`, e.g. the largest square) is still out of reach:
  the split detector only cuts on one variable at a threshold.
- **Data without tails, beyond the built families.** Without tails, affine, extremum, linear-factor product and
  single-threshold count steps are found. A factor that is not linear in `h` (`h^2 * acc`) needs the per-value
  factor reading in the table; a count on a condition other than one threshold needs the condition handed to the
  element domain.
- **Detectors' drag.** The product and count detectors abandon a candidate at its first failure past what the best
  allows, and add little; long sums under MCTS still went from about 3.3 s to 4 s across the extremum, product and
  count detectors. **The affine detector's drag.** On data it cannot fit (product, maximum) it tries every multiplier from both starts
  on every detection, and MCTS detects again at every step: product under MCTS went from 0.77 s to 1.65 s. A
  necessary-condition filter (solving modulo a small prime before the exact solve) or caching detection per
  expression would remove most of it.
- **Composition.** `map(e^2, filter(lt(0, e), xs))` needs a detector that recognizes one program inside another.
- **Enough element values.** A piece of a body needs more distinct element values than it has coefficients, as in
  polynomial fitting.
- **Filters are thresholds.** No parity, membership or multi-interval conditions yet.
- **No list-valued folds.** `reverse` or `append` need a fold whose accumulator is a list.
- **Large expressions are slow to expand.** On the long product, best-first finds `fold(h * acc, 1, xs)` on its first
  expansion (0.03 s), then spends 37 s using the rest of its budget, because every expansion detects at every
  position of a piecewise of about 15000 nodes. A search that stops at a form it cannot improve, or detection that
  skips the data's subterms, would remove it. The order of step samples also matters here (see above).
- **No generic filter yet.** It would hand the element domain data points labelled true or false (kept or removed)
  and take a condition back. It needs boolean values and a condition detector in the element domain.
- **MCTS drag.** With a wrong label, the step samples have an outlier, the fold detector's fitting is slower, and MCTS
  repeats it at every step of every simulation (33 s against 0.3 s for best-first). Caching detection per expression
  (`docs/extending.md`) would remove it.
