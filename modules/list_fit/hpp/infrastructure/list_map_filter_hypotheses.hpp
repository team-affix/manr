#ifndef LIST_MAP_FILTER_HYPOTHESES_HPP
#define LIST_MAP_FILTER_HYPOTHESES_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_data_point.hpp"

// A detector that reads list-to-list data as a composition, `map(f, filter(c, xs))`, split outer-inner
// (`docs/decomposing-detection.md`): each element value is either removed by the filter or kept and mapped to f of
// it, so the data gives f's values and the kept and removed values, and both go on to the readings that already
// exist. The signature reading seeds f's values from every list at once: a kept value v and its image f(v) appear in
// exactly the same lists (the filter keeps or removes by value), so an output value whose set of lists is that of
// exactly one input value is that value's image; values sharing a set of lists are ambiguous and left alone. This
// starts the reading on long lists, where every list mixes kept and removed values (reduce request #1). Then a point
// whose output is as long as its values not known to be removed keeps all of those, and pairs them
// with the output by position. A point whose output is exactly the images of its values known to be kept removes the
// rest (an empty output removes all). Points are revisited until
// nothing new is learned, since each one read can settle another. A point that contradicts what is known (a value
// both kept and removed, two images of one value) is ignored and stays as a branch.
// The kept and removed values must be split by a threshold, as for `list_filter_hypotheses`, and f's values go to the
// element domain; each body it proposes becomes `map(f, filter(c, xs))`. Nothing is proposed when no value is removed
// (a map) or f is the identity on every kept value (a filter): the map and filter detectors read those.
template<typename IFindElements,
         typename IFindLiteralValue,
         typename IConvertSamples,
         typename IProposeElementHypotheses,
         typename IMakeLiteral,
         typename IMakeFunctor>
struct list_map_filter_hypotheses {
    list_map_filter_hypotheses(IFindElements& find_elements,
                               IFindLiteralValue& find_literal_value,
                               IConvertSamples& convert_samples,
                               IProposeElementHypotheses& propose_element_hypotheses,
                               IMakeLiteral& make_literal,
                               IMakeFunctor& make_functor,
                               list_symbols symbols,
                               uint32_t less_symbol);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    std::optional<std::vector<integer>> find_values(const expr* list) const;

    IFindElements& find_elements_;
    IFindLiteralValue& find_literal_value_;
    IConvertSamples& convert_samples_;
    IProposeElementHypotheses& propose_element_hypotheses_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    list_symbols symbols_;
    uint32_t less_symbol_;
    std::vector<const expr*> bodies_;
};

#define LIST_MAP_FILTER_HYPOTHESES_TEMPLATE                                                           \
    template<typename IFindElements, typename IFindLiteralValue, typename IConvertSamples,          \
             typename IProposeElementHypotheses, typename IMakeLiteral, typename IMakeFunctor>
#define LIST_MAP_FILTER_HYPOTHESES                                                                    \
    list_map_filter_hypotheses<IFindElements, IFindLiteralValue, IConvertSamples, IProposeElementHypotheses, \
                               IMakeLiteral, IMakeFunctor>

LIST_MAP_FILTER_HYPOTHESES_TEMPLATE LIST_MAP_FILTER_HYPOTHESES::list_map_filter_hypotheses(
    IFindElements& find_elements,
    IFindLiteralValue& find_literal_value,
    IConvertSamples& convert_samples,
    IProposeElementHypotheses& propose_element_hypotheses,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    list_symbols symbols,
    uint32_t less_symbol)
    : find_elements_(find_elements)
    , find_literal_value_(find_literal_value)
    , convert_samples_(convert_samples)
    , propose_element_hypotheses_(propose_element_hypotheses)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , less_symbol_(less_symbol)
    , bodies_() {}

