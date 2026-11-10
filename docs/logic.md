# Logic: the smallest formula for a truth table

The logic domain fits boolean functions: data whose inputs are tuples of bits and whose outputs are bits, reduced
to a small formula (`docs/vision.md`). It was built for reduce requests #9 (parity of 4 bits) and #10 (majority of
3 bits).

```
pw(case(point(0, 0, 0), 0), case(point(1, 0, 0), 0), ..., case(point(1, 1, 1), 1), ?0)
    ->   lams(3, or(and(#2, or(#1, #0)), and(#1, #0)))
```

**Notation.** There are no named variables: inputs and parameters are de Bruijn indices `#k`, and every fitted model
is a closed function of the data piecewise's inputs, `lam(b)` or `lams(k, b)` (`plans.md`, bound variables). Older
examples below that name an input (`x`, `xs`, `n`, `a`) mean its index: one input is `#0`; with k inputs the first is
`#(k-1)` and the last `#0`.

## Modules

- `logic` (`logic_`): the vocabulary, its evaluator, the structural laws (`logic_simplifier`), and the condition
  evaluator that lets any domain's predicates be combined with the connectives (`logic_condition_evaluator`).
- `logic_fit` (`logic_`): the composition. The table reader, the two solvers and their detectors, the rule set, the
  solver manifests and the runtime. It runs on a polynomial kernel for the pool, the literals and the printer.

## Vocabulary

| expression | meaning |
|---|---|
| `0`, `1` | the truth values (integer literals) |
| `a`, `b`, ... | the input variables, in the order each sample lists them |
| `and(x, y)`, `or(x, y)`, `xor(x, y)`, `not(x)` | the connectives |
| `xnor(x, y)`, `nand(x, y)`, `nor(x, y)`, `imp(x, y)`, `maj(x, y, z)`, `mux(s, x, y)` | primitives: equivalence, not-and, not-or, implication, at least two of three, x where s holds else y |

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

- **`logic_primitive_hypotheses`**: one primitive over some of the inputs, each argument plain or negated, checked
  against the rows the data gives, so unseen rows are don't cares: `maj` from six of its eight rows.
- **`logic_decomposition_hypotheses`**: a composition `c(g, x)` of an easy outer combiner (`and`, `or`, `xor`) with
  one input, split outer-inner: assuming c and x, each row says what g must be or leaves it free, and g's partial
  table goes to the primitive and parity detectors. `xor(maj(a, b, c), d)` is read in one step, with rows unseen too.
  One level, kept beside the general reading below.
- **`logic_bidecomposition_hypotheses`**: compositions of any depth. f = c(g(A), h(B)) for `or`, `and` or `xor` and
  two functions of disjoint groups of inputs, each decomposed again, down to leaves (primitives, parities, covers).
  For a split, each test is one pass over the table with unseen rows as don't cares (`or`: g(a) is 1 where every known
  f(a, .) is 1, and every known 1 must be covered; `and` the same with 0s; `xor` by propagating parities over the
  known rows). Every split of the support is tried and the smallest result kept, memoized per table; depth is bounded
  only by the inputs, with a work budget (`k_logic_default_max_decompositions`). `xor(maj(a, b, c), maj(d, e, f))` is
  read from 56 of its 64 rows. The parts may share inputs: f = c(g(A, C), h(B, C)) for a shared set C of at most
  two inputs (`k_logic_default_max_shared_inputs`), with the same tests taken over each part's inputs and C, so
  `xor(maj(a, b, c), and(a, d))` is read through the shared a. Two more outer shapes, at the top and wherever no
  connective splits a part: `mux(v, g, h)` on an input v (g and h are f where v holds and where it fails, Shannon's
  expansion) and `maj(x, y, g)` on two inputs (where they agree f must equal them; g is f where they differ), so
  `mux(a, xor(b, c), and(b, d))` and `maj(a, b, xor(c, d))` are read whole. `logic_fit_hypotheses_manifest` packages these
  detectors for other domains.
