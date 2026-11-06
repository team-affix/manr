# Polynomial algebra (`modules/poly`, `modules/poly_reduce`)

The first real domain: polynomials over the integers, with `+ - * ^` (exponent a non-negative integer literal),
integer literals of any size, and any number of variables. Cost is node count.

Literals and coefficients are arbitrary-precision (`integer`, Boost's `cpp_int`), so arithmetic is exact. The only
limits are explicit ones: at most 256 terms in a normal form, exponents up to 64, the power detector up to degree 8,
and rational-root divisors up to 100 000.

Every expression has an exact **normal form**: the expanded sum of monomials, `poly_polynomial`, computed by
`poly_expander` with `poly_arithmetic`. Two expressions are equal as polynomials iff their normal forms are equal.
The normal form is the oracle that high-level detectors read, and `poly_soundness.cpp` checks every rule against it.
It is also how the benchmark verifies every result.

## Rules

Low-level (`poly_local_rules`, local, syntactic): folding, identities, power folding, like terms, factoring
`ab + ac`, distribution, power expansion, commutativity, associativity. This is close to a complete rewrite system
for commutative rings, so search could in principle reach any equal form with these rules alone.

High-level detectors (each reads the normal form and proposes a far-away result in one step):

| detector | finds | example |
|---|---|---|
| `poly_collector` | cancellation hidden behind expansion | `(x+1)^2 - (x-1)^2` -> `4*x` |
| `poly_like_terms` | like terms merged without expanding them | `3*(a+b+c)^9 - 2*(a+b+c)^9` -> `(a+b+c)^9` |
| `poly_power_detector` + `poly_root_extractor` | `q^n + r`: binomial/multinomial theorem backwards, completing the square/cube | `x^2+6x+10` -> `(x+3)^2 + 1` |
| `poly_factor_detector` | `d * (p/d)` by exact division against candidates: content, rational-root linear factors, variable pairs, the subterm's own sums, and restriction | `x^3+6x^2+11x+6` -> `(x+1)*(...)` |
| `poly_linear_factor_finder` | multivariate linear factors with any coefficients, from the rational roots of each one-variable restriction | `... ` -> `(x - 3z + 2) * ...` |
| `poly_horner_detector` | (multivariate) Horner form | `x^5+2x^4+...+6` -> `((((x+2)x+3)x+4)x+5)x+6` |
| `poly_component_splitter` | regroups a sum into variable-disjoint parts so other detectors can see them | `x^2+2x+y^2+2y+2` -> `(x^2+2x) + (y^2+2y) + 2` |

Detectors propose only results no larger than their input (factoring and power) or strictly smaller (collector,
Horner, like terms). The splitter is the exception, since its job is restructuring.

## Solvers

`poly_kernel_manifest` holds everything that is not a rule or a search. Rule sets: `poly_rules_manifest` (full) and
`poly_local_rules_manifest` (low-level only). The four solver manifests are those two rule sets crossed with
best-first and MCTS: `poly_reduce_manifest`, `poly_reduce_mcts_manifest`, `poly_baseline_manifest`,
`poly_baseline_mcts_manifest`.

## Benchmark

```bash
make poly_bench && ./build/poly_bench
```

22 hand-written problems from easy to hard, plus 12 random ones. A random problem takes a compact target built from
linear forms (powers and products of up to three of them) and gives the solver only its full expansion. A problem
counts as solved when the result is no larger than the target. Some hand-written targets turned out not to be
minimal, and the solvers beat them, e.g. `(x+2)^3 - (x+2)` for `x^3+6x^2+11x+6`.

Known gaps:

- `poly_linear_factor_finder` needs a nonzero constant term, so linear factors of homogeneous polynomials come only
  from the pair candidates. Restricting at 1 instead of 0 (dehomogenizing) would cover them.
- Irreducible non-linear factors (e.g. `x^2 + x + 1` inside a product) are found only when the factor is already
  written as a subterm, or as the root of an exact power.
- Detectors run at every position on every expansion. The factor detector dominates the run time.
