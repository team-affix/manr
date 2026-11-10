# Number theory: primitives for the primes

Primes are common enough to get primitives (reduce requests #12, #13, #14), rather than being rebuilt from general
vocabulary each time. The `number` module (`num_`) is vocabulary on top of another domain, not a fitting domain of
its own: it is wired into polynomial, sequence and list fitting.

## Vocabulary

| expression | meaning |
|---|---|
| `prime(n)` | the n-th prime, from 0 (`prime(0) = 2`); no value for a negative n |
| `is_prime(n)` | 1 if n is prime, else 0; as a condition, it holds where n is prime |

`num_prime_table` answers both from a sieve grown on demand, up to 10^7 (`k_num_default_max_value`); past that
there is no value. `num_evaluator` evaluates the primitives over another domain's evaluator: a primitive's argument
is evaluated through it (so they nest, and `is_prime(#0 + 1)` works), and everything else goes to the inner domain.
The inner domain's own expressions cannot hold a primitive: `prime(#0) + 1` has no value yet.

## Detector

`num_prime_hypotheses` compares integer data of one input with the primes. Data of 0s and 1s with at least one 1 is
read as `is_prime(#0)`, any other integer data as `prime(#0)`. It proposes the reading when it explains more than half
of the points (the rest stay as branches). It also serves as a condition domain: the list filter detector hands it
the kept values as 1s and the removed values as 0s, so `filter(lam(is_prime(#0)), #0)` is read from list data.

## Benchmark

| problem | bench | cost | form |
|---|---|---|---|
| #12 the n-th prime, and from index 20 | seq | 3 | `lam(prime(#0))` |
| #13 is n prime | seq | 3 | `lam(is_prime(#0))` |
| is n prime, two wrong labels | seq | 12 | two cases + `lam(is_prime(#0))` |
| #14 primes of a list, and of longer lists | list | 6 | `lam(filter(lam(is_prime(#0)), #0))` |
| primes of a list, wrong label | list | 24 | one case + the filter |

## Gaps

- Primitives inside another domain's expressions (`prime(#0) + 1`, `2 * is_prime(#0)`): the inner evaluators would
  need to defer to this one.
- Offsets and compositions: `prime(#0 + 1)`, primes from a nonzero start counted from there, `is_prime` of a mapped
  value. The detector only compares with `prime(#0)` and `is_prime(#0)`.
- More number theory: divisors, gcd, factorization, as reduce requests ask for them.
