# Roadmap: from a checkpoint to "throw a dart at it"

The goal (`vision.md`): put in anything, as an expression to reduce or as data,
`pw(case(input, output), ..., ?0)`, with arbitrary functors and holes anywhere, and get back the smallest model that
means the same, across algebra, polynomials, trigonometry, calculus, Boolean algebra and programs. Recovery should
succeed on nearly every problem drawn at random from what the vocabulary can express: aim for 100%, measure how
close we are. MANR is also useful without the piecewise, as a reducer of arbitrary symbolic expressions. Both are
the same machine.

This document records where we stand (the checkpoint) and the large pieces that stand between here and there, in
the order they unlock each other. `plans.md` holds the detailed designs. The issues and the workflows (scout, forge,
showcase) drive the work one step at a time.

## Checkpoint (2026-11-09)

**What exists.** A domain-free core: hash-consed expressions, rule plumbing, best-first and MCTS search, the
piecewise algebra with extension binding, and abstraction (`lam`, `lams`, `app`, `let`, de Bruijn indices, a
reducer). Domains are integer polynomials (normal-form oracle, factoring, Horner, fitting by interpolation, regions,
residues, iteration with a helper), integer sequences (recurrences of order 1 to 3, closing to powers, bases solved
backwards), lists (map, filter, fold, append, reverse, with decomposed detectors), logic (parity, factored covers),
and number theory (`prime`, `is_prime`). Holes in data labels are matched and bound. 317 tests; the benchmarks hold
about 230 problems over five corpora; 53 showcase problems are gated.

**Where it is strong.** Fitting data in one domain, when a detector reads the model's shape: polynomials and
piecewise polynomials with noise and holes, recurrences from partial data, one-level list programs, small truth
tables. Reducing polynomial expressions: cancellation, factoring, perfect powers.

**Where it is weak** (each found by experiment, at this checkpoint):

