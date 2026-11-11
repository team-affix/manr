# Rational functions (`modules/rational`, in `solvers/poly_reduce`)

The fifth domain, and the first started by a reduce request for a domain MANR could not state (#38): quotients of
polynomials over the integers. This is its first piece.

## Vocabulary

| expression | text | meaning |
|---|---|---|
| `div(n, d)` | `n / d` | the quotient of two expressions; no value where d is 0 |

`/` parses at the level of `*`, left-associative, and prints back the same way (`poly_parser`, `poly_infix_printer`).
`rat_symbols` holds the functor.

## Rules

- **`rat_quotient_canceller`** (local rule, in the full polynomial rule set): a quotient of polynomials whose
  denominator divides the numerator is the quotient (`(x^2 - 1) / (x - 1)` is `x + 1`, `(x^2 + 2xy + y^2) / (x + y)`
  is `x + y`); otherwise numerator and denominator are both divided by their greatest common
  divisor (`(x^2 - 1) / (x^2 + 2x + 1)` is `(x - 1) / (x + 1)`, `poly_arithmetic::find_gcd`). Fires only when smaller.

- **`rat_fraction_combiner`** (local rule, beside the canceller): a sum, difference, product or power of quotients
  and polynomials read as one quotient and cancelled. Quotients over one denominator add their numerators
  (`x / (x + 1) + 1 / (x + 1)` is 1, #40), others meet over the product of their denominators
  (`1 / (x - 1) - 1 / (x + 1)` is `2 / (x^2 - 1)`, #41), a product folds its factors in
  (`(x^2 + 1) / (x + 1) * (x + 1)` is `x^2 + 1`, #42), and the result loses its common factor and integer content,
  with a positive leading coefficient below. Fires only when smaller (`1 / x + 1 / y` stays), and leaves a lone
  quotient to the canceller.
- **`rat_hole_binder`** (root rule, through `hole_scope_proposer`, so also inside a sum): binds holes so that a
  quotient in one variable divides exactly. Holes in the numerator: each integer root of the denominator gives a linear
  equation, solved together (`(x^2 - ?0) / (x - 1)` is `x + 1` with `?0 = 1`, #43); one hole in the denominator: each
  integer root of the numerator pins it (`(x^2 - 4) / (x - ?0)` is `x + 2` with `?0 = 2`, #44). Every binding is checked
  by exact division.

The gcd works in any number of variables (`poly_arithmetic::find_gcd`: in one variable Euclid over the rationals; in
several, contents and Euclid on primitive parts by pseudo-remainders over a main variable), so
`(x^2 - y^2) / (x^2 - 2xy + y^2)` is `(x + y) / (x - y)` (#39).

The cancelled form also has a value where the cancelled factor was 0, where the quotient had none: it is the quotient's
continuous extension, as computer algebra writes it. The benchmark compares fractions by cross-multiplying,
`n1 * d2 = n2 * d1`, which is equality wherever both denominators are nonzero.

## Benchmark

`poly_bench`: `#38 (x^2 - 1) / (x - 1)` and three like it (a divisor in two variables, a common factor of degree one,
nothing in common); #39 to #44 and like problems (common factors in two and three variables, telescoping pairs,
quotients of quotients, powers of quotients, two holes in a numerator, a hole in a quadratic denominator, a quotient
with a hole inside a sum), and near misses that stay (`1 / x + 1 / y`, `(x^2 + 1) / (x - ?0)`), under all four
polynomial solvers. Soundness reads each side as a fraction by the rules of fractions (independently of the rules)
and cross-multiplies.

## Gaps (the open problems)

- **Partial fractions**, the reverse of combining (`(2x + 1) / (x^2 + x)` to `1 / x + 1 / (x + 1)` when smaller),
  and splitting a quotient into a polynomial plus a proper quotient (`(x^2 + 1) / x` to `x + 1 / x`).
- **Holes in several variables, on both sides, or at repeated roots** of the denominator (`(x^2 + ?0 x + 1) / (x + 1)^2`
  needs the derivative's equation too).
- **Domains of definition.** Cancelling loses where the quotient had no value; a reading that keeps it
  (`when(not(eq(x, 1)), ...)`) would make the extension explicit.
- **Data**: fitting rational functions to samples (Padé-like readings) needs a hypothesis source.
