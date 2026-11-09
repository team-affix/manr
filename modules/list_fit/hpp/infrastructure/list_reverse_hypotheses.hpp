#ifndef LIST_REVERSE_HYPOTHESES_HPP
#define LIST_REVERSE_HYPOTHESES_HPP

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/pw_data_point.hpp"

// A detector that reads list-to-list data as the primitive `reverse(xs)`: proposed when more than half of the points
// map a list to its elements in reverse order. The fold that builds the same list
// (`list_listed_fold_hypotheses`) stays beside it, for the steps no primitive covers.
template<typename IFindElements, typename IMakeFunctor>
struct list_reverse_hypotheses {
    list_reverse_hypotheses(IFindElements& find_elements, IMakeFunctor& make_functor, list_symbols symbols);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    IFindElements& find_elements_;
    IMakeFunctor& make_functor_;
    list_symbols symbols_;
};

#define LIST_REVERSE_HYPOTHESES_TEMPLATE template<typename IFindElements, typename IMakeFunctor>
#define LIST_REVERSE_HYPOTHESES          list_reverse_hypotheses<IFindElements, IMakeFunctor>

LIST_REVERSE_HYPOTHESES_TEMPLATE LIST_REVERSE_HYPOTHESES::list_reverse_hypotheses(
    IFindElements& find_elements, IMakeFunctor& make_functor, list_symbols symbols)
    : find_elements_(find_elements)
    , make_functor_(make_functor)
    , symbols_(symbols) {}

LIST_REVERSE_HYPOTHESES_TEMPLATE void LIST_REVERSE_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    size_t lists = 0;
    size_t reversed = 0;
    bool changed = false;
    for (const pw_data_point& point : points) {
        const std::optional<std::vector<const expr*>> input = find_elements_.find_elements(point.input);
        const std::optional<std::vector<const expr*>> output = find_elements_.find_elements(point.output);
        if (!input.has_value() || !output.has_value())
            continue;
        ++lists;
        const bool matches = std::equal(input.value().rbegin(), input.value().rend(), output.value().begin(),
                                        output.value().end());
        reversed += matches ? 1 : 0;
        changed = changed || (matches && input.value() != output.value());
    }
    // every matching list a palindrome is the identity, which needs no reversal
    if (!changed || reversed <= std::max<size_t>(lists / 2, 1))
        return;
    out.push_back(make_functor_.make_functor(symbols_.reverse, {make_functor_.make_functor(symbols_.input, {})}));
}

#undef LIST_REVERSE_HYPOTHESES
#undef LIST_REVERSE_HYPOTHESES_TEMPLATE

#endif
