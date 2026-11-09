#ifndef LIST_SYMBOLS_HPP
#define LIST_SYMBOLS_HPP

#include <compare>
#include <cstdint>

// Functor ids of the list vocabulary. A list value is `cons(head, tail)` ending in `nil`. A program reads the list it
// is applied to, `#0` (the argument of the function the program is the body of), and takes closed functions,
// `lam(body)`, whose bound variables are de Bruijn indices: `#0` is bound by the innermost `lam`, `#1` by the one
// around it. `map(lam(f), l)`         f applied to every element of l, the element as `#0` `filter(lam(c), l)`      the
// elements of l where the condition c holds, the element as `#0` `fold(lam(lam(s)), b, l)` right fold: b for nil; for
// cons(h, t), s with the head h as `#0` (inner) and the fold of t as `#1` (outer), so the element is `#0` in every
// program `append(a, b)`           the elements of a, then those of b `reverse(l)`             the elements of l in
// reverse order (a primitive: reversal is common enough to be one) The bodies `f`, `c` and `s` belong to the element
// domain, which evaluates them; a fold step the element domain cannot evaluate (one that builds a list, `append(#1,
// cons(#0, nil))`) is evaluated by the list evaluator itself. `element`, `head` and `accumulator` are the index symbols
// each body sees: `#0`, `#0` and `#1`.
struct list_symbols {
    uint32_t nil;
    uint32_t cons;
    uint32_t append;
    uint32_t reverse;
    uint32_t map;
    uint32_t filter;
    uint32_t fold;
    uint32_t lambda;
    uint32_t input;
    uint32_t element;
    uint32_t head;
    uint32_t accumulator;
    auto operator<=>(const list_symbols&) const = default;
};

#endif
