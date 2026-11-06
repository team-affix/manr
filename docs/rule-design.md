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
