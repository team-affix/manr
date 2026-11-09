# Lists: fitting programs to data

The list domain is the first step toward fitting programs rather than formulas (`docs/vision.md`). Data is a
piecewise over lists, and the model is a small program that reads the input list.

```
pw(case(nil, 0), case(cons(1, nil), 1), case(cons(2, cons(1, nil)), 3), ..., ?0)   ->   lam(fold(lam(lam(#0 + #1)), 0, #0))
```

**Notation.** There are no named variables: inputs and parameters are de Bruijn indices `#k`, and every fitted model
is a closed function of the data piecewise's inputs, `lam(b)` or `lams(k, b)` (`plans.md`, bound variables). Older
examples below that name an input (`x`, `xs`, `n`, `a`) mean its index: one input is `#0`; with k inputs the first is
`#(k-1)` and the last `#0`.

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
| `lam(b)` | a closed function: `b` refers to its argument by de Bruijn index, `#0` for the innermost `lam` |
| `map(lam(f), l)` | `f` applied to every element of `l`, the element as `#0` |
| `filter(lam(c), l)` | the elements of `l` where the condition `c` holds, the element as `#0` |
| `fold(lam(lam(s)), b, l)` | right fold: `b` for `nil`; for `cons(h, t)`, `s` with the head `h` as `#0` (inner `lam`) and the fold of `t` as `#1` (outer `lam`) |
| `append(a, b)` | the elements of `a`, then those of `b` |
| `reverse(l)` | the elements of `l` in reverse order (a primitive) |

**Primitives are for very common concepts only.** A primitive makes its concept nearly free in description length,
so it puts that answer into the vocabulary. `reverse` is common enough to deserve one; anything rarer is built from
the general vocabulary (folds, `append`, helpers) and found by detectors, which also keeps the general readings: the
fold that builds a reversed list finds "duplicate each element" too, and stays beside the primitive.

Every bound variable is a de Bruijn index (`plans.md`, bound variables), so binders nest. The fold binds its
accumulator outside and its head inside, so the element is `#0` in every program: a body fitted for a map can be a
fold's inner function as it stands. Sum is `fold(lam(lam(#0 + #1)), 0, xs)`.

`list_evaluator` evaluates a program at an input list. It unwraps the function's binders and hands the body to the
element domain at `point(element)` (map, filter) or `point(head, accumulator)` (fold), binding `#0` and `#1`, so a
body is anything that domain evaluates: a polynomial in the indices, or a piecewise polynomial with regions. A body
that is not under the right number of `lam`s has no value. Programs compose
(`fold(lam(lam(#0 + #1)), 0, map(lam(#0^2), xs))` evaluates), and the evaluator also decides `lt(a, b)` between two
integer-valued programs, for regions over lists.

A fold step the element domain cannot evaluate, one that builds a list (`append(#1, cons(#0, nil))`), is evaluated by
the list evaluator itself, with the head and the accumulator substituted for `#0` and `#1`.

## Detectors

Each detector recognizes the shape of a program and reduces the rest to a smaller fitting problem in the element
domain. The element domain proposes bodies (constants, interpolated polynomials, piecewise polynomials), and the
detector wraps each one as a program. As with every hypothesis, the binder keeps the data points the program does
not reproduce as branches.

`list_fit_rules_manifest` uses both kinds, side by side, following design rule 7 in `docs/extending.md`
(generalize beside, not instead of):

- **Generic, for elements of any domain.** They hand the element domain data points of expressions.
  - `list_map_hypotheses`: every list-to-list point whose lists have the same length gives one element data point
    per position, `point(element) -> output element`. Proposes `map(lam(f), xs)`.
  - `list_fold_hypotheses`: the base is the output for `nil`. Every point `cons(h, t)` whose tail `t` is also in the
    data gives a step data point `point(h, output of t) -> output`, whatever the outputs are (numbers, lists).
    Proposes `fold(lam(lam(s)), b, xs)`.
- **Integer.** The same reading for integer elements, with the samples ordered by
  value, plus the threshold filter.
  - `list_integer_map_hypotheses`, `list_integer_fold_hypotheses`.
  - `list_filter_hypotheses`: a point is a filtering when its output is its input with every occurrence of some
    values removed. When every kept value is above every removed one, proposes `filter(lam(lt(d, #0)), xs)`; when below,
    `filter(lam(lt(#0, d)), xs)`.
