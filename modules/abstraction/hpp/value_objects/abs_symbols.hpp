#ifndef ABS_SYMBOLS_HPP
#define ABS_SYMBOLS_HPP

#include <compare>
#include <cstdint>

// Functor ids of the abstraction vocabulary, generic over the domain:
//   `lam(b)`       a closed function; b refers to its argument as `#0`, and to anything bound further out by a
//                  larger index (de Bruijn indices: `#k` is bound by the k-th enclosing binder, innermost first)
//   `app(f, a)`    f applied to a
//   `let(v, b)`    exactly `app(lam(b), v)`: b with v bound as `#0`. A helper is defined this way, and called by its
//                  index: `let(lam(#0^2 + 1), app(#0, app(#0, x)))`
// Indices are nullary symbols `#0`, `#1`, ... (`abs_indices`), so a body's own domain sees them as variables.
struct abs_symbols {
    uint32_t lambda;
    uint32_t application;
    uint32_t binding;
    auto operator<=>(const abs_symbols&) const = default;
};

#endif
