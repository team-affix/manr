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

## Benchmark

`prog_bench`: the scout problems #56 to #71 and variants (descending sort, running maximum, prefix products, tree size
and pre-order, sublist lengths, in-degree), holes (an input element of a sort, an output of a prefix sum, a tree value,
an inner list of a flatten), controls (a sum, random labels that must stay cases), under both solvers.

## Gaps (the open problems)

- **Larger programs**: the dot product `fold(add, 0, zipwith(mul, xs, ys))` (17 nodes) is past the search; remove
  duplicates as a fold (15) needs a 9-node step.
- **Holes bound to structures** from partial matches: `[[1, 2], ?1, [5]] -> [1, 2, 4, 5]` needs `?1 = [4]`, which no
  filler proposes (fillers are 0, 1 and the parts of the label).
- **Search cost**: a search that finds nothing takes up to about 45 s; the grammar order and caps decide what is found.
- **General recursion and fixpoints** beyond the folds and `reachable`.
