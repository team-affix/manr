#ifndef POLY_ROOT_HPP
#define POLY_ROOT_HPP

#include <compare>
#include <cstdint>
#include "value_objects/poly_polynomial.hpp"

// polynomial = root ^ degree + remainder
struct poly_root {
    poly_polynomial root;
    uint32_t degree;
    poly_polynomial remainder;
    auto operator<=>(const poly_root&) const = default;
};

#endif
