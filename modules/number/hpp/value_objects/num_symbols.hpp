#ifndef NUM_SYMBOLS_HPP
#define NUM_SYMBOLS_HPP

#include <compare>
#include <cstdint>

// Functor ids of the number-theory primitives, over integer literals:
//   `prime(n)`     the n-th prime, from 0 (`prime(0) = 2`); no value for a negative n.
//   `is_prime(n)`  1 if n is prime, else 0; as a condition, it holds where n is prime.
struct num_symbols {
    uint32_t prime;
    uint32_t is_prime;
    auto operator<=>(const num_symbols&) const = default;
};

#endif
