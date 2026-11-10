# Plans

What we plan to implement, at the level of modules, rules and searches. `vision.md` says what MANR should eventually
be able to do; this document says what we intend to build next and how. When something here is built, its design
moves to its module's doc (`poly.md`, `list.md`, `search.md`, ...) and leaves this one.

Reduce requests (`reduce-requests.md`) decide the order: a plan here is built when a request needs it, with that request's
problems in the benchmark first.

## Abstraction: helper functions, composition and application

Occam's razor needs abstraction. A model that does the same computation in several places is shorter when the
computation is named once and called by name: `g(g(g(x)))` with `g(y) = y^2 + 1` instead of its degree-8 expansion,
or "fold both lists and add 1" defined once and applied to many pairs. Helpers, composition and application (and,
eventually, recursion) are what make MANR's models Turing complete. Nothing in this section exists yet.

### A helper is a cut of the computation graph, not a subterm

Think of an expression as a computation graph. **Any connected piece of it is a function**: pick a node as the output,
take some of the nodes below it, and cut every edge that leaves the piece downward. Each cut edge is an input. The
piece's arity is its number of inputs. That piece is a *fragment*, and every helper is one.

Some fragments, from `a(f(g(x)))` and `b(f(g(y + 1)))`:

| fragment | arity | what it is |
|---|---|---|
| `f(g(□))` | 1 | the composition `f ∘ g`: a chain of functions with the input left out. It is neither a subterm (it does not reach the leaves) nor rooted at the top (`a` and `b` differ). Here it occurs twice, applied to `x` and to `y + 1` |
| `f(g(x))` | 0 | a subterm: the fragment with no cuts. Here it occurs only once |
| `f(□)` | 1 | a single function node, everything below cut |
| `□ * □ + □` from `y * y + y` | 1 | the same input used several times: holes that always receive the same argument are one input, `#0` repeated |
| `□ + □` from `p + q` | 2 | two independent inputs |

So subterms are the special case with arity 0. They are the easiest to find, since interning makes equal subterms
the same pointer, and they deserve their own cheap rule. But most helpers worth having have inputs: compositions
`f ∘ g ∘ h` that recur on different arguments, a "fold both inputs and add something" that recurs on different pairs
of lists, a shape that recurs with different constants. A fragment can sit anywhere: under different parents, above
different arguments, inside a map body or a piecewise region.

Fragments are not unique: from two occurrences, cutting higher gives a smaller fragment that may occur in more places;
cutting lower gives a larger one with fewer occurrences. When a hole receives the same argument in every occurrence,
it is better not cut (the argument belongs in the body). Choosing the cut is the search problem of abstraction.

Further out: a cut can have several outputs (a piece of the graph computing two results), which is a helper
returning a tuple. Not needed first.

### Why it fits MANR's objective as is