- **Affine folds, from whole lists.** `list_affine_fold_hypotheses` needs no tails and no empty list. It assumes the
  step is affine in the accumulator, `p(h) + c * acc`. Then a list's output is `sum_i c^(i-1) p(x_i) + c^n b`, so
  the value of every tail drops out: once `c` is fixed, the output is linear in `p`'s coefficients and the base `b`,
  and `list_affine_fold_solver` solves for them exactly (Gauss-Jordan over the rationals). Multipliers are tried by
  magnitude up to 10, from two starting lists so that one wrong label cannot spoil both. The fold that reproduces
  the most lists (more than half, and more than its unknowns) is proposed. It covers sum, length, sum of squares,
  alternating sums and digits; it does not cover steps where `acc` is multiplied by a function of `h` (product) or
  steps in pieces (maximum).
- **Extremum folds, from whole lists.** `list_extremum_fold_hypotheses` reads the largest or smallest element with a
  base, `fold(lam(lam(pw(when(lt(#1, #0), #0), #1))), b, xs)` or with `lt(#0, #1)`. A base only shows where an output lies
  beyond every element of its list (always for `nil`), so each such output is a candidate, beside one base that
  leaves every list to its elements (0 when it qualifies). The direction and base reproducing the most lists (more
  than half) are proposed, so a wrong label above its list cannot become the base. It is a direct check, linear in
  the data, and needs no tails.
- **Product folds, from whole lists.** `list_product_fold_hypotheses` reads `fold(lam(lam((a * #0 + k) * #1)), b, xs)`: the
  output is `b` times one factor per element. Factors are tried simplest first (`a` up to 2 in size, `a = 0` left to
  the affine detector; `k` up to 3), the base is a starting list's output over its product when it divides, and a
  factor is abandoned at its first failure past what the best so far allows. Two starting lists, as for affine.
- **Count folds, from whole lists.** `list_count_fold_hypotheses` reads a base plus the number of elements above or
  below a threshold, `fold(lam(lam(pw(when(lt(d, #0), #1 + 1), #1))), b, xs)` or with `lt(#0, d)`. Every element value is tried
  as `d` in both directions on lists sorted once (each count is a binary search); the base is a starting list's
  output less its count, with the same early abandoning.
