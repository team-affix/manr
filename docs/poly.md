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

**Notation.** There are no named variables: inputs and parameters are de Bruijn indices `#k`, and every fitted model
is a closed function of the data piecewise's inputs, `lam(b)` or `lams(k, b)` (`plans.md`, bound variables). Older
examples below that name an input (`x`, `xs`, `n`, `a`) mean its index: one input is `#0`; with k inputs the first is
`#(k-1)` and the last `#0`.

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
which applies wherever the condition holds. The first branch that applies gives the value. Conditions are `lt(a, b)`
and `eq(a, b)`, and a side may be `mod(a, m)`: `a` modulo a positive literal `m`, from 0 to `m - 1`, so
`eq(mod(x, 2), 0)` is "x is even" for negative `x` too.

`poly_interpolator` solves for the polynomial through the data. Three detectors use it.

`poly_interpolation_hypotheses` proposes one polynomial through all the data:

- For each total degree, lowest first, the unknowns are the coefficients of every monomial up to that degree.
- It solves them exactly (Gauss–Jordan over rationals) from points taken in order from several starting indices,
  so a fit can start past an outlier.
- It proposes the result only when the coefficients are integers and the polynomial reproduces at least one point
  beyond those that fixed it.
- It stops at the first degree that reproduces every point.

`poly_split_hypotheses` proposes a piecewise polynomial, `pw(when(lt(x, k1), P1), ..., Q)`, for each input variable:

- It orders the data by that variable and cuts it greedily into pieces. From the first unexplained point, the next
  piece is the interpolant that saves the most nodes: the branches of the points it reproduces, less its own size
  (expanded). Ties go to the lower degree. So a higher degree that reaches further by skipping a step, or reproduces
  one more point, wins only if it pays for its size.
- Interpolants are tried from several of the next points, so an outlier near the start of a piece does not hide it.
- Within a run, one missed value of the variable is tolerated if the next fits. Missed points stay as `case`
  branches.
- It proposes only when there are two pieces or more.

`poly_iteration_hypotheses` (in the fit rule set, not the element domain) proposes a polynomial iterated with a helper,
`let(lam(g(#0)), app(#0, app(#0, ... x)))` (`plans.md`, abstraction):

- For each degree `d >= 2` and count `k >= 2` with `d^k` up to 16 and below the number of samples, the data is
  interpolated at degree `d^k`, past the usual degree limit.
- A polynomial `P` reproducing every sample is solved for `g` with `g^k = P` exactly: the leading coefficient is an
  integer root of `P`'s, and each next coefficient, from the top, enters the next coefficient of `g^k` linearly, so
  trying 0 and 1 and dividing pins it. The whole `g^k` is checked against `P`.
- `g(g(g(x)))` with `g = y^2 + 1` (degree 8, size 23 expanded) becomes the 14-node helper form (reduce request #8).
  Expressions are evaluated through the abstraction reducer, which reduces helpers away first.

`poly_residue_hypotheses` proposes a periodic piecewise over one variable, `pw(when(eq(mod(x, m), r1), P1), ..., Q)`:

- For the smallest `m` from 2 to 6 that fits, each residue class of `x` modulo `m` is interpolated at the lowest
  degree reproducing every sample in it, and must hold at least two samples (a class with none is free).
- The polynomial most classes share is the otherwise. So a parity, `pw(when(eq(mod(x, 2), 1), 0), 1)`, is found as
  easily as `pw(when(eq(mod(x, 3), 0), x^2), 2)`, and outliers on every third point become one branch.
- Data that is one polynomial in every class is left to interpolation.

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

Branches guarded by any Boolean condition (`pw_guarded_hypotheses`, with the condition domain of `logic.md` over
thresholds, residues and primality): `x^2` where x is prime and `2x + 1` elsewhere reads as
`lam(pw(when(is_prime(#0), #0^2), 2 * #0 + 1))`; a default model that reproduces most points, the points it misses
read as a condition and fitted as a value. Readings are kept only when exact on every point.

Labels holding holes (#18): `x^2 + 1` with labels `?1` and `2 * ?2` reduces to `lam(#0^2 + 1)` with `?1 = 2`,
`?2 = 5`: the branch filter matches a label under a binding, solving a linear one for its hole. A label with no
integer solution (`2 * ?1` where the model gives 5) stays a case.

Gaps:

- Integer coefficients only: `x*(x+1)/2` has no polynomial over Z, so it is not found.
- Up to 8 starts: when outliers are dense enough that every start's window holds one, no fit is found.
- Regions are intervals of one variable, nested in order. Regions bounded by a curve or by two variables at once,
  such as quadrants, are not found.
- A bound is placed where the next piece's run starts. Where two pieces agree at a point near the bound, that point
  can go to the wrong piece and leave one extra `case`.
