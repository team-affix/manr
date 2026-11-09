#ifndef SEQ_SYMBOLS_HPP
#define SEQ_SYMBOLS_HPP

#include <compare>
#include <cstdint>
#include <vector>

// Functor ids of the sequence vocabulary, over an integer index:
//   `rec(s, b0, ..., b(k-1), n)`   the term at index n of the sequence a with a(j) = bj for j < k and, from k on,
//                                  a(j) = s with `i` the index j and `p1`, `p2`, ... the terms a(j-1), a(j-2), ...
//                                  (0 before the start). Its order k is its number of bases, at most `previous`'s
//                                  size. No value at a negative index.
//   `exp(b, n)`                    b to the power n, for n >= 0.
// The step s belongs to the polynomial domain, over `i` and every `previous` variable.
struct seq_symbols {
    uint32_t recurrence;
    uint32_t power;
    uint32_t index;
    std::vector<uint32_t> previous;
    auto operator<=>(const seq_symbols&) const = default;
};

#endif
