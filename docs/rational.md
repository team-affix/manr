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
  is `x + y`); otherwise, over one variable, numerator and denominator are both divided by their greatest common
  divisor (`(x^2 - 1) / (x^2 + 2x + 1)` is `(x - 1) / (x + 1)`, `poly_arithmetic::find_gcd`). Fires only when smaller.

The cancelled form also has a value where the cancelled factor was 0, where the quotient had none: it is the quotient's
continuous extension, as computer algebra writes it. The benchmark compares fractions by cross-multiplying,
`n1 * d2 = n2 * d1`, which is equality wherever both denominators are nonzero.

## Benchmark

`poly_bench`: `#38 (x^2 - 1) / (x - 1)` and three like it (a divisor in two variables, a common factor of degree one,
nothing in common), under all four polynomial solvers.

## Gaps (the open problems)

- **Common factors in several variables.** Exact division works in any number of variables; the gcd only in one.
- **Sums and products of quotients**: `1 / x + 1 / y` to `(x + y) / (x * y)`, partial fractions, and quotients
  inside polynomials; no rule moves terms across a `div`.
- **Domains of definition.** Cancelling loses where the quotient had no value; a reading that keeps it
  (`when(not(eq(x, 1)), ...)`) would make the extension explicit.
- **Data**: fitting rational functions to samples (Padé-like readings) needs a hypothesis source.
