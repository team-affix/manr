#ifndef LOGIC_IMPLICANT_HPP
#define LOGIC_IMPLICANT_HPP

#include <compare>
#include <cstdint>

// A product of literals: variable i appears when bit i of `mask` is set, plainly when bit i of `bits` is set and
// negated otherwise. It covers the inputs that agree with `bits` on `mask`.
struct logic_implicant {
    uint64_t mask;
    uint64_t bits;
    auto operator<=>(const logic_implicant&) const = default;
};

#endif
