#ifndef LOGIC_SYMBOLS_HPP
#define LOGIC_SYMBOLS_HPP

#include <compare>
#include <cstdint>

// Functor ids of the logic vocabulary, over the truth values 0 and 1 (integer literals): `and(a, b)`, `or(a, b)`,
// `xor(a, b)` and `not(a)`. Every other nullary functor that is not a literal is a de Bruijn index
// (`#k`), bound by the function the formula is the body of.
struct logic_symbols {
    uint32_t conjunction;
    uint32_t disjunction;
    uint32_t exclusion;
    uint32_t negation;
    auto operator<=>(const logic_symbols&) const = default;
};

#endif
