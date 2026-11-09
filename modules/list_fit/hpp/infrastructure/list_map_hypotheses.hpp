#ifndef LIST_MAP_HYPOTHESES_HPP
#define LIST_MAP_HYPOTHESES_HPP

#include <cstdint>
#include <optional>
#include <unordered_set>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/pw_data_point.hpp"

// A detector that reads list-to-list data as `map(f, xs)`, for elements of any domain: every point whose input and
// output are lists of the same length gives one element data point per position, `point(e) -> output element`. The
// element data points are handed to the element domain, and each body it proposes becomes `map(f, xs)`. Points of any
// other shape are ignored, and stay as branches unless the proposal happens to reproduce them. The first output seen
// for an element is kept, so a conflicting point cannot make the element data inconsistent; elements keep the order
// they are first seen in.
template<typename IFindElements, typename IProposeElementHypotheses, typename IMakeFunctor>
struct list_map_hypotheses {
    list_map_hypotheses(IFindElements& find_elements,
                        IProposeElementHypotheses& propose_element_hypotheses,
                        IMakeFunctor& make_functor,
                        list_symbols symbols,
                        uint32_t point_symbol);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    IFindElements& find_elements_;
    IProposeElementHypotheses& propose_element_hypotheses_;
    IMakeFunctor& make_functor_;
    list_symbols symbols_;
    uint32_t point_symbol_;
    std::vector<const expr*> bodies_;
};

#define LIST_MAP_HYPOTHESES_TEMPLATE \
    template<typename IFindElements, typename IProposeElementHypotheses, typename IMakeFunctor>
#define LIST_MAP_HYPOTHESES list_map_hypotheses<IFindElements, IProposeElementHypotheses, IMakeFunctor>

LIST_MAP_HYPOTHESES_TEMPLATE LIST_MAP_HYPOTHESES::list_map_hypotheses(
    IFindElements& find_elements,
    IProposeElementHypotheses& propose_element_hypotheses,
    IMakeFunctor& make_functor,
    list_symbols symbols,
    uint32_t point_symbol)
    : find_elements_(find_elements)
    , propose_element_hypotheses_(propose_element_hypotheses)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , point_symbol_(point_symbol)
    , bodies_() {}

LIST_MAP_HYPOTHESES_TEMPLATE void LIST_MAP_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    std::vector<pw_data_point> elements;
    std::unordered_set<const expr*> seen;
    for (const pw_data_point& point : points) {
        const std::optional<std::vector<const expr*>> inputs = find_elements_.find_elements(point.input);
        if (!inputs.has_value())
            continue;
        const std::optional<std::vector<const expr*>> results = find_elements_.find_elements(point.output);
        if (!results.has_value() || results.value().size() != inputs.value().size())
            continue;
        for (size_t index = 0; index < inputs.value().size(); ++index) {
            const bool fresh = seen.insert(inputs.value()[index]).second;
            if (!fresh)
                continue;
            elements.push_back(pw_data_point{.input = make_functor_.make_functor(point_symbol_, {inputs.value()[index]}),
                                             .output = results.value()[index]});
        }
    }
    if (elements.empty())
        return;
    bodies_.clear();
    propose_element_hypotheses_.propose_hypotheses(elements, bodies_);
    const expr* list = make_functor_.make_functor(symbols_.input, {});
    for (const expr* body : bodies_)
        out.push_back(make_functor_.make_functor(symbols_.map, {make_functor_.make_functor(symbols_.lambda, {body}), list}));
}

#undef LIST_MAP_HYPOTHESES
#undef LIST_MAP_HYPOTHESES_TEMPLATE

#endif