- **`logic_condition_hypotheses`**: condition data (a value kept or removed) as a Boolean function of atomic
  predicates from another domain (`num_condition_atoms`: thresholds, residues, primality; any domain's predicates
  can be atoms). One atom, or a connective of two; beyond that, atoms are chosen until every kept value is told apart
  from every removed one (greedily from several starts, and every triple when the greedy choice needs more than three,
  since each atom of an xor separates only half), and the labels over the chosen atoms are fitted as a truth table by
  `logic_fit_hypotheses_manifest`, any depth. This is how list filters take Boolean conditions:
  `and(is_prime(#0), lt(2, #0))` for the odd primes, `maj(...)` and `xor(xor(...), ...)` of three predicates.

## Rules

- **`logic_simplifier`**: the laws of Boolean algebra as structural rewrites, at any width, over any subformulas:
  constants, double negation, the negated forms (`not(and)` is `nand`, ...), De Morgan, idempotence, complement,
  absorption, cancellation in xor chains (parity stays linear), and the expansions of `xor`, `xnor` and `maj`.
  Soundness is fuzzed against truth tables.
- **`logic_formula_refitter`**: reduces a subformula by its meaning, bounded in width
  (`k_logic_default_max_refit_width`, 8 inputs, 256 rows): its truth table is refitted by the parity, cover and primitive detectors, and the smallest
  exact fit is proposed when it shrinks. Holes are don't cares: a row the holes can change is free, so the formula
  becomes the smallest one reachable under any binding (`or(x, ?0)` becomes `1`). It complements the structural rules;
  it is never the only way, since tables are exponential in width.

## Benchmark

`bench/logic_bench.cpp` (`make logic_bench`, gated in `make bench_check`, baseline `bench/baseline/logic.tsv`), costs
as the benchmark counts them (with the closing `lams`):

| problem | cost | form |
|---|---|---|
| #9 parity of 4 bits, parity of 3 and of 6 | 9, 7, 13 | xor chains |
| #10 majority of 3 bits, and with two rows unseen | 6 | `maj(#2, #1, #0)` |
| equivalence, implication, nand, nor | 5 | `xnor`, `imp`, `nand`, `nor` of two |
| nand of two of three, two rows unseen | 5 | `nand(#2, #1)` |
| majority of three of five inputs | 6 | `maj(#4, #2, #0)` |
| xor of a majority and a fourth, also with three rows unseen | 8 | `xor(maj(#3, #2, #1), #0)` |
| and of an equivalence and a third | 7 | `and(xnor(#2, #1), #0)` |
| multiplexer | 10 | `or(and(s, a), and(not(s), b))` |
| parity of 4, wrong label | 17 | the parity plus one `case` |
| majority of 5 bits | 29 | was 39; uses `maj` twice, still no sharing |
| xnor of an and and an or | 9 | `xor(and(#3, #2), nor(#1, #0))` |
| two levels over six inputs | 12 | `or(xor(#5, #4), and(#3, maj(#2, #1, #0)))` |
| xor of two majorities, also with eight rows unseen | 11 | `xor(maj(#5, #4, #3), maj(#2, #1, #0))` |
| three levels over six inputs | 13 | `and(or(xor(#5, #4), #3), imp(#2, xor(#1, #0)))` |

Every problem takes milliseconds except parity of 6 bits (0.3 s): the refit tables a 6-input formula's subformulas
at 64 rows each.

## Gaps (the open problems)

- **Holes as missing pieces.** The refit binds holes to whatever is smallest, so `or(and(not(#1), #0), ?0)` becomes
  `1`, not the `xor(#1, #0)` reduce request #19 asks for. Recovering "the rest of a shape" needs an objective that
  keeps the known part's structure (completion against primitive expansions), to be decided.
- **Outer shapes over subfunctions**: `maj(g, h, k)` with three composite arguments, and `mux(s, g, h)` whose
  selector is itself a formula, are not read. Shared sets are at most two inputs.
- **Shared subformulas.** Majority of 5 repeats `maj(c, d, e)`; sharing it needs helpers (`plans.md`, abstraction).
- **Exact minimization** for small formulas; the cover is greedy.
- **Algebraic normal form** (xor of ands, the GF(2) polynomial) as a second normal form beside the cover: parities
  are linear there and sums of products are exponential, and the reverse holds for ors, so each catches what the other
  cannot.
- **Wide formulas**: the refit stops at 8 inputs; structural rules are the only reduction beyond.
- **Several guarded branches**: `pw_guarded_hypotheses` reads one guarded branch over a default; data with two or
  more differently guarded branches is read only where splits by thresholds or residues apply. Separating sets beyond
  three atoms rely on the greedy choice.
