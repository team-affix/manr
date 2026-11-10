#ifndef SEQ_SYMBOLS_HPP
#define SEQ_SYMBOLS_HPP

#include <compare>
#include <cstdint>
#include <vector>

// Functor ids of the sequence vocabulary, over an integer index:
//   `rec(f, b0, ..., b(k-1), n)`   the term at index n of the sequence a with a(j) = bj for j < k and, from k on,
//                                  a(j) = s, where f is the closed step function `lam(...lam(s))`, k + 1 lambdas
//                                  deep: s sees the index j as `#0` (innermost) and the terms a(j-1), ..., a(j-k) as
//                                  `#1`, ..., `#k` (0 before the start). Its order k is its number of bases, at most
//                                  `previous`'s size. No value at a negative index.
//   `exp(b, n)`                    b to the power n, for n >= 0.
//   `tabulate(f)`                  the infinite list f(0), f(1), f(2), ... of a function of the index.
//   `unfold(f, b0, ..., b(k-1))`   the infinite list of the recurrence's terms, `rec(f, b0, ..., b(k-1), n)` for
//                                  n >= 0.
// The step's body s belongs to the polynomial domain. `index` and `previous` are the index symbols it sees: `#0`, and
// `#1`, `#2`, `#3`.
struct seq_symbols {
    uint32_t recurrence;
    uint32_t power;
    uint32_t tabulation;
    uint32_t unfolding;
    uint32_t lambda;
    uint32_t index;
    std::vector<uint32_t> previous;
    auto operator<=>(const seq_symbols&) const = default;
};

#endif
