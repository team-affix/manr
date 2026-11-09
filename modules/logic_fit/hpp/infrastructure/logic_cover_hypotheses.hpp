#ifndef LOGIC_COVER_HYPOTHESES_HPP
#define LOGIC_COVER_HYPOTHESES_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/logic_implicant.hpp"
#include "value_objects/logic_symbols.hpp"
#include "value_objects/logic_truth_table.hpp"
#include "value_objects/pw_data_point.hpp"

// A detector that reads truth-table data as a sum of products: the cover of the rows whose output is 1 (from the
// cover solver, with unseen inputs as don't cares), and the negated cover of the rows whose output is 0. Each cover
// is factored as it is rendered: the literal shared by the most products is pulled out of them,
// `or(and(l, rest of those), the others)`, recursively, so majority becomes `or(and(a, or(b, c)), and(b, c))`. A
// constant is left to the constant detector.
template<typename IFindTable, typename IFindCover, typename IMakeFunctor>
struct logic_cover_hypotheses {
    logic_cover_hypotheses(IFindTable& find_table,
                           IFindCover& find_cover,
                           IMakeFunctor& make_functor,
                           logic_symbols symbols,
                           std::vector<uint32_t> variables);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    const expr* render(const std::vector<logic_implicant>& products);
    const expr* render_literal(size_t position, bool plain);

    IFindTable& find_table_;
    IFindCover& find_cover_;
    IMakeFunctor& make_functor_;
    logic_symbols symbols_;
    std::vector<uint32_t> variables_;
};

#define LOGIC_COVER_HYPOTHESES_TEMPLATE template<typename IFindTable, typename IFindCover, typename IMakeFunctor>
#define LOGIC_COVER_HYPOTHESES          logic_cover_hypotheses<IFindTable, IFindCover, IMakeFunctor>

LOGIC_COVER_HYPOTHESES_TEMPLATE LOGIC_COVER_HYPOTHESES::logic_cover_hypotheses(
    IFindTable& find_table,
    IFindCover& find_cover,
    IMakeFunctor& make_functor,
    logic_symbols symbols,
    std::vector<uint32_t> variables)
    : find_table_(find_table)
    , find_cover_(find_cover)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , variables_(std::move(variables)) {}

LOGIC_COVER_HYPOTHESES_TEMPLATE void LOGIC_COVER_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    const std::optional<logic_truth_table> table = find_table_.find_table(points);
    if (!table.has_value())
        return;
    for (const bool output : {true, false}) {
        const std::vector<logic_implicant> cover = find_cover_.find_cover(table.value(), output);
        bool constant = cover.empty();
        for (const logic_implicant& product : cover)
            constant = constant || product.mask == 0;
        if (constant)
            continue;
        const expr* formula = render(cover);
        out.push_back(output ? formula : make_functor_.make_functor(symbols_.negation, {formula}));
    }
}

// The products as a formula, factoring out the literal shared by the most of them while one is shared.
LOGIC_COVER_HYPOTHESES_TEMPLATE const expr* LOGIC_COVER_HYPOTHESES::render(
    const std::vector<logic_implicant>& products) {
    size_t best_position = 0;
    bool best_plain = true;
    size_t best_count = 0;
    for (size_t position = 0; position < variables_.size(); ++position)
        for (const bool plain : {true, false}) {
            const uint64_t bit = uint64_t(1) << position;
            size_t count = 0;
            for (const logic_implicant& product : products)
                count += (product.mask & bit) != 0 && ((product.bits & bit) != 0) == plain ? 1 : 0;
            if (count <= best_count)
                continue;
            best_position = position;
            best_plain = plain;
            best_count = count;
        }
    if (best_count < 2) {
        const expr* sum = nullptr;
        for (const logic_implicant& product : products) {
            const expr* term = nullptr;
            for (size_t position = 0; position < variables_.size(); ++position) {
                const uint64_t bit = uint64_t(1) << position;
                if ((product.mask & bit) == 0)
                    continue;
                const expr* literal = render_literal(position, (product.bits & bit) != 0);
                term = term == nullptr ? literal : make_functor_.make_functor(symbols_.conjunction, {term, literal});
            }
            sum = sum == nullptr ? term : make_functor_.make_functor(symbols_.disjunction, {sum, term});
        }
        return sum;
    }
    const uint64_t bit = uint64_t(1) << best_position;
    std::vector<logic_implicant> sharing;
    std::vector<logic_implicant> others;
    bool absorbed = false;
    for (const logic_implicant& product : products) {
        const bool shares = (product.mask & bit) != 0 && ((product.bits & bit) != 0) == best_plain;
        if (!shares) {
            others.push_back(product);
            continue;
        }
        const logic_implicant rest{.mask = product.mask & ~bit, .bits = product.bits & ~bit};
        absorbed = absorbed || rest.mask == 0;
        sharing.push_back(rest);
    }
    const expr* literal = render_literal(best_position, best_plain);
    const expr* factored =
        absorbed ? literal : make_functor_.make_functor(symbols_.conjunction, {literal, render(sharing)});
    if (others.empty())
        return factored;
    return make_functor_.make_functor(symbols_.disjunction, {factored, render(others)});
}

LOGIC_COVER_HYPOTHESES_TEMPLATE const expr* LOGIC_COVER_HYPOTHESES::render_literal(size_t position, bool plain) {
    const expr* variable = make_functor_.make_functor(variables_[position], {});
    return plain ? variable : make_functor_.make_functor(symbols_.negation, {variable});
}

#undef LOGIC_COVER_HYPOTHESES
#undef LOGIC_COVER_HYPOTHESES_TEMPLATE

#endif