`expr_size` counts tree size: a fragment occurring k times is paid k times. Naming it once and calling it is
therefore a real cost reduction under the existing objective. No new measure is needed. For a fragment of size B
(its own nodes, not its inputs' arguments), occurring k times, with p inputs:

    saving = (k - 1) * B  -  (definition overhead)  -  k * (call overhead, which grows with p)

A rule that introduces a helper fires only when the saving is positive, so it strictly shrinks the expression
(`extending.md` design rule 3). The mirror rule, inlining, fires when the saving is negative, for example a helper
used once.

### Vocabulary (a domain-free module, like `piecewise`)

A new module, `abstraction` (`abs_`), generic over the domain, as the piecewise module is:

| expression | meaning |
|---|---|
| `lam(b)` | a function whose body `b` refers to its parameters by de Bruijn index |
| `#i` | the parameter `i` binders out (de Bruijn index; a nullary symbol, so a body's domain sees a variable) |
| `app(f, a)` | `f` applied to `a`; several parameters by currying, or `app(f, a, b, ...)` |
| `let(v, b)` | `b` with `v` bound as `#0`: exactly `app(lam(b), v)`, opening a scope. How a helper is defined once and used many times: `v` is the helper (a `lam`), and `b` calls it by its index, `app(#0, ...)` |

Design points:

- **Helpers are referenced by index too.** A helper has no name: `let(lam(#0^2 + 1), app(#0, app(#0, x)))` defines
  `g(y) = y^2 + 1` and applies it twice, `#0` in the scope of the `let` being the helper itself. Inside the helper's
  own `lam`, `#0` is its parameter instead; under each further binder, the helper is one index further out (`#1`
  under one `lam`), as for any bound variable.
- **De Bruijn indices, not names.** Inputs are numbered, never named, so there is no naming to manage, and interning
  makes two helpers with the same body the same pointer: equal helpers are recognized as one for free, and dedup in
  the search keeps working.
- **Bound parameters are not free variables.** `expr::var` (`?n`) stays the extension term, an unknown to be
  resolved. `#i` is a bound parameter. They must never be confused. A helper's body may contain `?n`: every
  occurrence of a helper then shares that one unknown, which is the correct meaning (`rule-design.md`, section 5).
- **Evaluation** substitutes arguments into the body and hands the result to the domain's evaluator (`IEvaluateAt`),
  so any domain gets helpers without changing its evaluator. With recursion, evaluation needs a step budget and
  returns nullopt when it runs out, since termination cannot be decided.
- **The existing binders are helpers already.** `map(lam(f), xs)` and `fold(lam(lam(s)), b, xs)` take closed
  lambdas with de Bruijn indices (below, "Bound variables"). Later they take any `lam`, so a helper defined once can
  be mapped, folded and called.

### Rules, as pairs

Paired operations mirror each other. Each introduction has an elimination, and each fires only when it shrinks.

| introduce | eliminate | fires when |
|---|---|---|
| **abstract a fragment**: a fragment occurring k ≥ 2 times becomes `let(lam(body), ...)`, its body with `#i` at its cut edges, and each occurrence becomes `app(#k, its arguments)`, with `#k` the index of the `let` at that occurrence | **inline**: replace each call by the body with its arguments substituted (beta-reduction), and drop the `let` when no call is left | the saving is positive (abstract) or negative (inline: a helper used once, or a body smaller than its call) |

One general rule and several cheaper special cases of it, kept beside it (design rule 7):

- **Common subterms (arity 0).** Interning makes this nearly free: count each node's occurrences in one pass over the
  DAG with multiplicities (paths to each node), cached per root. The easiest rule, and the first to build.
- **Compositions (one input, a chain).** A path of single-argument nodes `f(g(h(□)))` that recurs above different
  arguments. Found by comparing paths of functors.
- **Anti-unification of a pair (any arity).** For two nodes with the same functor, walk both top-down together and
  cut wherever they differ: the result is the largest fragment rooted at both (their least general generalization).
  Holes whose two arguments are equal are merged or not cut, as above.
- **The general search over fragments.** Start from a single functor with every edge cut, and grow a fragment by
  un-cutting one input at a time, ranked by saving, as Stitch searches program corpora. Pruned by refutation
  (`rule-design.md`): a fragment's occurrence count can only fall as it grows, so `(k - 1) * (largest reachable B)`
  bounds any saving below it, and a branch whose bound is no better than the best found is dropped. A fragment
  rooted at functor F can only occur where F occurs; if F occurs once, nothing rooted there can pay.

Abstraction is a **whole-expression detector**, not a local rule: it needs every occurrence. Inline and beta-reduce
are local rules that always shrink when they fire, so they are the cheap "should fire whenever they can" kind. They
also undo abstractions the search tried that did not pay off.

### Detectors that propose helpers from data

Syntactic factoring finds helpers that are already visible in an expression. Detectors can propose them from data,
where no repetition is visible yet:

- **The outer-inner split** (`decomposing-detection.md`) is already a helper detector: it separates a known combiner
  from an unknown inner function `g`, fits `g` once, and uses it at every element.
- **Iterated functions.** Data from `g(g(x))` gives pairs `(x, y)` where `y = g(g(x))`. A detector can look for a
  smaller `g` whose double application fits, for example by fitting `g` on the chain `x -> g(x) -> g(g(x))` when
  the data contains both ends.
- **Shared structure across branches.** A piecewise whose regions use the same shape with different constants is one
  fragment applied to per-region arguments. Anti-unifying the regions finds it.
- **A library across data sets.** Helpers found for one reduce request can be offered to the next, as DreamCoder and
  Stitch do for program corpora. This is the furthest out, and the most powerful.

### Searching with helpers

- Abstraction and inlining are mirror images, so a search can cycle between them. Both fire only when they shrink the
  expression, which rules out a cycle at equal cost.
- Abstraction usually pays only after a fit has made the model regular. So it is a late-stage reduction: the
  detectors find the model, then factoring compresses it. A first version can run abstraction at the root only.

### Benchmark problems to start from (to write first, per `extending.md`)

- **Polynomial:** data from `g(g(g(x)))` with `g(y) = y^2 + 1`. For `g(g(x))` the expanded `x^4 + 2x^2 + 2` (size 11)
  still beats the helper form `let(lam(#0^2 + 1), app(#0, app(#0, x)))` (size 12); one more level of
  nesting expands to degree 8 and the helper wins. A good test of the saving formula.
- **Polynomial:** a piecewise whose pieces are one shape with different constants.
- **Lists:** "fold both lists and add 1", over pairs of lists, which needs inputs that are tuples of lists.
- **Lists:** `map(lam(g), xs)` and `fold(lam(lam(g(#0) + #1)), 0, xs)` sharing one `g`.

### Order of work

Built so far (module `abstraction`, `abs_`, for reduce request #8): the vocabulary (`lam`, `app`, `let`), de Bruijn
interning (`abs_indices`), the reducer (`abs_reducer`: capture-free shifting and substitution, beta-reduction of
`let` and applied lambdas, a step budget, memoized), and evaluation through it (`abs_evaluator`), used by the
polynomial fit rule set. The first helper detector is built too (item 6, iteration): `poly_iteration_hypotheses`.

1. The vocabulary, de Bruijn interning, and evaluation by substitution (built), with a soundness fuzz (an evaluator
   check that a rewrite kept the meaning).
2. Inline and beta-reduce (local, always shrinking).
3. Common subterms (arity 0) at the root, with the saving formula: the cheapest special case.
4. Compositions and anti-unification of pairs: fragments with inputs.
5. The general search over fragments, pruned by its saving bound.
6. Detectors that propose helpers from data: iteration (built, polynomials), shared branch structure.
7. Recursion with a step budget, which is where Turing completeness arrives.
8. A library of helpers carried across reduce requests.

## Bound variables: de Bruijn indices everywhere

**Built.** There are no named variables anywhere: inputs, program parameters and helpers are all de Bruijn indices,
nullary symbols `#k` that a body's domain treats as variables (so polynomial fitting and the polynomial rules work on
bodies unchanged). Binders are explicit and local:

- `lam(b)` binds one argument, `#0` in b. `lams(k, b)` binds k at once and means `k` nested `lam`s: the first
  argument is `#(k-1)`, the last `#0`. `abs_binder_merger` merges chains of 3+ `lam`s (or a `lams` and another binder)
  into one `lams`, since that is smaller.
- **A piecewise is a function of its keys.** Its cases map key tuples to outputs, and what replaces its catch-all is
  closed over the piecewise's own inputs (`abs_closing_hypotheses` wraps every proposed body in `lam`/`lams`). So
  once no case is left, the piecewise is that closed function. There is no root or global scope: a piecewise anywhere
  is a function, and evaluating a function at an input applies it (`abs_applying_evaluator`).
- Programs take closed functions: `map(lam(f), l)`, `filter(lam(c), l)`, `fold(lam(lam(s)), b, l)` (the accumulator
  bound outside, the head inside, so the element is `#0` everywhere), `rec(lams(k + 1, s), b..., n)` (index `#0`,
  previous terms `#1`...). Helpers: `let(v, b)` is `app(lam(b), v)`, the helper `#0` in its scope.
- Moving a term across a binder shifts its indices (`abs_reducer`).
- Free variables `?n` are not indices: they are holes (below).
- **Still to build:** fitting a piecewise nested under other binders (the detectors fit in the piecewise's own
  inputs, so a nested piecewise needs the scope passed to them), and evaluating programs nested in steps.

## Free variables

A free variable `?n` is a hole at the syntax level: reduction may bind it to any expression, and binds it to
whatever makes the whole expression smallest. Binding is what turns minimization into generalization. The data
piecewise's catch-all is bound (`pw_extension_binder`), and so are holes in its labels (built, reduce requests #15,
#18): the branch filter checks a case under some binding of the holes in its label instead of by equality
(`pw_hole_matcher`, one binding shared by the branches of a check), so a model absorbs `case(7, ?1)`,
`case(3, 2 * ?2)` and `case(cons(3, cons(4, nil)), cons(9, cons(?1, nil)))`. A hole matches anything, functors of
one shape match argument by argument, and the rest is solved in the domain: `arith_hole_solver` solves a label
polynomial in a hole over the integers (`2 * ?2 = 10`, `?1^2 + 1 = 10` by integer roots, #22), with any other unbound
holes of the label set to 0, then 1 (`?1 + ?2 = 10`, #23). Holes in the keys are solved too (#20, #24): a
polynomial evaluated at a key holding a hole gives the residual (`?1^2 + 1` at `point(?1)`), which the list evaluator
carries into its elements, and the matcher matches a value holding holes against a plain label (`?1^2 + 1` against
17 gives `?1 = 4`). Every other `?n` is carried through untouched, which is sound but never generalizes. What remains
(`rule-design.md`, section 5):

- **Keys the evaluators cannot carry**: a hole in a filtered element (a condition at a hole has no value), a hole
  standing for a whole list or its length, and labels with holes matched against residuals with holes.
- Holes in labels the logic table reader refutes (a truth table with an unknown row is a don't care there).
- **Binding rules on expressions**: a hole whose best binding shrinks the whole, e.g. `(?0 + 1)^2 - ?0^2` reduces to
  `1` with `?0 = 0`, or `or(and(not(x), y), ?0)` to `xor(x, y)` with `?0 = and(x, not(y))`. These are detectors:
  read what the rest of the expression needs from the hole, and propose it.
- **One hole, one binding.** A rewrite that binds `?n` substitutes it everywhere in the whole expression, so it acts
  at the root even when it was found in a subterm (a binding rewrite, beside `subterm_proposer`'s local ones).
- **A text form for `?n`** in the parsers, so reduce requests with holes can be written as text.

## Search

- **Root advancement.** Commit to the best first move and re-root, so a long horizon can be covered by many shorter
  probes (`search.md`).
- **Rollout policies smarter than uniform**, for example ones that prefer detector proposals over local rewrites.
- **Per-rule counters in `proposer_union`:** calls, detection time, proposals, and proposals on the best derivation.
  A rule that never appears on a best derivation in the corpus is costing the search and contributing nothing.
- **Stopping at a form nothing can improve.** Best-first spends its whole budget after finding the long product's
  fold on its first expansion (`list.md`, gaps).

## Lists

The open problems and their causes are in `list.md` (gaps, and the fold families table). What we plan to build:

- **A count with fewer unknowns than values**, for long lists over many values (`list.md`, gaps); the generic `+`,
  `max` and `*` readings are built.
- **Residue conditions in filters**, beside the threshold filters.
- **Sign-aware rendering**: put a positive term first (`#1 - #0`, not `-1 * #0 + #1`).
- **A piecewise inside a condition**: conditions evaluated by the piecewise evaluator, so a step like
  `pw(when(lt(acc, |h|), |h|), acc)` can be evaluated (`list.md`, gaps).
- **A zero-alignment filter for the product detector**: a factor whose root is in a list must give 0 there, and only
  there (`rule-design.md`, refuting is cheaper than confirming).
- **Booleans** in the vocabulary, for `all` and `any`, generic filter conditions, and list-to-boolean data.
- **List-valued steps beyond enumeration**: list-valued folds are read by enumerating steps up to 7 nodes
  (`list_listed_fold_hypotheses`); larger ones (a sort, an insertion) need a reading that decomposes them.
- **More compositions**: `filter` over `map`, a fold over a map.

## Sequences

The open problems are in `seq.md` (gaps). What we plan to build:

- **Bases solved for, not read**, so data that starts past index 0 still reads as a recurrence.
- **Closed forms of higher order** from the characteristic polynomial's integer roots (`2^n + 3^n`), beside the
  order-one closing rule.
- **Recurrences as parts of a model**: a recurrence plus a polynomial, or a recurrence on a region.

## Logic

The open problems are in `logic.md` (gaps). What we plan to build:

- **Local rules** for formulas: absorption, De Morgan, double negation, distribution.
- **Exact or better minimization** beside the greedy factored cover, for mixed forms like `and(xor(..), not(..))`.
- **Booleans for other domains**: list conditions (`all`, `any`, the count of evens in #4) and region conditions.

## Core vocabulary

- **Tuples as inputs**, so a model can take several values or several lists (needed by "fold both lists and add 1").
- **A text front end**: reading data and expressions from text, so a reduce request can be run without writing C++.
