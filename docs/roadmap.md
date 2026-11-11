# Roadmap: from a checkpoint to "throw a dart at it"

The goal (`vision.md`): put in anything, as an expression to reduce or as data,
`pw(case(input, output), ..., ?0)`, with arbitrary functors and holes anywhere, and get back the smallest model that
means the same, across algebra, polynomials, trigonometry, calculus, Boolean algebra and programs. Recovery should
succeed on nearly every problem drawn at random from what the vocabulary can express: aim for 100%, measure how
close we are. MANR is also useful without the piecewise, as a reducer of arbitrary symbolic expressions. Both are
the same machine.

Two layers get there. The **fast layer** is what MANR is today: eager detectors and special-case rules, one per
recognizable shape (is it a polynomial, a fold, a parity, a piecewise?), each reaching a far-away result in one step.
The **complete layer** (piece 6) is generic relations and an exhaustive search behind them, which guarantees that the
minimal form is eventually found. Special cases are there to make it lightning fast; the complete layer is there so
that nothing reachable is missed.

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
| holes in expressions | `?0 + ?0`; `(?0 + 1)^2 - ?0^2` | polynomials and logic bind them (`poly_hole_binder`, the logic refit); lists, sequences and conditions do not |
| unknown functors | data `n -> g(n^2 + 1)` | no reduction at all: nothing reads structure it has no vocabulary for |
| formulas without data | `not(not(x))`, `and(x, or(x, y))`, De Morgan | solved: structural laws at any width and a truth-table refit (`logic.md`) |
| composition across domains | sum of the primes in a list; squares of the primes; `map(f, reverse(xs))` | unsolved: each detector hands inner functions to polynomial fitting only |
| shared subcomputations | majority of 5 (cost 39, `maj(c, d, e)` twice) | no common-subexpression helpers |
| exact minimization | at least 2 of 4 (19, a 17 exists) | greedy covers |
| numbers beyond integers | rationals, division, reals | none |
| whole domains | calculus, trigonometry, sets, strings, graphs | none |
| text input | reduce requests with `pw`, `lam`, lists, logic | polynomial text parses `?n`; the rest is built in C++ by hand |

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
- **Holes in data keys and open structures.** A key holding a hole is solved (#20, #24): the model evaluated with
  the hole as an unknown gives a residual, and the output equation is solved in the domain (`?2^2 = 16`). An
  open-ended list `cons(..., ?0)` (#16) is read as the infinite list of a sequence (`tabulate`, `unfold`); list
  programs over infinite lists are not evaluated yet.
- **Domain hole solvers.** `arith_hole_solver` solves polynomial equations in one hole (integer roots), the others
  set to 0 or 1 (#22, #23). Still missing: systems of several equations, list and tree matching, and Boolean
  constraint solving.

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

### 6. Completeness: generic relations and a guarantee (held for later)

The detectors and rules are special cases, and so is much of how holes are bound: each binder guesses bindings for
the shapes it knows (a root of a label polynomial, a completed square, a list hole bound to `nil`, a filler taken
from a label). Between them lies a large set of reduced forms that are correct and smaller but **unreachable**: no
detector proposes them, no binder guesses their binding, and the rules only shrink, so a path that must first expand
(distribute, introduce a helper, rewrite into a form that does not look better yet) is never walked.

**The guarantee we want.** Put in any data or expression over the functors MANR knows, holes anywhere, and the
search eventually returns the minimal form under those primitives: anytime (the best so far is always sound), and
complete in the limit. The special cases stay (`keep-specific-detectors`); they become accelerators of a search that
would get there anyway, not the only roads.

**Ingredients** (not designed yet):

- **Generic relations, both ways.** Equations per functor (laws as data, piece 1) used as relations rather than
  shrinking rewrites: expansions as well as contractions, so the closure of the rules connects every pair of equal
  forms a domain can prove equal, and a search may take steps that grow before they shrink.
- **Holes bound by one mechanism.** Bind `?n` by unification against those relations (narrowing) and against data
  (evaluate with the hole symbolic, solve the residual constraints), instead of a binder per shape. Every relation is
  then also a hole binder.
- **An exhaustive floor.** Enumerate candidate expressions in order of size over the functor set and check each
  against the data (the evaluator) or the expression (a normal-form oracle, or evaluation on a finite domain). It is
  minimal by construction and slow; `prog_enumerative_hypotheses` is one domain's instance of it.
- **Search that interleaves.** The fast layer runs first; the complete layer gets a share of the budget, dovetailed
  (a growing share of time, never starved by detectors), in an e-graph or MCTS that keeps non-shrinking steps.
  Results from either layer compete on size as today.

**Limits.** Equality is undecidable in general (programs, reals), so the guarantee is relative: for data, the minimal
model consistent with the data, reached by enumeration in the limit; for expressions, exact within domains that have
a decision procedure (polynomials, finite Booleans), and anytime best elsewhere.

**Cost.** It must not slow down what is fast. The complete layer only spends what the fast layer leaves, the gate
(`make bench_check`) holds every current cost and time, and the dart board measures what it adds. Build it after the
foundations; until then, keep adding special cases.

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
6. **Completeness.** Generic relations in both directions, hole binding by narrowing, the exhaustive floor and an
   interleaved search: the guarantee that the minimal form is eventually found, with the special cases as its
   accelerators.

Each step lands the usual way: a benchmark problem first, a fix that generalizes, the gate, the showcase.
