#ifndef LIST_AFFINE_FOLD_HYPOTHESES_HPP
#define LIST_AFFINE_FOLD_HYPOTHESES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/list_affine_fold.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/poly_monomial.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_power.hpp"
#include "value_objects/pw_data_point.hpp"

constexpr size_t k_list_default_max_fold_degree = 6;
constexpr size_t k_list_default_max_multiplier = 10;
constexpr size_t k_list_default_max_fold_starts = 2;

// A detector that reads list-to-integer data as a right fold whose step is affine in the accumulator,
// `fold(p(h) + c * acc, b, xs)`, from whole lists: unlike `list_integer_fold_hypotheses` it needs neither tails nor
// the empty list in the data, since for a fixed multiplier c the solver pins p and b from the lists' outputs alone
// (`list_affine_fold_solver`). Multipliers are tried by magnitude (1, -1, 2, -2, ...) up to the limit, first from the
// first list, then from later starting lists so a fit can begin past a wrong label. The fold reproducing the most
// lists is proposed, rendered as a polynomial step, when it reproduces more than half of them and more lists than it
// has unknowns; the binder keeps the rest as branches. A fold reproducing every list ends the search.
template<typename IFindElements,
         typename IFindLiteralValue,
         typename ISolveFold,
         typename IAccumulateFold,
         typename IRender,
         typename IMakeLiteral,
         typename IMakeFunctor>
struct list_affine_fold_hypotheses {
    list_affine_fold_hypotheses(IFindElements& find_elements,
                                IFindLiteralValue& find_literal_value,
                                ISolveFold& solve_fold,
                                IAccumulateFold& accumulate_fold,
                                IRender& render,
                                IMakeLiteral& make_literal,
                                IMakeFunctor& make_functor,
                                list_symbols symbols,
                                size_t max_degree = k_list_default_max_fold_degree,
                                size_t max_multiplier = k_list_default_max_multiplier,
                                size_t max_starts = k_list_default_max_fold_starts);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    std::optional<list_affine_fold> find_fold(const std::vector<std::vector<integer>>& lists,
                                              const std::vector<integer>& outputs) const;

    IFindElements& find_elements_;
    IFindLiteralValue& find_literal_value_;
    ISolveFold& solve_fold_;
    IAccumulateFold& accumulate_fold_;
    IRender& render_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    list_symbols symbols_;
    size_t max_degree_;
    size_t max_multiplier_;
    size_t max_starts_;
};

#define LIST_AFFINE_FOLD_HYPOTHESES_TEMPLATE                                                                      \
    template<typename IFindElements, typename IFindLiteralValue, typename ISolveFold, typename IAccumulateFold, \
             typename IRender, typename IMakeLiteral, typename IMakeFunctor>
#define LIST_AFFINE_FOLD_HYPOTHESES                                                                  \
    list_affine_fold_hypotheses<IFindElements, IFindLiteralValue, ISolveFold, IAccumulateFold, IRender, \
                                IMakeLiteral, IMakeFunctor>

LIST_AFFINE_FOLD_HYPOTHESES_TEMPLATE LIST_AFFINE_FOLD_HYPOTHESES::list_affine_fold_hypotheses(
    IFindElements& find_elements,
    IFindLiteralValue& find_literal_value,
    ISolveFold& solve_fold,
    IAccumulateFold& accumulate_fold,
    IRender& render,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    list_symbols symbols,
    size_t max_degree,
    size_t max_multiplier,
    size_t max_starts)
    : find_elements_(find_elements)
    , find_literal_value_(find_literal_value)
    , solve_fold_(solve_fold)
    , accumulate_fold_(accumulate_fold)
    , render_(render)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , max_degree_(max_degree)
    , max_multiplier_(max_multiplier)
    , max_starts_(max_starts) {}

LIST_AFFINE_FOLD_HYPOTHESES_TEMPLATE void LIST_AFFINE_FOLD_HYPOTHESES::propose_hypotheses(
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
    const std::optional<list_affine_fold> fold = find_fold(lists, outputs);
    if (!fold.has_value())
        return;
    poly_polynomial step;
    for (size_t exponent = 0; exponent < fold.value().coefficients.size(); ++exponent) {
        const integer& coefficient = fold.value().coefficients[exponent];
        if (coefficient == 0)
            continue;
        poly_monomial monomial;
        if (exponent > 0)
            monomial.powers.push_back(poly_power{.variable = symbols_.head, .exponent = static_cast<uint32_t>(exponent)});
        step.terms.emplace(monomial, coefficient);
    }
    step.terms.emplace(poly_monomial{.powers = {poly_power{.variable = symbols_.accumulator, .exponent = 1}}},
                       fold.value().multiplier);
    out.push_back(make_functor_.make_functor(symbols_.fold,
                                             {render_.render(step), make_literal_.make_literal(fold.value().base),
                                              make_functor_.make_functor(symbols_.input, {})}));
}

// The fold reproducing the most lists, among those reproducing more than half of them and more than any fold's
// unknowns; nothing when there is none.
LIST_AFFINE_FOLD_HYPOTHESES_TEMPLATE std::optional<list_affine_fold> LIST_AFFINE_FOLD_HYPOTHESES::find_fold(
    const std::vector<std::vector<integer>>& lists, const std::vector<integer>& outputs) const {
    const size_t most_unknowns = max_degree_ + 2;
    std::optional<list_affine_fold> best;
    size_t best_reproduced = std::max(lists.size() / 2, most_unknowns);
    if (lists.size() <= best_reproduced)
        return std::nullopt;
    const size_t starts = std::min(max_starts_, lists.size());
    for (size_t start = 0; start < starts; ++start) {
        for (size_t candidate = 0; candidate < 2 * max_multiplier_; ++candidate) {
            const integer magnitude = candidate / 2 + 1;
            const bool negative = candidate % 2 == 1;
            const integer multiplier = negative ? integer(-magnitude) : magnitude;
            const std::optional<list_affine_fold> fold =
                solve_fold_.solve(lists, outputs, multiplier, max_degree_, start * lists.size() / starts);
            if (!fold.has_value())
                continue;
            // a fold that cannot beat the best is abandoned at its first failure past the slack
            const size_t slack = lists.size() - best_reproduced - 1;
            size_t failures = 0;
            for (size_t index = 0; index < lists.size() && failures <= slack; ++index) {
                const bool reproduced = accumulate_fold_.accumulate(fold.value(), lists[index])[0] == outputs[index];
                if (!reproduced)
                    ++failures;
            }
            if (failures > slack)
                continue;
            best = fold;
            best_reproduced = lists.size() - failures;
            if (failures == 0)
                return best;
        }
    }
    return best;
}

#undef LIST_AFFINE_FOLD_HYPOTHESES
#undef LIST_AFFINE_FOLD_HYPOTHESES_TEMPLATE

#endif