LIST_MAP_FILTER_HYPOTHESES_TEMPLATE void LIST_MAP_FILTER_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    std::vector<std::vector<integer>> inputs;
    std::vector<std::vector<integer>> outputs;
    for (const pw_data_point& point : points) {
        const std::optional<std::vector<integer>> input = find_values(point.input);
        if (!input.has_value())
            continue;
        const std::optional<std::vector<integer>> output = find_values(point.output);
        if (!output.has_value() || output.value().size() > input.value().size())
            continue;
        inputs.push_back(input.value());
        outputs.push_back(output.value());
    }
    std::map<integer, integer> images;
    std::set<integer> removed;
    // the signature reading: each value's set of lists, as one flag per list
    std::map<integer, std::vector<bool>> input_lists;
    std::map<integer, std::vector<bool>> output_lists;
    for (size_t index = 0; index < inputs.size(); ++index) {
        for (const integer& value : inputs[index])
            input_lists.try_emplace(value, inputs.size(), false).first->second[index] = true;
        for (const integer& value : outputs[index])
            output_lists.try_emplace(value, inputs.size(), false).first->second[index] = true;
    }
    std::map<std::vector<bool>, std::vector<integer>> values_by_lists;
    for (const auto& [value, lists] : input_lists)
        values_by_lists[lists].push_back(value);
    for (const auto& [image, lists] : output_lists) {
        const auto sources = values_by_lists.find(lists);
        const bool unique = sources != values_by_lists.end() && sources->second.size() == 1;
        if (!unique)
            continue;
        images.emplace(sources->second.front(), image);
    }
    std::vector<bool> read(inputs.size(), false);
    bool learned = true;
    while (learned) {
        learned = false;
        for (size_t index = 0; index < inputs.size(); ++index) {
            if (read[index])
                continue;
            const std::vector<integer>& input = inputs[index];
            const std::vector<integer>& output = outputs[index];
            // the candidates are the values not known to be removed; those with a known image are kept
            std::vector<integer> candidates;
            std::vector<integer> known_images;
            for (const integer& value : input) {
                if (removed.contains(value))
                    continue;
                candidates.push_back(value);
                const auto image = images.find(value);
                if (image != images.end())
                    known_images.push_back(image->second);
            }
            const bool keeps_all = output.size() == candidates.size();
            const bool keeps_only_known = output.size() == known_images.size();
            if (!keeps_all && !keeps_only_known)
                continue;
            read[index] = true;
            // check before learning, so a contradicting point teaches nothing
            bool consistent = keeps_all || known_images == output;
            for (size_t position = 0; position < candidates.size() && consistent && keeps_all; ++position) {
                const auto image = images.find(candidates[position]);
                consistent = image == images.end() || image->second == output[position];
            }
            if (!consistent)
                continue;
            for (size_t position = 0; position < candidates.size(); ++position) {
                const bool fresh = keeps_all ? images.emplace(candidates[position], output[position]).second
                                             : !images.contains(candidates[position]) &&
                                                   removed.insert(candidates[position]).second;
                learned = learned || fresh;
            }
        }
    }
    if (images.empty() || removed.empty())
        return;
    bool identity = true;
    for (const auto& [value, image] : images)
        identity = identity && value == image;
    if (identity)
        return;
    const expr* element = make_functor_.make_functor(symbols_.element, {});
    const bool above = *removed.rbegin() < images.begin()->first;
    const bool below = images.rbegin()->first < *removed.begin();
    if (!above && !below)
        return;
    const expr* condition =
        above ? make_functor_.make_functor(less_symbol_, {make_literal_.make_literal(*removed.rbegin()), element})
              : make_functor_.make_functor(less_symbol_, {element, make_literal_.make_literal(*removed.begin())});
    std::vector<poly_sample> samples;
    samples.reserve(images.size());
    for (const auto& [value, image] : images)
        samples.push_back(poly_sample{.inputs = {value}, .output = image});
    bodies_.clear();
    propose_element_hypotheses_.propose_hypotheses(convert_samples_.convert(samples), bodies_);
    const expr* kept = make_functor_.make_functor(symbols_.lambda, {condition});
    const expr* filtered =
        make_functor_.make_functor(symbols_.filter, {kept, make_functor_.make_functor(symbols_.input, {})});
    for (const expr* body : bodies_) {
        const expr* function = make_functor_.make_functor(symbols_.lambda, {body});
        out.push_back(make_functor_.make_functor(symbols_.map, {function, filtered}));
    }
}

// The literal values of a list of literals.
LIST_MAP_FILTER_HYPOTHESES_TEMPLATE std::optional<std::vector<integer>> LIST_MAP_FILTER_HYPOTHESES::find_values(
    const expr* list) const {
    const std::optional<std::vector<const expr*>> elements = find_elements_.find_elements(list);
    if (!elements.has_value())
        return std::nullopt;
    std::vector<integer> values;
    values.reserve(elements.value().size());
    for (const expr* element : elements.value()) {
        const std::optional<integer> value = find_literal_value_.find_value(element);
        if (!value.has_value())
            return std::nullopt;
        values.push_back(value.value());
    }
    return values;
}

#undef LIST_MAP_FILTER_HYPOTHESES
#undef LIST_MAP_FILTER_HYPOTHESES_TEMPLATE

#endif
