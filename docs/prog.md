# Generic programs (`modules/prog`, `solvers/prog_fit`)

Programs over data of any shape, recovered from samples. A sample is text (`prog_value_parser`): integers, lists
`[1, [2, 3]]`, trees `node(leaf, 4, leaf)`, records `edge(0, 1)`, a tuple of inputs `point(xs, x)`, and holes `?k`. The
samples become a piecewise with an open catch-all, as in every fitting solver, and the catch-all is bound to the
smallest program that reproduces them. Started by the scout run on generic algorithms (#56 to #71).

## Vocabulary (`prog_symbols`)

| kind | expressions |
|---|---|
| integers (1 and 0 as true and false) | `add`, `sub`, `mul`, `pow`, `mod`, `max`, `min`, `lt`, `le`, `eq` (structural, any values), `and`, `or`, `not`, `pw(when(c, v), d)` |
| lists | `nil`, `cons`, `head`, `tail`, `length`, `reverse`, `append`, `sort`, `unique`, `elem`, `map(f, xs)`, `filter(p, xs)`, `fold(f, b, xs)` (right fold, `#1` the accumulator, `#0` the head), `scan(f, b, xs)` (left scan, the running values), `zipwith(f, xs, ys)` |
| trees | `leaf`, `node(l, v, r)`, `tfold(f, b, t)` (`#2` the folded left, `#1` the value, `#0` the folded right) |
| records | any other constructor is data; `field(i, r)` is its i-th argument |
| graphs | a list of edges (two-field records); `reachable(g, s)`, the sorted nodes reached from s |
| functions | `lam`, `lams`, `app(f, x)`, `let(v, b)` |

`sort`, `unique` and `reachable` are primitives because they are among the most common operations of their kind; the
fold forms remain valid, larger readings.

## The interpreter (`prog_interpreter`)

Runs a program on an environment of values (a persistent chain of frames; closures share it uncopied). A function
model is applied to an input: `lams(k, b)` to `point(a1, ..., ak)` (the first `#(k-1)`, the last `#0`), `lam(b)` to
any value. Closures are curried, so `lam(lam(s))` takes two arguments. Arithmetic on a value holding a hole stays
symbolic (`add(?1, 1)`), so the piecewise hole matcher solves it (a tree sum with an unknown value); a comparison or a
structural step on one has no value. Steps are bounded (`max_steps`).

## The enumerator (`prog_enumerative_hypotheses`)

The one detector: the smallest program of the grammar that reproduces every sample, bottom-up by size.

- **Types** are read from the values (integer, list of integers, list of lists, tree, record, list of records), and
  the grammar is typed: each production has a result type and argument types, lambda arguments a context of their own
  (the enclosing variables plus the parameters), two lambdas deep at most (a filter in a fold's step, a fold in a
  map's body).
- **Observational equivalence**: a term is kept only when its values on its context's probe environments differ from
  every smaller term's. At the top they are the samples' inputs (24 at most; a candidate that matches them is checked
  on the rest); a lambda's parameters range over values pooled from the data.
- **Bounds**: body sizes (8 nodes for one parameter, 5 for more, less nested), base sizes (2), banks per type and size
  (2000, filled in grammar order: higher-order programs and list primitives first, arithmetic last), and a term
  budget. Commutative operations are built in one order; operations on literals alone are skipped. Results are
  memoized per sample set, and at most three sets are searched (the piecewise rules ask again for subsets).
- **Deductions** read larger programs backwards from the outputs, over the terms built so far: `out = A op B` (+, -,
  *: B's values looked up), `map`, `zipwith` and `scan` over small list terms (the body found by evaluating the body
  bank on examples the outputs give), `fold` and `tfold` over the inputs (the base from the empty samples, the body
  screened on one-element lists or nodes over two leaves), and folds of computed lists. The smallest candidate wins.

## Composition: decomposing the data (`prog_decomposing_hypotheses`)

The enumerator finds programs of about 15 nodes. Programs of many steps are found by splitting the problem instead
of searching a larger space: a specification (`prog_spec`: environments, outputs, and the relation a value must have
to its output) is solved recursively, each step read off the outputs, so a program's depth is bounded by a depth limit
and a budget of searches, not by the enumerator's size. Three kinds of step:

- **Inverted steps.** When the outputs determine a step's input, the step is peeled and the rest is a new
  specification: `reverse` (the outputs reversed); `sort` and `sort(unique(..))` (sorted outputs: the rest need only
  be a permutation, or the same set, of them); `map(f, L)` (an input as long as every output: f's specification is
  each element to its image, at the environment plus that element); `filter(p, L)` (a source holding each output as
  a subsequence, an input or the smallest program that does: p's specification is each element to whether it was
  kept, any nonzero value meaning kept); `append(A, B)` (the outputs split at an input's length); a tree map (an input
  tree of each output's shape: f's specification is each node's value to its image).
- **Frames with a hole.** When the outputs do not determine the step's input, the step is a frame and the function
  inside it a hole, enumerated in the hole's own scope (`prog_hole`, the enumerator's `fill_hole`) and checked through
  the frame: a count `length(filter(lam(□), L))`, a sum `fold(lam(lam(#1 + □)), 0, L)`, a tree sum, and under a
  `sort` (where the order is lost) `map(lam(□), L)` and `filter(lam(□), L)`.
- **Helpers.** An output that combines one function of two inputs is `let(lam(g), c(app(#0, a), app(#0, b)))`, for c
  an arithmetic operation or `append`, and g one hole (or a sum or count of one) filled once for both uses, so g is
  searched at its own size instead of twice its size. A fragment that recurs in any found program is named as a
  helper afterwards by `abs_helper_abstractor` (wired into `prog_fit`), when that shrinks it.

The specification at the top goes to the enumerator's own hypothesis source; below the top, each specification is
first given to the enumerator (`find_program`, with a small budget), then split. Splits are memoized per
specification (a specification is not split inside itself, so a reverse of a reverse stops), and every result is
checked on every sample.

## Benchmark

`prog_bench`: the scout problems #56 to #71 and variants (descending sort, running maximum, prefix products, tree size
and pre-order, sublist lengths, in-degree), holes (an input element of a sort, an output of a prefix sum, a tree value,
an inner list of a flatten), controls (a sum, random labels that must stay cases), under both solvers.

The composition group (27 problems, held out when the decomposer was designed, then kept): one step with a body of
five nodes (count the evens), two to four steps (doubled then kept above 5, reversed prefix sums, sorted distinct
squares, reversed incremented evens, sorted squares of each sublist), lists of lists (map a reverse, drop the empty
sublists), helpers used twice (`sumsq(xs) - sumsq(ys)`, the sum of the positives of two lists, the sorted squares of
two lists appended), tree programs (sum of squares, add one to every value, sorted values). 24 are solved; open are
the range of two lists added (the helper `max - min` is 8 nodes and its search runs out), the squares of the reversed
positives (a filter under a map under a reverse: the kept elements are read from the images, which repeat), and the
largest square (11 nodes found where 9 exist).

## Gaps (the open problems)

- **Steps that are not inverted.** A map over a filtered list under a `sort` (two holes), arithmetic on two unknown
  parts (`out = A + B` with neither in a bank), and helpers over one input (`g(xs) + g(reverse(xs))`) are not split.
  A real answer is a general inverse per primitive (a witness function for every argument of every operation), so
  any program the vocabulary writes can be read top-down.
- **Larger programs**: the dot product `fold(add, 0, zipwith(mul, xs, ys))` (17 nodes) is past the search; remove
  duplicates as a fold (15) needs a 9-node step.
- **Holes bound to structures** from partial matches: `[[1, 2], ?1, [5]] -> [1, 2, 4, 5]` needs `?1 = [4]`, which no
  filler proposes (fillers are 0, 1 and the parts of the label).
- **Search cost**: a search that finds nothing takes up to about 45 s, and a composition up to two minutes (each split
  is a search of its own); the grammar order and caps decide what is found. A split is searched only for programs
  smaller than the best so far.
- **General recursion and fixpoints** beyond the folds and `reachable`.
