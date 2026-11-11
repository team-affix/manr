#ifndef PROG_VALUE_HPP
#define PROG_VALUE_HPP

#include <cstddef>
#include <memory>
#include "value_objects/expr.hpp"

struct prog_frame;

// A value of a running program: data (an interned expression: a literal, a list, a tree, a record, or an expression
// holding holes), or a closure (a function body of `arity` parameters with the environment it captured). An
// environment is a persistent chain of frames, the innermost binding (`#0`) first, so a closure shares it uncopied.
// The value and the frame refer to each other, so they share this file.
struct prog_value {
    const expr* data;
    const expr* body;
    size_t arity;
    std::shared_ptr<const prog_frame> captured;
};

struct prog_frame {
    prog_value value;
    std::shared_ptr<const prog_frame> next;
};

#endif
