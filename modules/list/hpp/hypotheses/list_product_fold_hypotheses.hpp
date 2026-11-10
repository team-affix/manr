#ifndef LIST_PRODUCT_FOLD_HYPOTHESES_HPP
#define LIST_PRODUCT_FOLD_HYPOTHESES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/poly_monomial.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_power.hpp"
#include "value_objects/pw_data_point.hpp"

constexpr size_t k_list_default_max_factor_slope = 2;
constexpr size_t k_list_default_max_factor_offset = 3;

// A detector that reads list-to-integer data as a right fold whose step multiplies the accumulator by a linear factor
// of the head, `fold((a * h + k) * acc, b, #0)`: the output is b times one factor per element, so it reads whole lists
// and needs no tails. Factors are tried simplest first (a by magnitude 1, -1, 2, -2, ..., k by magnitude 0, 1, -1,
// ...), up to the limits; a = 0 is left to the affine detector, which covers `c * acc`. For each factor the base is
// the output of a starting list over its product (the first list, then one halfway along, so a fit can begin past a
// wrong label), when it divides. The fold reproducing the most lists is proposed when it reproduces more than half of
// them and at least two; the binder keeps the rest as branches. A fold reproducing every list ends the search.
template<typename IFindElements, typename IFindLiteralValue, typename IRender, typename IMakeLiteral,
         typename IMakeFunctor>
struct list_product_fold_hypotheses {
    list_product_fold_hypotheses(IFindElements& find_elements,
                                 IFindLiteralValue& find_literal_value,
                                 IRender& render,
                                 IMakeLiteral& make_literal,
                                 IMakeFunctor& make_functor,
                                 list_symbols symbols,
                                 size_t max_slope = k_list_default_max_factor_slope,
                                 size_t max_offset = k_list_default_max_factor_offset);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    integer multiply(const std::vector<integer>& list, const integer& slope, const integer& offset) const;

    IFindElements& find_elements_;
    IFindLiteralValue& find_literal_value_;
    IRender& render_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    list_symbols symbols_;
    size_t max_slope_;
    size_t max_offset_;
};

#define LIST_PRODUCT_FOLD_HYPOTHESES_TEMPLATE                                                      \
    template<typename IFindElements, typename IFindLiteralValue, typename IRender, typename IMakeLiteral, \
             typename IMakeFunctor>
#define LIST_PRODUCT_FOLD_HYPOTHESES \
    list_product_fold_hypotheses<IFindElements, IFindLiteralValue, IRender, IMakeLiteral, IMakeFunctor>

LIST_PRODUCT_FOLD_HYPOTHESES_TEMPLATE LIST_PRODUCT_FOLD_HYPOTHESES::list_product_fold_hypotheses(
    IFindElements& find_elements,
    IFindLiteralValue& find_literal_value,
    IRender& render,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    list_symbols symbols,
    size_t max_slope,
    size_t max_offset)
    : find_elements_(find_elements)
    , find_literal_value_(find_literal_value)
    , render_(render)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , max_slope_(max_slope)
    , max_offset_(max_offset) {}

LIST_PRODUCT_FOLD_HYPOTHESES_TEMPLATE void LIST_PRODUCT_FOLD_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    std::vector<std::vector<integer>> lists;
    std::vector<integer> outputs;
    for (const pw_data_point& point : points) {
        const std::optional<integer> output = find_literal_value_.find_value(point.output);
        if (!output.has_value())
            continue;
        const std::optional<std::vector<const expr*>> elements = find_elements_.find_elements(point.input);
        if (!elements.has_value())
            continue;
        std::vector<integer> list;
        for (const expr* element : elements.value()) {
            const std::optional<integer> value = find_literal_value_.find_value(element);
            if (!value.has_value())
                break;
            list.push_back(value.value());
        }
        if (list.size() != elements.value().size())
            continue;
        lists.push_back(std::move(list));
        outputs.push_back(output.value());
    }
    integer best_slope = 0;
    integer best_offset = 0;
    integer best_base = 0;
    size_t best_reproduced = std::max<size_t>(lists.size() / 2, 1);
    if (lists.size() <= best_reproduced)
        return;
    bool complete = false;
    for (size_t slope_candidate = 0; slope_candidate < 2 * max_slope_ && !complete; ++slope_candidate) {
        const integer slope_magnitude = slope_candidate / 2 + 1;
        const integer slope = slope_candidate % 2 == 1 ? integer(-slope_magnitude) : slope_magnitude;
        for (size_t offset_candidate = 0; offset_candidate <= 2 * max_offset_ && !complete; ++offset_candidate) {
            const integer offset_magnitude = (offset_candidate + 1) / 2;
            const integer offset = offset_candidate % 2 == 0 ? integer(-offset_magnitude) : offset_magnitude;
            for (size_t start : {size_t(0), lists.size() / 2}) {
                // the base: the first starting list's output over its product, when the product divides it
                std::optional<integer> base;
                for (size_t offset_index = 0; offset_index < lists.size() && !base.has_value(); ++offset_index) {
                    const size_t index = (start + offset_index) % lists.size();
                    const integer product = multiply(lists[index], slope, offset);
                    const bool readable = product != 0 && outputs[index] != 0;
                    if (!readable)
                        continue;
                    const bool divides = outputs[index] % product == 0;
                    if (!divides)
                        break;
                    base = outputs[index] / product;
                }
                if (!base.has_value())
                    continue;
                // a fold that cannot beat the best is abandoned at its first failure past the slack
                const size_t slack = lists.size() - best_reproduced - 1;
                size_t failures = 0;
                for (size_t index = 0; index < lists.size() && failures <= slack; ++index)
                    if (base.value() * multiply(lists[index], slope, offset) != outputs[index])
                        ++failures;
                if (failures > slack)
                    continue;
                best_slope = slope;
                best_offset = offset;
                best_base = base.value();
                best_reproduced = lists.size() - failures;
                complete = failures == 0;
                if (complete)
                    break;
            }
        }
    }
    if (best_slope == 0)
        return;
    const poly_power head{.variable = symbols_.head, .exponent = 1};
    const poly_power accumulator{.variable = symbols_.accumulator, .exponent = 1};
    poly_polynomial step;
    // a monomial's powers are sorted by variable id
    const bool head_first = symbols_.head < symbols_.accumulator;
    step.terms.emplace(poly_monomial{.powers = head_first ? std::vector<poly_power>{head, accumulator}
                                                          : std::vector<poly_power>{accumulator, head}},
                       best_slope);
    if (best_offset != 0)
        step.terms.emplace(poly_monomial{.powers = {accumulator}}, best_offset);
    const expr* body = render_.render(step);
    const expr* function =
        make_functor_.make_functor(symbols_.lambda, {make_functor_.make_functor(symbols_.lambda, {body})});
    out.push_back(make_functor_.make_functor(
        symbols_.fold,
        {function, make_literal_.make_literal(best_base), make_functor_.make_functor(symbols_.input, {})}));
}

// The product of the factor a * h + k over the list's elements.
LIST_PRODUCT_FOLD_HYPOTHESES_TEMPLATE integer LIST_PRODUCT_FOLD_HYPOTHESES::multiply(const std::vector<integer>& list,
                                                                                   const integer& slope,
                                                                                   const integer& offset) const {
    integer product = 1;
    for (const integer& element : list)
        product *= slope * element + offset;
    return product;
}

#undef LIST_PRODUCT_FOLD_HYPOTHESES
#undef LIST_PRODUCT_FOLD_HYPOTHESES_TEMPLATE

#endif
