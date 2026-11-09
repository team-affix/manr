#ifndef LOGIC_PARITY_HYPOTHESES_HPP
#define LOGIC_PARITY_HYPOTHESES_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <utility>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/logic_parity.hpp"
#include "value_objects/logic_symbols.hpp"
#include "value_objects/logic_truth_table.hpp"
#include "value_objects/pw_data_point.hpp"

// A detector that reads truth-table data as a parity, the xor of some of the variables, possibly negated
// (`xor(xor(a, b), c)`, `not(xor(a, b))`): xor is linear over GF(2), so the solver finds it by an exact solve mod 2,
// and an inconsistent system refutes it. When it is inconsistent, each row is left out in turn, so a parity with one
// wrong label is still found (the binder keeps that row as a branch). A constant is left to the constant detector.
template<typename IFindTable, typename ISolveParity, typename IMakeFunctor>
struct logic_parity_hypotheses {
    logic_parity_hypotheses(IFindTable& find_table,
                            ISolveParity& solve_parity,
                            IMakeFunctor& make_functor,
                            logic_symbols symbols,
                            std::vector<uint32_t> variables);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    void propose_parity(const logic_parity& parity, std::vector<const expr*>& out);

    IFindTable& find_table_;
    ISolveParity& solve_parity_;
    IMakeFunctor& make_functor_;
    logic_symbols symbols_;
    std::vector<uint32_t> variables_;
};

#define LOGIC_PARITY_HYPOTHESES_TEMPLATE template<typename IFindTable, typename ISolveParity, typename IMakeFunctor>
#define LOGIC_PARITY_HYPOTHESES          logic_parity_hypotheses<IFindTable, ISolveParity, IMakeFunctor>

LOGIC_PARITY_HYPOTHESES_TEMPLATE LOGIC_PARITY_HYPOTHESES::logic_parity_hypotheses(
    IFindTable& find_table,
    ISolveParity& solve_parity,
    IMakeFunctor& make_functor,
    logic_symbols symbols,
    std::vector<uint32_t> variables)
    : find_table_(find_table)
    , solve_parity_(solve_parity)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , variables_(std::move(variables)) {}

LOGIC_PARITY_HYPOTHESES_TEMPLATE void LOGIC_PARITY_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    const std::optional<logic_truth_table> table = find_table_.find_table(points);
    if (!table.has_value())
        return;
    const std::optional<logic_parity> parity = solve_parity_.solve(table.value());
    if (parity.has_value()) {
        propose_parity(parity.value(), out);
        return;
    }
    std::set<logic_parity> found;
    for (const auto& [input, output] : table.value().rows) {
        logic_truth_table rest = table.value();
        rest.rows.erase(input);
        const std::optional<logic_parity> nearly = solve_parity_.solve(rest);
        if (!nearly.has_value() || !found.insert(nearly.value()).second)
            continue;
        propose_parity(nearly.value(), out);
    }
}

LOGIC_PARITY_HYPOTHESES_TEMPLATE void LOGIC_PARITY_HYPOTHESES::propose_parity(
    const logic_parity& parity, std::vector<const expr*>& out) {
    if (parity.variables == 0)
        return;
    const expr* formula = nullptr;
    for (size_t position = 0; position < variables_.size(); ++position) {
        if ((parity.variables & (uint64_t(1) << position)) == 0)
            continue;
        const expr* variable = make_functor_.make_functor(variables_[position], {});
        formula = formula == nullptr ? variable : make_functor_.make_functor(symbols_.exclusion, {formula, variable});
    }
    if (parity.inverted)
        formula = make_functor_.make_functor(symbols_.negation, {formula});
    out.push_back(formula);
}

#undef LOGIC_PARITY_HYPOTHESES
#undef LOGIC_PARITY_HYPOTHESES_TEMPLATE

#endif
