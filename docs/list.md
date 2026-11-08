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

There are two kinds, following design rule 7 in `docs/extending.md` (generalize beside, not instead of):

- **Generic, for elements of any domain.** They hand the element domain data points of expressions.
  - `list_map_hypotheses`: every list-to-list point whose lists have the same length gives one element data point
    per position, `point(e) -> output element`. Proposes `map(f, xs)`.
  - `list_fold_hypotheses`: the base is the output for `nil`. Every point `cons(h, t)` whose tail `t` is also in the
    data gives a step data point `point(h, output of t) -> output`, whatever the outputs are (numbers, lists).
    Proposes `fold(s, b, xs)`.
- **Integer, used by `list_fit_rules_manifest`.** The same reading for integer elements, with the samples ordered by
  value, plus the threshold filter.
  - `list_integer_map_hypotheses`, `list_integer_fold_hypotheses`.
  - `list_filter_hypotheses`: a point is a filtering when its output is its input with every occurrence of some
    values removed. When every kept value is above every removed one, proposes `filter(lt(d, e), xs)`; when below,
    `filter(lt(e, d), xs)`.

The polynomial rules then compress the bodies, at every position, as they do for polynomial fitting.

On integer data the generic and integer map and fold detectors find the same programs (identical costs on every
benchmark problem), but neither is faster everywhere. Generic was much faster on the long product (best-first
37 s to 0.9 s) and slower with a wrong label in long data (MCTS 13 s to 31 s); the difference is the order the
samples reach the element domain. Wiring both only doubled detection, since they propose the same programs. So the
integer manifest keeps the integer detectors, and the generic ones wait for an element domain that is not integers.

## Benchmark

`bench/list_bench.cpp`, gated by `make bench_check` with its baseline in `bench/baseline/list.tsv`. 26 problems, 22
solved by best-first:

- folds: sum, length, product, sum of squares, alternating sum, digits read as a number, sum over all 781 lists of
  length up to 4, and a sum with one wrong label (it stays as the only branch)
- maps: square, `2e + 1`, absolute value (a piecewise body), and a map with one wrong label
- filters: keep the positives, keep the values below one
- long lists, 20 to 40 elements with values up to 50 in size: sum, length, sum of squares, product (values reach
  about 9^40), a sum with one wrong label, two maps and a filter. Folds are given every list's tails; maps and
  filters need none, so 30 random lists are enough.

Every result is checked against every sample. The soundness fuzz (`list_fit_soundness.cpp`) performs every rewrite
along random walks and checks the same.

## Gaps (the open problems)

- **Maximum.** Its step, `pw(when(lt(acc, h), h), acc)`, compares two variables. The split detector only cuts on
  one variable at a threshold.
- **Data without tails.** The fold detector reads steps off lists whose tails are also in the data. On other data,
  a fold has to be solved for (its step's unknown coefficients through the whole list), not read off.
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
