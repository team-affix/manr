#ifndef PROG_SYMBOLS_HPP
#define PROG_SYMBOLS_HPP

#include <cstdint>

// The vocabulary of generic programs over structured data, by interned id. Integers (with 1 and 0 as true and false):
// `add`, `sub`, `mul`, `pow`, `mod`, `max`, `min`, `lt`, `le`, `eq` (structural equality of any values), `and`, `or`,
// `not`, and guarded values `pw(when(c, v), ..., d)`. Lists: `nil`, `cons`, `head`, `tail`, `length`, `reverse`,
// `append`, `sort`, `unique` (the first occurrence of each value, in order), `elem`, and the higher-order `map(f, xs)`, `filter(p, xs)`, `fold(f, b, xs)` (right fold, `f`
// over the accumulator `#1` and the head `#0`), `scan(f, b, xs)` (left scan, the running values after each element,
// `f` over the running value `#1` and the element `#0`), `zipwith(f, xs, ys)` (`f` over `#1` from xs and `#0` from
// ys). Trees: `leaf`, `node(l, v, r)`, and `tfold(f, b, t)` (b at a leaf, `f` over the folded left `#2`, the value
// `#1` and the folded right `#0`). Records: any other constructor `c(a, b, ...)` is data, and `field(i, r)` is its
// i-th argument. Graphs as lists of two-field records (edges): `reachable(g, s)`, the sorted nodes reached from s.
// Functions: `lam`, `lams`, `app(f, x)`, `let(v, b)`; a tuple of inputs is `point(a, ..., z)`.
struct prog_symbols {
    uint32_t add;
    uint32_t sub;
    uint32_t mul;
    uint32_t pow;
    uint32_t mod;
    uint32_t max;
    uint32_t min;
    uint32_t less;
    uint32_t less_equal;
    uint32_t equal;
    uint32_t conjunction;
    uint32_t disjunction;
    uint32_t negation;
    uint32_t piecewise;
    uint32_t region;
    uint32_t nil;
    uint32_t cons;
    uint32_t head;
    uint32_t tail;
    uint32_t length;
    uint32_t reverse;
    uint32_t append;
    uint32_t sort;
    uint32_t unique;
    uint32_t member;
    uint32_t map;
    uint32_t filter;
    uint32_t fold;
    uint32_t scan;
    uint32_t zip_with;
    uint32_t leaf;
    uint32_t node;
    uint32_t tree_fold;
    uint32_t field;
    uint32_t reachable;
    uint32_t lambda;
    uint32_t lambdas;
    uint32_t application;
    uint32_t binding;
    uint32_t point;
    auto operator<=>(const prog_symbols&) const = default;
};

#endif
