#ifndef ABS_CLOSING_HYPOTHESES_HPP
#define ABS_CLOSING_HYPOTHESES_HPP

#include <cstddef>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/pw_data_point.hpp"

// Closes every hypothesis a domain proposes for a piecewise's catch-all over the piecewise's inputs: a piecewise is a
// function of its keys, so what replaces its catch-all is a closed function, `lam(b)` or `lams(arity, b)`, whose body
// b is what the domain proposed (with the keys' coordinates as `#(arity-1)` ... `#0`). When every case is explained
// the piecewise is just that function.
template<typename IProposeHypotheses, typename IMakeFunction>
struct abs_closing_hypotheses {
    abs_closing_hypotheses(IProposeHypotheses& propose_hypotheses, IMakeFunction& make_function, size_t arity);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    IProposeHypotheses& propose_hypotheses_;
    IMakeFunction& make_function_;
    size_t arity_;
    std::vector<const expr*> bodies_;
};

#define ABS_CLOSING_HYPOTHESES_TEMPLATE template<typename IProposeHypotheses, typename IMakeFunction>
#define ABS_CLOSING_HYPOTHESES          abs_closing_hypotheses<IProposeHypotheses, IMakeFunction>

ABS_CLOSING_HYPOTHESES_TEMPLATE ABS_CLOSING_HYPOTHESES::abs_closing_hypotheses(
    IProposeHypotheses& propose_hypotheses, IMakeFunction& make_function, size_t arity)
    : propose_hypotheses_(propose_hypotheses)
    , make_function_(make_function)
    , arity_(arity)
    , bodies_() {}

ABS_CLOSING_HYPOTHESES_TEMPLATE void ABS_CLOSING_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    bodies_.clear();
    propose_hypotheses_.propose_hypotheses(points, bodies_);
    for (const expr* body : bodies_)
        out.push_back(make_function_.make_function(arity_, body));
}

#undef ABS_CLOSING_HYPOTHESES
#undef ABS_CLOSING_HYPOTHESES_TEMPLATE

#endif
