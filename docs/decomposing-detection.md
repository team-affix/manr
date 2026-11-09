# Decomposing detection

Detecting a program shape in general is hard. "This data is some fold" leaves the step, the base and their
interaction all unknown, and the data constrains the step only through many applications of it. No single detector
solves that. MANR's way forward is to **split a hard detection problem into subproblems, each made easy by one
structural assumption**, solve each with its own detector, and keep splitting what is still open. This document is the
method, with the fold detectors as the worked example. Read it before adding a detector for a new shape.

`rule-design.md` says how to write one detector cheaply. `extending.md` says how to add it without regressing. This
document says which detectors to write.

## 1. A subproblem is an assumption that makes the data readable

Each subproblem assumes something about the program (or the data) that turns the unknowns into something the data
shows directly, or into a problem with a known exact method. A good assumption is cheap to test, so the detector can
reject data that breaks it in a few operations.

The fold families so far (`docs/list.md` keeps the full table and its status):

| subproblem | assumption | what it makes easy | technique | detector |
|---|---|---|---|---|
| folds with tails | the data holds each list's tail | every `(h, acc) -> out` step is visible as a point | read the step's points, hand them to the step domain | `list_fold_hypotheses`, `list_integer_fold_hypotheses` |
| affine folds | step is `p(h) + c * acc` | once `c` is fixed, the output is linear in `p`'s coefficients and `b` | try small `c`, solve exactly over the rationals | `list_affine_fold_hypotheses` |
| extremum folds | step keeps the larger (or smaller) of `h` and `acc` | the output is one of the list's elements, or the base | direct check, linear in the data | `list_extremum_fold_hypotheses` |
| product folds | step is `(a * h + k) * acc` | the output is `b` times a known product once the factor is fixed | try small factors, read `b` by one division | `list_product_fold_hypotheses` |
| count folds | step adds 1 when `h` passes a threshold | the output less the count is constant | try each element value as the threshold, binary search on sorted lists | `list_count_fold_hypotheses` |

Notice the pattern: none of these detects "a fold". Each detects a family where one assumption does almost all the
work, and then the rest is a check or a small exact solve. The assumption can be on the data too: "folds with tails"
assumes nothing about the step, only that the tails are present.

## 2. Make each subproblem's solution as generic as possible

An assumption can fix too much. The product detector enumerates factors `a * h + k` with `a` and `k` small: that
solves products, and nothing else in its family. A better split fixes only the part that makes the problem easy and
**hands the rest to another detector as data**, through `IProposeHypotheses`. The leftover then gets everything the
other domain can do, now and as it grows.

### The outer-inner split

The most useful form: assume the step is an **easy outer combiner `f` applied to an unknown inner function `g`** of
the head, `step(h, acc) = f(g(h), acc)`. `f` comes from a small set, each with its own easy reading. `g` stays fully
open. The reading of `f` turns the data into points `(element value, g(element value))`, and those points go to the
element domain's hypotheses (polynomial fitting today; any domain's detectors later).

| outer `f` | the output is | how `f`'s reading recovers `g`'s values | examples, by `g` |
|---|---|---|---|
| `g(h) + acc` | `b + sum g(x_i)` | linear in the unknowns `g(v)`, one per distinct element value `v`: an exact solve | sum (`g = h`), sum of squares (`h^2`), sum of absolute values (a piecewise `g`) |
| `g(h) * acc` | `b * prod g(x_i)` | zero outputs mark the values with `g(v) = 0`; on the rest each prime's exponent is linear in per-value exponents (or `log|out|` by least squares, rounded, then checked exactly); signs are linear mod 2 | product (`h`), product of squares (`h^2`), `2^length` (`g = 2`) |
| `max(g(h), acc)` | `max(b, max g(x_i))` | each output is `g` of one of its list's elements: the element values that attain it are the candidates | maximum (`h`), largest square (`h^2`), largest absolute value |
| `acc + [c(h)]` | `b + count of x_i where c(x_i)` | counts of each value are linear in the unknowns `[c(v)]` (0 or 1): an exact solve, then a condition fit | count of positives, count of evens |

