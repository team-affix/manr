# Rule design: fail early, compute cheaply

MANR's advantage over generic rewriting is that rules and detectors are hand-written, so each one can know its own
domain. Two principles follow, and every rule and detector should be written to them.

## 1. Fail early

A rule is asked "do you apply here?" at every position of every expression the search visits, and almost always
the answer is no. So the no should be as cheap as possible.

- **Exit on the first failed requirement.** When a rule needs several things to hold, check them one at a time and
  return as soon as one fails. Don't gather everything first and decide at the end.
- **Check necessary conditions before the real test.** A necessary condition is a one-way implication: if the rule
  applies, the property must hold. The converse need not be true. If the property fails, the rule cannot apply, so
  return before doing any expensive work. If it holds, carry on to the full check. Example: if a rule only works
  when every number involved is positive, check for positivity first when that is cheap.
- **Cheapest and most selective first.** Order the checks by cost, and among checks of similar cost, put first the
  one that rejects most often.
- **A filter must cost much less than what it saves.** A filter is worth having when (chance it rejects) × (cost of
  the full test) is much larger than (cost of the filter).

### Refuting is often far cheaper than confirming

Proving a rule applies often means nearly computing its result: fitting the polynomial, solving the system, expanding
the product, building the rewritten expression. Proving it does not apply often takes one cheap fact that any result
would have to satisfy. The two questions have very different costs, so ask the cheap one first. Design each rule by
asking: **what must be true of the data or expression if this rule applies, and what is the cheapest such fact to
check?** Each such fact is a refutation, and a rule can stack several, cheapest first, before its real test.

Examples from MANR's rules:

- **Too few data to pin the unknowns.** A fit with k unknowns needs at least k + 1 points to be checked. Counting
  points or distinct values refutes it before any solve (`list_affine_fold_solver`, the count and product fold
  detectors).
- **The output must be one of the inputs.** A largest-element fold can only output an element of its list or a value
  beyond all of them, so one pass over a list refutes it, without trying any base.
- **Zeros must line up.** A product fold `(a * h + k) * acc` outputs 0 only on lists that contain the root `-k / a`,
  so one nonzero output on a list holding the root, or a zero output on a list without it, refutes that factor
  without computing any big product.
- **Remainders.** Equality, divisibility and perfect powers can all be refuted modulo a word-sized prime
  (section 2), long before exact big-integer work.
- **Abandon at the first failure that decides.** When a candidate must reproduce more lists than the best found so
  far, checking stops at the first failure past what it can afford, which is usually the first or second list
  (`list_affine_fold_hypotheses`).

Memory counts as cost too: refute before allocating anything the full test would need.

### Signatures: precomputed facts about each expression

Expressions are interned, so any property computed from an expression's children can be computed once per
expression and cached by its address, like `expr_size`. A *signature* is a bundle of such facts, for example:

- its set of variables, total degree, and number of nodes
- whether it contains a subtraction or a negative literal
- the size (in bits) of its largest literal
- its value at a few fixed points modulo a large prime (see below)

Each fact is computed in one step from the children's facts. A filter then reads facts in O(1) and never walks the
expression. Domain modules own their signatures. Core only provides the caching pattern.

### Soundness: filters may only reject

A filter, especially a modular one, may reject a candidate but must never accept it. Every proposal is still
checked exactly before it is made. A filter that wrongly passes a candidate only wastes time; a filter used to accept
could make a wrong rewrite. That would break the guarantee that MANR never makes a wrong rewrite.

## 2. Don't compute with large numbers when you don't need to

Exact big-integer arithmetic (`integer`) is the final check, not the first. Most questions a rule asks can be
answered "definitely not" with arithmetic modulo a prime that fits in a machine word.

- **Equality.** Two polynomials that differ give different values at a random point modulo a large prime p, except
  with probability at most (degree)/p (Schwartz–Zippel). Evaluating takes one pass over the expression and needs no
  expansion. So different fingerprints prove two expressions differ, and matching fingerprints mean "check exactly."
- **Divisibility.** If D divides P over the integers, it divides P mod p. For a linear D, pick a point mod p where
  D is zero; P must be zero there too. For a general D, D(a) must divide P(a) at small integer points a.
- **Perfect powers.** If P = Q^n, then P(a) is an n-th power for every point a. Modulo p, it must be an n-th power
  residue (for n = 2, a quadratic residue, which is a one-exponentiation test).
- **Roots.** For a candidate root r = s/t, check whether P(r) · t^degree ≡ 0 mod p before evaluating it exactly.

Use several primes or several points when one gives too many false passes. Their failures are independent.

## 3. Time budgets: cheap detection, bounded rewrite

There will be many rules, and every one is asked at every position, so one slow rule slows the whole system.

- **Detection** (does this rule apply here?) must take **under 1 second**, and should usually take a small fraction
  of that.
- **Rewrite** (compute the result, done only once search chooses to take that step) may take up to **10–20 seconds**.

