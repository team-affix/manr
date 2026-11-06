#ifndef PW_DATA_POINT_HPP
#define PW_DATA_POINT_HPP

#include <compare>
#include "value_objects/expr.hpp"

struct pw_data_point {
    const expr* input;
    const expr* output;
    auto operator<=>(const pw_data_point&) const = default;
};

#endif
