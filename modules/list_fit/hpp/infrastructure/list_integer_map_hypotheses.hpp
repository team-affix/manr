#ifndef LIST_INTEGER_MAP_HYPOTHESES_HPP
#define LIST_INTEGER_MAP_HYPOTHESES_HPP

#include <map>
#include <optional>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_data_point.hpp"

// The integer case of `list_map_hypotheses`, kept beside it: element samples are integers, ordered by value, and handed
// to the element domain as integer samples. Reads list-to-list data as `map(f, xs)`: every point whose input and
// output are lists of literals of the same length gives one element sample per position, `e -> output element`. The
// element samples are handed to the element domain, and each body it proposes becomes `map(f, xs)`. Points of any other shape are ignored, and stay
// as branches unless the proposal happens to reproduce them. The first output seen for an element value is kept, so
// a conflicting point cannot make the element samples inconsistent.
template<typename IFindElements,
         typename IFindLiteralValue,
         typename IConvertSamples,
         typename IProposeElementHypotheses,
         typename IMakeFunctor>
struct list_integer_map_hypotheses {
    list_integer_map_hypotheses(IFindElements& find_elements,
                        IFindLiteralValue& find_literal_value,
                        IConvertSamples& convert_samples,
                        IProposeElementHypotheses& propose_element_hypotheses,
                        IMakeFunctor& make_functor,
                        list_symbols symbols);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    IFindElements& find_elements_;
    IFindLiteralValue& find_literal_value_;
    IConvertSamples& convert_samples_;
    IProposeElementHypotheses& propose_element_hypotheses_;
    IMakeFunctor& make_functor_;
    list_symbols symbols_;
    std::vector<const expr*> bodies_;
};

#define LIST_INTEGER_MAP_HYPOTHESES_TEMPLATE                                                                    \
    template<typename IFindElements, typename IFindLiteralValue, typename IConvertSamples, \
             typename IProposeElementHypotheses, typename IMakeFunctor>
#define LIST_INTEGER_MAP_HYPOTHESES \
    list_integer_map_hypotheses<IFindElements, IFindLiteralValue, IConvertSamples, IProposeElementHypotheses, IMakeFunctor>

LIST_INTEGER_MAP_HYPOTHESES_TEMPLATE LIST_INTEGER_MAP_HYPOTHESES::list_integer_map_hypotheses(
    IFindElements& find_elements,
    IFindLiteralValue& find_literal_value,
    IConvertSamples& convert_samples,
    IProposeElementHypotheses& propose_element_hypotheses,
    IMakeFunctor& make_functor,
    list_symbols symbols)
    : find_elements_(find_elements)
    , find_literal_value_(find_literal_value)
    , convert_samples_(convert_samples)
    , propose_element_hypotheses_(propose_element_hypotheses)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , bodies_() {}

LIST_INTEGER_MAP_HYPOTHESES_TEMPLATE void LIST_INTEGER_MAP_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    std::map<integer, integer> outputs;
    for (const pw_data_point& point : points) {
        const std::optional<std::vector<const expr*>> inputs = find_elements_.find_elements(point.input);
        if (!inputs.has_value())
            continue;
        const std::optional<std::vector<const expr*>> results = find_elements_.find_elements(point.output);
        if (!results.has_value() || results.value().size() != inputs.value().size())
            continue;
        for (size_t index = 0; index < inputs.value().size(); ++index) {
            const std::optional<integer> input = find_literal_value_.find_value(inputs.value()[index]);
            const std::optional<integer> result = find_literal_value_.find_value(results.value()[index]);
            if (!input.has_value() || !result.has_value())
                continue;
            outputs.emplace(input.value(), result.value());
        }
    }
    if (outputs.empty())
        return;
    std::vector<poly_sample> samples;
    samples.reserve(outputs.size());
    for (const auto& [input, result] : outputs)
        samples.push_back(poly_sample{.inputs = {input}, .output = result});
    bodies_.clear();
    propose_element_hypotheses_.propose_hypotheses(convert_samples_.convert(samples), bodies_);
    const expr* list = make_functor_.make_functor(symbols_.input, {});
    for (const expr* body : bodies_)
        out.push_back(make_functor_.make_functor(symbols_.map, {make_functor_.make_functor(symbols_.lambda, {body}), list}));
}

#undef LIST_INTEGER_MAP_HYPOTHESES
#undef LIST_INTEGER_MAP_HYPOTHESES_TEMPLATE

#endif