Detection and rewrite are separate steps, curried. A rule's `detect(e, out)` appends one `rewrite` per rewrite that
applies (`core/hpp/value_objects/rewrite.hpp`): a thunk capturing what detection found. Nothing appended means the
rule does not apply. A search performs only the rewrites it takes.

**Detection is certain.** It resolves every uncertainty: by the time a rewrite is appended, the rule is sure it
applies and meets all its conditions (e.g. "the result is no larger"). Performing it always succeeds and never
returns nullptr. The searches assert this, and `poly_soundness.cpp` checks it for every polynomial rule. The thunk
holds the remaining work that cannot change the answer, such as building the result, and as much work as possible
should go there. Cheap filters (section 1) are how detection stays fast while being certain: they reject most
candidates before the exact check that settles the rest.

**Who keeps the budgets.** Core and the rules enforce nothing. Every limit is a parameter, and a caller may set any
value, including one that breaks the budgets. Keeping within them is the job of whoever composes and runs the rules
(a solver manifest, the benchmark, a test). To make that easy, each rule's limit parameters have default arguments:
known reasonable values, named `k_<module>_default_<limit>` in the rule's header, chosen to stay within the budgets
at typical sizes. `k_poly_default_limits` gathers them for configuring a whole solver at once.

## 4. Terms too large to expand

Rules that work on the expanded form only reach subterms under the term limit. Larger terms need rules that never
expand them fully:

- **Structure instead of expansion.** Flatten sums and products into cached n-ary views. Then merge identical terms
  (by pointer) and pull out factors that every summand shares, all without expanding.
- **Split, then analyze the pieces.** Partition a large sum by the variables its summands use (computed bottom-up,
  no expansion). Each piece small enough is handed to the expansion-based detectors.
- **Regroup by one variable.** Write a sum as `sum_k v^k * C_k` from its summands' structure. The coefficients
  `C_k` are smaller and may be analyzable.
- **Bottom-up invariants.** Degree, leading term, variable set, and a modular fingerprint can each be computed from
  the children's values, without expanding. They act as filters, e.g. q^n requires a degree divisible by n.
- **Truncated expansion.** Compute only the few highest terms of an expansion (enough to guess a root or factor),
  never the whole thing.

Open question: exactly verifying that two huge polynomials are equal may itself need full expansion. Such rewrites
must be correct by construction (built only from identities), or else accepted on a probabilistic check (several
primes, with error probability set by configuration). The second option weakens the "never wrong" guarantee, so it
needs an explicit decision.

## 5. Free variables in the expression

The goal is generalization: fitting piecewises, and symbolic expressions in general, that themselves contain free
variables (`?n`), in every domain. A free variable is a hole at the syntax level: reduction may bind it to any
expression, and binding it well is how reduction generalizes (`(?0 + 1)^2 - ?0^2` reduces to `1` with `?0 = 0`;
`or(and(not(x), y), ?0)` to `xor(x, y)` with `?0 = and(x, not(y))`; the data piecewise's catch-all is the same thing).
So when writing a rule, consider the case where part of the expression or the data is a free variable: a rule may
leave it alone, or propose a binding that shrinks the whole expression.

- **Handle it when it costs nothing.** If the rule can treat `?n` as an opaque term, or simply skip it, without
  losing speed or reach on the common case, do that. It must never crash or rewrite wrongly because a `?n` appeared.
- **Don't weaken a rule for it.** If handling free variables would make a fast common-case rule slower or less
  effective, leave the rule as it is and add a separate rule beside it that works with free variables (as with
  specific and generic detectors, `extending.md` design rule 7).
- The architecture is to add modules and add rules; free-variable support can grow the same way.

## Where the current polynomial rules stand

None of this has been applied yet. These are the most promising opportunities, with the biggest expected wins first:

| rule | expensive part now | early exit / modular filter |
|---|---|---|
| `poly_factor_detector` | exact division for each of up to ~100 candidate divisors | modular divisibility check per candidate; only survivors get exact division |
| `poly_linear_factor_finder` | exact big-integer evaluation of every rational root candidate | check P(s/t) · t^n ≡ 0 mod p first |
| `poly_root_extractor` | recomputes q^n exactly after every term it adds | filter first: degree divisible by n, leading coefficient an exact n-th power, P(a) an n-th power residue mod p at a few points |
| `poly_like_terms` | expands every summand fully just to compare them | compare cached modular fingerprints; expand only summands whose fingerprints match |
| `poly_collector`, `poly_horner_detector` | expand and render, then compare sizes | Horner needs a variable of degree >= 2 and at least two terms; the collector can bound the expanded size from the term count before rendering |
| every detector | `find_polynomial` on every subterm | skip subterms whose signature rules them out (no variables, a single node, or a total degree the detector can't use) |

To measure the effect, the benchmark should count, per rule: calls, filter rejections, exact rejections, and
proposals.