- **A map over a filter.** `list_map_filter_hypotheses` reads `map(lam(f), filter(lam(c), xs))` by the outer-inner split
  (`decomposing-detection.md`): every element value is either removed or kept and mapped to `f` of it. The signature
  reading starts it from every list at once: a kept value and its image appear in exactly the same lists, so an
  output value whose set of lists is that of exactly one input value is that value's image (values sharing a set of
  lists are left alone). This is what reads long random lists, where every list mixes kept and removed values
  (reduce request #1). Then a point whose output is as long as its values not known to be removed keeps all of them and pairs them by position; a point
  whose output is exactly the images of its values known to be kept removes the rest (an empty output removes
  all). Points are revisited until nothing new is learned. The kept and removed values must be split by a
  threshold, as for the filter detector, and `f`'s values go to the element domain. Plain maps and plain filters
  are left to their own detectors. On lists of -2 to 2 (the open benchmark problem) only 1 and 2 are kept, so `f` is
  known at two values, and polynomial fitting proposes nothing it cannot check on a third; 30 random lists of -9 to 9
  are enough.
- **Selection of a value, from whole lists.** `list_selection_fold_hypotheses` reads the largest (or smallest) value
  of an unknown inner function, `fold(lam(lam(pw(when(lt(#1, g(#0)), g(#0)), #1))), b, xs)`: the outer-inner split with `max`.
  For the largest, `g(v)` is at most the smallest output of any list holding `v`, and that bound is exact wherever
  `v` attains a maximum, so the bounds reproduce every list when the model is right (a direction is refuted when
  they reproduce no more than half). A list whose output is the bound of exactly one of its values pins `g` there;
  those values go to the element domain, whose body in `#0` is the step's `g` as it stands. The base is the empty
  list's output, else 0 when no output lies below it, else the lowest output. The identity and constant bodies are
  left to the extremum and constant detectors. Reduce request #2.
- **Products of a value, from whole lists.** `list_factor_fold_hypotheses` reads `fold(lam(lam(g(#0) * #1)), b, xs)` for any
  `g`, with `list_factor_fold_solver`: values that occur only in lists whose output is 0 get the factor 0; least
  squares on `log|output|` gives every other factor's magnitude and the base's, rounded; the output signs give the
  factors' signs by elimination mod 2. The result is checked exactly against every list and must reproduce more than
  half. Needs more lists with a nonzero output than distinct values plus one. The identity (`h * acc`) and a
  constant factor (`c^n`, affine) are left to the specific detectors. Reduce request #3.

The polynomial rules then compress the bodies, at every position, as they do for polynomial fitting.

On integer data the generic and integer map and fold detectors find the same programs (identical costs on every
benchmark problem), so on integer data the pair proposes each program twice. Against the integer detectors alone,
running both made MCTS slower on every problem it spends time on (it detects again at every simulation step), and
made best-first faster on the long product (37 s to 0.9 s), where the generic order of samples suits the
interpolation. The difference is the order the samples reach the element domain.

## Benchmark

`bench/list_bench.cpp`, gated by `make bench_check` with its baseline in `bench/baseline/list.tsv`. 50 problems, 45
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
- reduce requests #2 (largest square) and #3 (product of squares, without tails), and similar problems for the
  generic readings: the smallest of `2h + 1`, the product of `h^2 + 1` from base 3, and two still open (below)
- reduce request #1 (`map(lam(2 * #0 + 1), filter(lam(lt(#0, 17)), xs))` on 30 lists of 20 to 40 elements), and two
  similar long compositions: squares of positives, and `3e - 2` over the values above -10
- reduce request #5 (reverse: the primitive `reverse(xs)`, cost 2), duplicating each element and reverse with each
  element twice (folds that build lists), and reverse with one wrong label
- reduce request #4 (count of evens, `fold(lam(lam(pw(when(eq(mod(#0, 2), 1), #1), 1 + #1))), 0, xs)`, cost 16), and
  the count of multiples of 3 and the sum of the evens; the sum of absolute values and the count of evens on long
  lists are open (below)

Every result is checked against every sample. The soundness fuzz (`list_fit_soundness.cpp`) performs every rewrite
along random walks and checks the same.

## Fold families and how to detect them

Folds over integer lists to integers, grouped by the shape of their step, with the reading that finds each one from
whole lists. In this table and the prose around it, `h` is the head (`#0` in the step's body) and `acc` the
accumulator (`#1`):

| family | step | examples | detection | status |
|---|---|---|---|---|
| affine | `p(h) + c * acc` | sum, length, sum of squares, alternating sum, digits | linear in `p` and `b` once `c` is fixed; exact solve | `list_affine_fold_hypotheses` |
| additive | `g(h) + acc` | count of evens, count of multiples of 3, sum of the evens, sum of absolute values | output is `b` plus a sum of per-value terms: linear in `b` and `g` at each value, an exact solve; `g`'s values go to the element domain (a parity through its residue detector) | `list_sum_fold_hypotheses` (any `g`; needs more lists than distinct values) |
| selection | `pw(when(lt(acc, h), h), acc)` | maximum, minimum | output is an element or the base; direct check | `list_extremum_fold_hypotheses` |
| multiplicative | `q(h) * acc` | product, product of squares, `2^length` | output is `b` times a product of per-value factors: zeros mark the values with `q = 0`; on the rest, each prime's exponent in the output is linear in the per-value exponents (or, cheaper, `log|out|` by least squares, then rounded and checked exactly); signs are linear mod 2. The factors per value then go to the element domain. | `list_product_fold_hypotheses` (linear factors by enumeration), `list_factor_fold_hypotheses` (any `q`, by logs and signs mod 2) |
| counting | `pw(when(c(h), acc + 1), acc)` | count of positives, count above a threshold | output is the number of elements passing a threshold: try the thresholds between element values, as the filter detector does | `list_count_fold_hypotheses` (one threshold); any condition is the additive row with `g` 0 or 1 |
| selection of a value | `pw(when(lt(acc, p(h)), p(h)), acc)` | largest square, largest absolute value | output is `p` of some element: bound `p` at each value by the outputs of the lists holding it; values that alone reach their list's output pin `p` there, and go to the element domain | `list_selection_fold_hypotheses` (open on long lists and for piecewise `p`, below) |
| boolean | `and`/`or` of a condition | all positive, any zero | a counting fold compared with 0 or the length | open (needs booleans) |
| list-valued | `cons(h, acc)`, `append(acc, cons(h, nil))` | reverse, duplicating each element, reverse with each element twice | enumerate the small steps over `cons`, `append`, `nil`, `#0`, `#1`; refute by length (`a * length(acc) + b` elements), then run the rest | `list_listed_fold_hypotheses` (steps up to 7 nodes) |

## Gaps (the open problems)

- **Steps in pieces in general.** The extremum detector covers the largest and smallest element. A step in pieces
  that compares the two variables some other way (`lt(acc, p(h))`, e.g. the largest square) is still out of reach:
  the split detector only cuts on one variable at a threshold.
- **Data without tails, beyond the built families.** Without tails, affine, extremum, linear-factor product and
  single-threshold count steps are found. A factor that is not linear in `h` (`h^2 * acc`) needs the per-value
  factor reading in the table; a count on a condition other than one threshold needs the condition handed to the
  element domain.
- **Detectors' drag.** MCTS no longer repeats detection on an expression it has seen (`detection_memo`), which
  removed most of it: long sums, lengths and sums of squares under MCTS now take well under a second. What remains
  is the first detection of each new expression; the affine detector still tries every multiplier from both starts
  on data it cannot fit, and a necessary-condition filter (solving modulo a small prime before the exact solve)
  would cut that.
- **Composition, beyond a map over a filter.** Other compositions (`filter` over `map`, a fold over a map) need their
  own readings. The map-over-filter readings need `f` to be one-to-one on the kept values for the signature reading
  (`e^2` over both signs merges two values' lists), or a list that keeps or removes all of its values.
- **Enough element values.** A piece of a body needs more distinct element values than it has coefficients, as in
  polynomial fitting.
- **Filters are thresholds.** The filter detectors read only thresholds; parity and other residue conditions reach
  folds (through the sum fold and the element domain's residue detector) but not filters yet.
- **The sum fold needs more lists than distinct values.** Its exact solve has one unknown per value, so 30 long lists
  over 101 values (the open long count of evens) do not pin it. A count would need fewer unknowns: a condition read
  from the lists that differ by one value, or a residue guessed first and checked.
- **Negative leading terms.** The sum of absolute values reaches `pw(when(lt(#0, 1), -1 * #0 + #1), #0 + #1)` (18),
  where `#1 - #0` would make it 16: the polynomial renderer writes terms in variable order, so a negative first term
  costs two nodes (`poly.md`).
- **No list-valued folds.** `reverse` or `append` need a fold whose accumulator is a list.
- **Large expressions are slow to expand.** On the long product, best-first finds `fold(lam(lam(#0 * #1)), 1, xs)` on its first
  expansion (0.03 s), then spends 37 s using the rest of its budget, because every expansion detects at every
  position of a piecewise of about 15000 nodes. A search that stops at a form it cannot improve, or detection that
  skips the data's subterms, would remove it. The order of step samples also matters here (see above).
- **No generic filter yet.** It would hand the element domain data points labelled true or false (kept or removed)
  and take a condition back. It needs boolean values and a condition detector in the element domain.
- **Selection of a value on long lists.** In a long list many values share the bound of the list with the smallest
  output, so few lists have exactly one value at their output, and too few values are pinned to fit `g` (benchmark:
  `long largest square (open)`). Bounds from several lists at once, or a guess of `g` checked against the bounds,
  would pin more.
- **A piecewise inside a condition.** The step `pw(when(lt(acc, g(h)), g(h)), acc)` with a piecewise `g` (the
  largest absolute value) cannot be evaluated: conditions are evaluated by the polynomial point evaluator, which
  does not evaluate piecewises, and the piecewise evaluator depends on the condition evaluator. Breaking that
  dependency (benchmark: `largest absolute value (open)`) is vocabulary work. The pinned values also leave the
  piece boundary undetermined (here between 1 and 5).
- **Products on many distinct values.** The factor reading has one unknown per distinct value, so long lists over
  -50 to 50 (101 values) need more than 102 lists with a nonzero output.
