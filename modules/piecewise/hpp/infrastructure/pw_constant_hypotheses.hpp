#ifndef PW_CONSTANT_HYPOTHESES_HPP
#define PW_CONSTANT_HYPOTHESES_HPP

#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/pw_data_point.hpp"

// Domain-free hypothesis source: the extension term could be any one output the data already shows.
struct pw_constant_hypotheses {
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
};

#endif
