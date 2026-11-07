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
| `poly_linear_factor_finder` | multivariate linear factors with any coefficients, from the rational roots of each one-variable restriction (moving the origin first when there is no constant term) | `... ` -> `(x - 3z + 2) * ...` |
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

22 hand-written problems from easy to hard, plus 12 random ones over x, y, z and 8 wide ones over 5 to 8 of a..h. A random problem takes a compact target built from
linear forms (powers and products of up to three of them) and gives the solver only its full expansion. A problem
counts as solved when the result is no larger than the target. Some hand-written targets turned out not to be
minimal, and the solvers beat them, e.g. `(x+2)^3 - (x+2)` for `x^3+6x^2+11x+6`.

Known gaps:

- Non-linear factors are found only as the quotient left after a linear factor, as an existing subterm, or as the
  root of an exact power. A product of factors that are each non-linear in every variable, e.g.
  `(x^2+y^2+1)*(x^2-y^2+x*y+2)`, stays expanded; that needs full multivariate factoring (Zassenhaus and Hensel
  lifting).
- Detectors run at every position on every expansion. The factor detector dominates the run time.

## Fitting data (`modules/poly_fit`)

`poly_fit_runtime` (best-first) and `poly_fit_mcts_manifest` fit integer samples over any number of input
variables. The samples become `pw(case(point(x1, ..., xk), y), ..., ?0)`. Three kinds of rule then work on it at
every position:

- `pw_extension_binder` resolves `?0` to a hypothesis and drops every branch it reproduces.
- `pw_branch_absorber` drops `case` branches that what follows them (the regions and the catch-all) already
  reproduces.
- The full polynomial rule set compresses whatever polynomial results.

`poly_point_evaluator` evaluates a candidate at a point through its normal form, so every shape of a polynomial
covers the same branches. `pw_evaluator` evaluates through piecewises, and `poly_condition_evaluator` decides their
region conditions.

A piecewise branch is either `case(point, value)`, which applies at exactly that input, or `when(condition, value)`,
which applies wherever the condition holds. The first branch that applies gives the value. The only condition so far
is `lt(a, b)`.

`poly_interpolator` solves for the polynomial through the data. Two detectors use it.

`poly_interpolation_hypotheses` proposes one polynomial through all the data:

- For each total degree, lowest first, the unknowns are the coefficients of every monomial up to that degree.
- It solves them exactly (Gauss–Jordan over rationals) from points taken in order from several starting indices,
  so a fit can start past an outlier.
- It proposes the result only when the coefficients are integers and the polynomial reproduces at least one point
  beyond those that fixed it.
- It stops at the first degree that reproduces every point.

`poly_split_hypotheses` proposes a piecewise polynomial, `pw(when(lt(x, k1), P1), ..., Q)`, for each input variable:

- It orders the data by that variable and cuts it greedily into pieces. From the first unexplained point, the next
  piece is the lowest-degree interpolant that runs furthest.
- Interpolants are tried from several of the next points, so an outlier near the start of a piece does not hide it.
- Within a run, one missed value of the variable is tolerated if the next fits. Missed points stay as `case`
  branches.
- Greedy gives the fewest pieces.
- It proposes only when there are two pieces or more.

Limits are in `poly_fit_limits` (`k_poly_fit_default_limits`): max degree 6, at most 84 unknowns (degree 6 in 3
variables), 8 starting points spread evenly over the data.

On grids of 7–1000 points, it recovers:

- `x^3 - x`
- the Horner form of a degree-5 polynomial
- `(2*x - 1)^3`
- `(x + y)^2`
- `(x + y + z)^3`
- `(x + y*z)*x - z`
- `(x - y + 2)*(x + 2*y)` with one outlier left as a branch

Each takes under 1.3 s.

Piecewise polynomials:

- `x^2` for x < 0, `3x + 1` otherwise: `pw(when(lt(x, 0), x^2), 3 * x + 1)` from 20 or 400 points (0.07 s).
- Three pieces: `pw(when(lt(x, 10), 5), when(lt(x, 20), x^2 - 7), (x - 1)^3)`, each piece compressed.
- With an outlier: `pw(case(point(-6), 39), when(lt(x, 0), x^2), 3 * x + 1)`.
- Two variables, split on either one: `pw(when(lt(y, 0), x - y), (x + y)^2)`.

Gaps:

- Integer coefficients only: `x*(x+1)/2` has no polynomial over Z, so it is not found.
- Up to 8 starts: when outliers are dense enough that every start's window holds one, no fit is found.
- Regions are intervals of one variable, nested in order. Regions bounded by a curve or by two variables at once,
  such as quadrants, are not found.
- A bound is placed where the next piece's run starts. Where two pieces agree at a point near the bound, that point
  can go to the wrong piece and leave one extra `case`.