| gap | example | today |
|---|---|---|
| holes in expressions | `?0 + ?0`; `(?0 + 1)^2 - ?0^2` | both should reduce to a literal (`0` with `?0 = 0`; `1`, #17); today nothing binds a hole, so they stay or only simplify around it |
| unknown functors | data `n -> g(n^2 + 1)` | no reduction at all: nothing reads structure it has no vocabulary for |
| formulas without data | `not(not(x))`, `and(x, or(x, y))`, De Morgan | unchanged: logic has detectors over data, no rules |
| composition across domains | sum of the primes in a list; squares of the primes; `map(f, reverse(xs))` | unsolved: each detector hands inner functions to polynomial fitting only |
| shared subcomputations | majority of 5 (cost 39, `maj(c, d, e)` twice) | no common-subexpression helpers |
| exact minimization | at least 2 of 4 (19, a 17 exists) | greedy covers |
| numbers beyond integers | rationals, division, reals | none |
| whole domains | calculus, trigonometry, sets, strings, graphs | none |
| text input | reduce requests with `?n`, `pw`, `lam` | built in C++ by hand |

## The large pieces

### 1. Domain-free reduction machinery (unlocks everything else)

- **Anti-unification (structure over arbitrary functors).** The entry point for "data of anything". Given the
  outputs of a piecewise, compute their least general generalization: the common skeleton, with a hole wherever the
  outputs differ (`g(1), g(2), g(5), g(10)` gives `g(□)`). Each hole becomes a new data set, `key -> the hole's
  value`, fitted recursively by every domain (`□ = #0^2 + 1`), and the model is the skeleton over the fits:
  `lam(g(#0^2 + 1))`. It needs no knowledge of `g`. Applied to inputs as well as outputs, it finds models that
  rebuild their input (`f(x, y) -> f(y, x)`). Variants: anti-unify within regions found by a split, and across the
  arguments of one functor.
- **One hypothesis service, recursive and budgeted.** Today a detector that reads a shape (a fold, a filter, a
  recurrence) hands the inner function's data to polynomial fitting alone. Replace that with one service any
  detector can call: "propose models for these points", answered by every domain's detectors, recursively, with a
  depth and size budget and memoized by the data's signature. That one change gives the sum fold an `is_prime`
  condition, the filter a mapped body, and anti-unification its sub-fits. Composition across domains is this service
  and nothing else.
- **Semantic re-fitting (expression to data to model).** Any expression whose inputs range over a finite or
  sampleable domain can be reduced by evaluating it, fitting the values with the detectors, and checking the result
  exactly (exhaustively on a finite domain like Booleans; by the normal-form oracle where a domain has one). This
  turns every data detector into a simplifier: logic formulas reduce today, without a single logic rule, and list
  programs can be refit from their own behavior. Run it as a detector, so it competes with local rules on size.
- **Laws as data, rules generated from them.** Most local rules are instances of a few laws: identity, absorbing
  element, idempotence, involution, commutativity, associativity, distributivity, absorption, De Morgan. Let a domain
  declare its laws in a table, and generate the rewrites, AC-normalization and factoring from it. Boolean algebra,
  sets, max/min, gcd and new domains get their local rules by declaration. Laws with conditions
  (`x / x = 1` when `x != 0`) carry their side condition.
- **Equality saturation as a search.** For pure expression reduction, an e-graph applies every rule at once and
  extracts the smallest form, which beats sequential search on identities that need rewrites that grow before they
  shrink (the two-squares identity needs MCTS today). Add an e-graph search manifest beside best-first and MCTS,
  fed by the same rules, with detectors as e-graph analyses.
- **Common subexpressions and helpers.** Detect repeated subterms (and repeated connected cuts of the compute graph,
  `plans.md`) and abstract them with `let`, firing when it shrinks. Several helpers in parallel form a compute
  graph: this is how a large model stays small. Majority of 5 is the first benchmark problem for it.

### 2. Free variables everywhere

Holes are part of every expression, so every domain needs to reduce around them and choose them.

A hole is not an opaque variable: reduction chooses its value, so an expression holding holes reduces to the
smallest expression reachable under any binding. `?0 + ?0` reduces to `0`, not to `2 * ?0`.

- **Binding rules, the core of it.** A binding rewrite substitutes `?n` everywhere in the whole expression (one
  hole, one binding), beside the local rewrites. Detectors propose the bindings that shrink the whole: the identity
  or absorbing element of the operator a hole sits in; a value that makes a subterm a literal (`?0 + ?0` with
  `?0 = 0`); in a normal form, the coefficients a hole can make vanish (`(?0 + 1)^2 - ?0^2` with `?0 = 0` gives `1`);
  the missing half of a known shape (`or(and(not(x), y), ?0)` completed to `xor(x, y)`, #19, by Shannon expansion
  over the formula's truth table); a value that makes a subterm a literal.
- **Holes in data keys and open structures.** A key holding a hole (#20) is solved by evaluating the model with the
  hole as an unknown and solving the output equation in the domain (`?2^2 = 16`). An open-ended list
  `cons(..., ?0)` (#16) needs vocabulary for unbounded lists (an `unfold` or generator) and a detector that binds the
  tail to it.
- **Domain hole solvers.** Generalize `arith_hole_solver` (linear, one hole) to polynomial equations in one hole
  (integer roots), systems in several, list and tree matching, and Boolean constraint solving.

### 3. Numbers and Booleans as the shared substrate

Almost every domain needs both, so they come before the new domains.

- **Rationals and division**, then rational functions (a normal form of a polynomial over a polynomial), integer
  division, `mod`, `gcd`, `abs`, `min` and `max` as functors with laws. Real-valued data needs exact rationals before
  any notion of tolerance.
- **Booleans in every domain.** Comparisons as Boolean-valued functors, Boolean connectives over them, conditions in
  regions and filters drawn from the full Boolean vocabulary (not only `lt` and residues), and the logic domain's
  minimization applied to conditions. Exact minimization for small formulas (exhaustive search under a size bound),
  with the greedy cover beyond it.

### 4. New domains

Each follows `vision.md`: vocabulary, evaluator, detectors, local rules (from laws where possible), a soundness
oracle, a benchmark.

- **Calculus.** Differentiation is pure rewriting (sum, product, chain, power rules) over the existing vocabulary,
  and the derivative is a soundness oracle for integration: an antiderivative is proposed by fitting (guess a form,
  fit its coefficients) and checked by differentiating. Limits and series later. Data: pairs `f -> f'` learn the
  operator itself.
- **Trigonometry.** `sin`, `cos` with exact values at rational multiples of pi, and identities through a normal form
  (rewrite to exponentials, or Chebyshev polynomials in one angle), so `sin(x)^2 + cos(x)^2` reduces to `1` and angle
  sums expand or collapse. Periodic integer data (sign patterns, residues) connects it to fitting.
- **Programs, deeper.** Nested folds, `zip`, `scan`, sorting, `take`, `drop`, indexing, recursion schemes, strings as
  lists, tuples and records, and models that are several cooperating helpers.
- **Sets, graphs, algebraic structures** (`vision.md`), each as the dart-board finds them missing.

### 5. Front end and measurement

- **A parser for the whole syntax**: `?n`, `#k`, `pw`, `case`, `when`, `point`, `lam`, `lams`, `let`, lists, every
  domain's functors. Reduce requests, benchmarks and users then write problems as text.
- **The dart board: a random-problem generator.** Draw models at random from a grammar over the supported
  vocabulary (with sizes, depths, domains and hole rates as parameters), sample data from them (with noise and holes
  as options), and measure the share recovered at or below the model's size. That share is the number this roadmap
  is aimed at: it is what "throw a dart and it solves it" means, and it says where the next weak spot is. It also
  generates expressions to reduce (a model plus a semantics-preserving scramble). Run it in the showcase, track it
  over time, and feed its failures to scout.

## Order

1. **Foundations.** The parser; the hypothesis service; anti-unification; semantic re-fitting; binding rules for
   holes; the dart board, so every later step is measured. These are domain-free and each
   lifts every domain at once.
2. **Substrate.** Laws as data (Boolean algebra first, then max/min/gcd), Booleans in every domain, rationals and
   division, common-subexpression helpers.
3. **Search.** Equality saturation for expression reduction; budgets and caching tuned against the dart board.
4. **Domains.** Calculus, trigonometry, deeper programs, then sets, strings, graphs.
5. **Real data.** Tolerance and noise models, floating-point inputs read as exact rationals, the associative-model
   usage (`vision.md`).

Each step lands the usual way: a benchmark problem first, a fix that generalizes, the gate, the showcase.