Each row is one detector, generic in `g`. Compare with the current detectors. The affine detector fixes `g` to a
polynomial of bounded degree. The extremum detector fixes `g = h`. The product detector fixes `g` to `a * h + k`. The
count detector fixes the condition to a threshold. Each is a specific case of its row. **They stay** (design rule 7
in `extending.md`): they are fast on common data, and the generic row is added beside them, not instead. The `max`
and `*` rows are built (`list_selection_fold_hypotheses`, `list_factor_fold_hypotheses`), and so is `+`, which covers
the count on any condition (`list_sum_fold_hypotheses`, with the element domain's residue detector reading a
parity).

What makes the split work:

- **`f` has an easy reading.** Its effect on the output is linear, multiplicative, a selection or a count, so the
  values of `g` can be read back exactly, or narrowed to a few candidates and checked.
- **The leftover is in a domain that already has detectors.** `g(v)` at each distinct element value is ordinary
  point data, which is exactly what `IProposeHypotheses` takes.
- **The number of unknowns is the number of distinct element values**, not the number of coefficients of some
  assumed form. That needs enough lists relative to distinct values (one more, to check), and that is a cheap
  necessary condition to test first.

### Other splits of the same kind

- **Selection of a value** (`max(g(h), acc)`) is the outer-inner split with `f = max`. Its hard part is that we don't
  know which element attains each output. The reading narrows it: the candidates are the elements `v` of the list
  whose `g(v)` could equal the output. Lists of one distinct value pin `g(v)` exactly and seed the rest.
- **Composition of programs**, `map(lam(f), filter(lam(c), xs))`, splits the same way one level up. The outer program is
  `map`, so the output's length is at most the input's and each output element is `f` of some input element. When
  the outputs are an order-preserving image of a subsequence, the subsequence gives `c`'s data (kept or removed per
  value) and the pairs give `f`'s data, each handed to the element domain.
- **Booleans** (`all`, `any`) are counts compared with 0 or with the length: the count reading plus one comparison,
  once the language has true/false values.

## 3. Choosing the next subproblem

Pick splits in this order:

1. **Common in real data.** The families of the benchmark and of the examples in `docs/vision.md` come first.
2. **The assumption is cheap to test** before any exact work (`rule-design.md`): a parity, a sign, a divisibility,
   the count of distinct values against the count of lists.
3. **The leftover goes to an existing domain's detectors.** A split that leaves an unknown nothing else can read is
   only half a detector.
4. **It should fire whenever it can** (design rule 3 in `extending.md`). When its assumption holds and it reproduces
   most of the data, its proposal is almost always the right move, so it adds little choice to the search.

And for each new detector:

- **Write the problems first**, in the domain's benchmark, including one with a wrong label and one without tails
  where that applies (`extending.md`, "The loop for a new capability").
- **Record the family** in the domain doc's table (for lists, `docs/list.md`, "Fold families"): the step, the
  examples, the reading, and whether it is built, specific or generic.
- **Name the specific case it generalizes**, if any, and keep that detector. Compare both in the gate.

## 4. Anti-patterns

- **One general detector for the whole problem.** "Any fold" by search over step expressions is the problem we are
  decomposing, not a solution. It dilutes the search and finds common cases slowly.
- **Only enumeration.** Enumerating a fixed small family (factors `a * h + k`, multipliers up to 10) is a fine
  specific case. It is not the end of a family: the generic version recovers the inner function's values and
  delegates them.
- **Replacing the specific with the generic.** Generalize beside, not instead of (design rule 7).
- **A split whose assumption is checked last.** Test the assumption that defines the subproblem first, so data
  outside the family is rejected before any solve.
