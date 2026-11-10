#ifndef LOGIC_DECOMPOSITION_HYPOTHESES_HPP
#define LOGIC_DECOMPOSITION_HYPOTHESES_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/logic_symbols.hpp"
#include "value_objects/logic_truth_table.hpp"
#include "value_objects/pw_data_point.hpp"

// A detector that reads a truth table as a composition `c(g, x)`: an easy outer combiner c (`and`, `or`, `xor`) of an
// unknown inner function g and one variable x, plain or negated, split outer-inner (`docs/decomposing-detection.md`).
// Assuming c and x, every row gives what g must be there, or nothing: for `xor`, g = output xor x; for `and`, g =
// output where x holds, and x false must give false; for `or`, g = output where x fails, and x true must give true.
// A row that contradicts c refutes it. The rows g is not determined on are don't cares. g's table goes to the inner
// domain (primitives and parities), and each g it proposes becomes `and(g, x)`, `or(g, x)`, `xor(g, x)`, or with x
// negated `and(g, not(x))`, `imp(x, g)`, `xnor(g, x)`. A g that is constant is skipped: the outer reading alone covers
// it.
template<typename IFindTable, typename IProposeInner, typename IMakeLiteral, typename IMakeFunctor>
struct logic_decomposition_hypotheses {
    logic_decomposition_hypotheses(IFindTable& find_table,
                                   IProposeInner& propose_inner,
                                   IMakeLiteral& make_literal,
                                   IMakeFunctor& make_functor,
                                   logic_symbols symbols,
                                   std::vector<uint32_t> variables,
                                   uint32_t point_symbol);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    std::optional<std::vector<pw_data_point>> find_inner(const logic_truth_table& table, uint32_t combiner,
                                                         size_t position, bool plain);
    const expr* combine(uint32_t combiner, const expr* inner, size_t position, bool plain);

    IFindTable& find_table_;
    IProposeInner& propose_inner_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    logic_symbols symbols_;
    std::vector<uint32_t> variables_;
    uint32_t point_symbol_;
    std::vector<const expr*> inner_;
};

#define LOGIC_DECOMPOSITION_HYPOTHESES_TEMPLATE \
    template<typename IFindTable, typename IProposeInner, typename IMakeLiteral, typename IMakeFunctor>
#define LOGIC_DECOMPOSITION_HYPOTHESES \
    logic_decomposition_hypotheses<IFindTable, IProposeInner, IMakeLiteral, IMakeFunctor>

LOGIC_DECOMPOSITION_HYPOTHESES_TEMPLATE LOGIC_DECOMPOSITION_HYPOTHESES::logic_decomposition_hypotheses(
    IFindTable& find_table,
    IProposeInner& propose_inner,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    logic_symbols symbols,
    std::vector<uint32_t> variables,
    uint32_t point_symbol)
    : find_table_(find_table)
    , propose_inner_(propose_inner)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , variables_(std::move(variables))
    , point_symbol_(point_symbol)
    , inner_() {}

LOGIC_DECOMPOSITION_HYPOTHESES_TEMPLATE void LOGIC_DECOMPOSITION_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    const std::optional<logic_truth_table> table = find_table_.find_table(points);
    if (!table.has_value() || table.value().rows.empty())
        return;
    for (const uint32_t combiner : {symbols_.conjunction, symbols_.disjunction, symbols_.exclusion})
        for (size_t position = 0; position < variables_.size(); ++position)
            for (const bool plain : {true, false}) {
                const std::optional<std::vector<pw_data_point>> inner =
                    find_inner(table.value(), combiner, position, plain);
                if (!inner.has_value())
                    continue;
                inner_.clear();
                propose_inner_.propose_hypotheses(inner.value(), inner_);
                for (const expr* g : inner_)
                    out.push_back(combine(combiner, g, position, plain));
            }
}

// g's rows under the combiner and the variable, as data points; nothing when a row contradicts the combiner or g is
// constant on its rows.
LOGIC_DECOMPOSITION_HYPOTHESES_TEMPLATE std::optional<std::vector<pw_data_point>>
LOGIC_DECOMPOSITION_HYPOTHESES::find_inner(const logic_truth_table& table, uint32_t combiner, size_t position,
                                           bool plain) {
    std::vector<pw_data_point> inner;
    bool seen_true = false;
    bool seen_false = false;
    for (const auto& [row, output] : table.rows) {
        const bool bit = (row >> position) & 1;
        const bool literal = plain ? bit : !bit;
        std::optional<bool> value;
        if (combiner == symbols_.exclusion)
            value = output != literal;
        if (combiner == symbols_.conjunction && !literal && output)
            return std::nullopt;
        if (combiner == symbols_.conjunction && literal)
            value = output;
        if (combiner == symbols_.disjunction && literal && !output)
            return std::nullopt;
        if (combiner == symbols_.disjunction && !literal)
            value = output;
        if (!value.has_value())
            continue;
        seen_true = seen_true || value.value();
        seen_false = seen_false || !value.value();
        std::vector<const expr*> coordinates;
        coordinates.reserve(variables_.size());
        for (size_t index = 0; index < variables_.size(); ++index)
            coordinates.push_back(make_literal_.make_literal(integer((row >> index) & 1)));
        const expr* label = make_literal_.make_literal(integer(value.value() ? 1 : 0));
        const expr* input = make_functor_.make_functor(point_symbol_, coordinates);
        inner.push_back(pw_data_point{.input = input, .output = label});
    }
    const bool constant = !seen_true || !seen_false;
    if (constant)
        return std::nullopt;
    return inner;
}

LOGIC_DECOMPOSITION_HYPOTHESES_TEMPLATE const expr* LOGIC_DECOMPOSITION_HYPOTHESES::combine(
    uint32_t combiner, const expr* inner, size_t position, bool plain) {
    const expr* variable = make_functor_.make_functor(variables_[position], {});
    if (plain)
        return make_functor_.make_functor(combiner, {inner, variable});
    if (combiner == symbols_.exclusion)
        return make_functor_.make_functor(symbols_.equivalence, {inner, variable});
    if (combiner == symbols_.disjunction)
        return make_functor_.make_functor(symbols_.implication, {variable, inner});
    const expr* negated = make_functor_.make_functor(symbols_.negation, {variable});
    return make_functor_.make_functor(symbols_.conjunction, {inner, negated});
}

#undef LOGIC_DECOMPOSITION_HYPOTHESES
#undef LOGIC_DECOMPOSITION_HYPOTHESES_TEMPLATE

#endif
