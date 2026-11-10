#ifndef LOGIC_SYMBOLS_HPP
#define LOGIC_SYMBOLS_HPP

#include <compare>
#include <cstdint>

// Functor ids of the logic vocabulary, over the truth values 0 and 1 (integer literals): `and(a, b)`, `or(a, b)`,
// `xor(a, b)`, `not(a)`, and the primitives `xnor(a, b)` (equivalence), `nand(a, b)`, `nor(a, b)`, `imp(a, b)`
// (a implies b) and `maj(a, b, c)` (at least two of three). Every other nullary functor that is not a literal is a de
// Bruijn index (`#k`), bound by the function the formula is the body of.
struct logic_symbols {
    uint32_t conjunction;
    uint32_t disjunction;
    uint32_t exclusion;
    uint32_t negation;
    uint32_t equivalence;
    uint32_t alternative_denial;
    uint32_t joint_denial;
    uint32_t implication;
    uint32_t majority;
    auto operator<=>(const logic_symbols&) const = default;
};

#endif
