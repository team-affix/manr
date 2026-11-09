# Logic: the smallest formula for a truth table

The logic domain fits boolean functions: data whose inputs are tuples of bits and whose outputs are bits, reduced
to a small formula (`docs/vision.md`). It was built for reduce requests #9 (parity of 4 bits) and #10 (majority of
3 bits).

```
pw(case(point(0, 0, 0), 0), case(point(1, 0, 0), 0), ..., case(point(1, 1, 1), 1), ?0)
    ->   or(and(a, or(b, c)), and(b, c))
```

## Modules

- `logic` (`logic_`): the vocabulary and its evaluator.
- `logic_fit` (`logic_`): the composition. The table reader, the two solvers and their detectors, the rule set, the
  solver manifests and the runtime. It runs on a polynomial kernel for the pool, the literals and the printer.

## Vocabulary

| expression | meaning |
|---|---|
| `0`, `1` | the truth values (integer literals) |
| `a`, `b`, ... | the input variables, in the order each sample lists them |
| `and(x, y)`, `or(x, y)`, `xor(x, y)`, `not(x)` | the connectives |

`logic_evaluator` gives a formula's value at `point(v1, ..., vk)` as the literal 0 or 1. As a region condition, a
formula holds where it is 1, so a piecewise can split on any formula.

## Detectors

Both read the data as a truth table (`logic_table_reader`). The reader refutes any data that is not 0s and 1s
before a solver runs.

- **`logic_parity_hypotheses`**: xor is linear over GF(2), so a parity (the xor of some variables, possibly negated)
  is an exact solve mod 2, by elimination on bit sets (`logic_parity_solver`). An inconsistent system refutes it.
  When it is inconsistent, each row is left out in turn, so a parity with one wrong label is still found and the
  binder keeps that row as a branch. This solves #9 in one step: `xor(xor(xor(a, b), c), d)`, cost 7.
- **`logic_cover_hypotheses`**: a small sum of products by Quine-McCluskey (`logic_cover_solver`): prime implicants
  by merging, every essential prime, then greedy cover. Inputs the data does not give are don't cares, so a cover
  generalizes over them. The detector covers the 1s, and also the 0s, which it then negates. It factors each cover as
  it renders it: the literal shared by the most products is pulled out, recursively. Majority's cover `ab + ac + bc`
  becomes `or(and(a, or(b, c)), and(b, c))`, cost 9 (#10). The cover is capped at 8 variables
  (`k_logic_default_max_cover_width`).

There are no local rules yet: the detectors propose finished formulas.

## Benchmark

`bench/logic_bench.cpp` (`make logic_bench`, gated in `make bench_check`, baseline `bench/baseline/logic.tsv`): 12
problems, all solved by both solvers in milliseconds.

| problem | target | form |
|---|---|---|
| #9 parity of 4 bits | 7 | `xor(xor(xor(a, b), c), d)` |
| #10 majority of 3 bits | 9 | `or(and(a, or(b, c)), and(b, c))` |
| parity of 3 and of 6 bits | 5, 11 | xor chains |
| equivalence | 4 | `not(xor(a, b))` |
| implication | 4 | `or(not(a), b)` |
| and of 3 bits | 5 | `and(and(a, b), c)` |
| multiplexer | 8 | `or(and(s, a), and(not(s), b))` |
| majority, two rows unseen | 9 | reaches 6: the don't cares allow a smaller formula |
| parity of 4, wrong label | 15 | the parity plus one `case` |
| exactly one of 3 | 12 | `pw(case(point(1, 1, 1), 0), xor(xor(a, b), c))` |
| majority of 5 bits | 37 | the factored cover of the ten triples |

## Gaps (the open problems)

- **Shared subformulas.** Majority of 5 is cost 37 because `maj(c, d, e)` appears twice. Sharing it needs helpers
  (`plans.md`, abstraction).
- **Exact minimization.** The cover is greedy and factoring is one heuristic. Neither is the smallest formula in
  general. Mixed forms like `and(xor(...), not(...))` are found only by luck (exactly one of 3 reaches 12 as a parity
  with an exception).
- **Local rules**: absorption, De Morgan, double negation, distribution, for formulas that come from elsewhere.
- **More than 8 variables** for the cover, and wrong labels in data that is not a parity.
- **Booleans in other domains**: list conditions (`all`, `any`, #4) and region conditions could use this vocabulary.
