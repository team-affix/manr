#ifndef LIST_SYMBOLS_HPP
#define LIST_SYMBOLS_HPP

#include <compare>
#include <cstdint>

// Functor ids of the list vocabulary. A list value is `cons(head, tail)` ending in `nil`. A program reads the input
// list `xs`:
//   `map(f, l)`            f, in the element variable `e`, applied to every element of l
//   `filter(c, l)`         the elements of l where the condition c, in `e`, holds
//   `fold(s, b, l)`        right fold: b for nil; for cons(h, t), s with `h` the head and `acc` the fold of t
// The bodies `f`, `c` and `s` belong to the element domain, which evaluates them.
struct list_symbols {
    uint32_t nil;
    uint32_t cons;
    uint32_t map;
    uint32_t filter;
    uint32_t fold;
    uint32_t input;
    uint32_t element;
    uint32_t head;
    uint32_t accumulator;
    auto operator<=>(const list_symbols&) const = default;
};

#endif
