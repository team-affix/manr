#ifndef LOGIC_FORMULA_REFITTER_HPP
#define LOGIC_FORMULA_REFITTER_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/logic_symbols.hpp"
#include "value_objects/pw_data_point.hpp"
#include "value_objects/rewrite.hpp"

constexpr size_t k_logic_default_max_refit_holes = 4;
constexpr size_t k_logic_default_max_refit_width = 8;

// Local rule: reduces a Boolean formula by its meaning. A subformula over the inputs (connectives, primitives, 0, 1
// and the input indices) is evaluated on every input row, and its truth table is fitted again by the logic detectors
// (primitives, compositions, parities, covers); the smallest fit that reproduces the table and is smaller than the
// subformula is proposed. Applied at every position, it recovers primitives and their compositions bottom-up
// (`or(and(#1, not(#0)), and(not(#1), #0))` becomes `xor(#1, #0)`).
//
// Free variables are holes it may bind: with holes in the subformula, a row where some choice of the holes' values
// gives 1 and another gives 0 is a don't care, since a binding of the holes to a function of the inputs can give
// either there. So `or(and(not(#1), #0), ?0)` becomes `xor(#1, #0)`, with `?0` bound to the missing half. Rows the
// holes cannot change are kept. This assumes the holes occur nowhere outside the subformula, as the extension binder
// does. The table has a row per input assignment, so this is a bounded rule, one among the structural ones: nothing
// over more than `max_width` inputs (2^width rows) or `max_holes` holes, for a subformula smaller than 3, or for
// anything outside the vocabulary.
template<typename IFindCost,
         typename IHoldsAt,
         typename IProposeHypotheses,
         typename IFindLiteralValue,
         typename IMakeLiteral,
         typename IMakeFunctor>
struct logic_formula_refitter {
    logic_formula_refitter(IFindCost& find_cost,
                           IHoldsAt& holds_at,
                           IProposeHypotheses& propose_hypotheses,
                           IFindLiteralValue& find_literal_value,
                           IMakeLiteral& make_literal,
                           IMakeFunctor& make_functor,
                           logic_symbols symbols,
                           std::vector<uint32_t> variables,
                           uint32_t point_symbol,
                           size_t max_width = k_logic_default_max_refit_width,
                           size_t max_holes = k_logic_default_max_refit_holes);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    const expr* find_refit(const expr* e);
    bool collect_holes(const expr* e, std::set<uint32_t>& holes) const;
    const expr* substitute(const expr* e, const std::vector<uint32_t>& holes, uint64_t assignment);
    std::optional<bool> find_constant(const std::vector<pw_data_point>& points);

    IFindCost& find_cost_;
    IHoldsAt& holds_at_;
    IProposeHypotheses& propose_hypotheses_;
    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    logic_symbols symbols_;
    std::vector<uint32_t> variables_;
    uint32_t point_symbol_;
    size_t max_width_;
    size_t max_holes_;
    std::unordered_map<const expr*, const expr*> refits_;
    std::vector<const expr*> hypotheses_;
};

#define LOGIC_FORMULA_REFITTER_TEMPLATE                                                                 \
    template<typename IFindCost, typename IHoldsAt, typename IProposeHypotheses, typename IFindLiteralValue, \
             typename IMakeLiteral, typename IMakeFunctor>
#define LOGIC_FORMULA_REFITTER                                                                              \
    logic_formula_refitter<IFindCost, IHoldsAt, IProposeHypotheses, IFindLiteralValue, IMakeLiteral, \
                           IMakeFunctor>

LOGIC_FORMULA_REFITTER_TEMPLATE LOGIC_FORMULA_REFITTER::logic_formula_refitter(IFindCost& find_cost,
                                                                               IHoldsAt& holds_at,
                                                                               IProposeHypotheses& propose_hypotheses,
                                                                               IFindLiteralValue& find_literal_value,
                                                                               IMakeLiteral& make_literal,
                                                                               IMakeFunctor& make_functor,
                                                                               logic_symbols symbols,
                                                                               std::vector<uint32_t> variables,
                                                                               uint32_t point_symbol,
                                                                               size_t max_width,
                                                                               size_t max_holes)
    : find_cost_(find_cost)
    , holds_at_(holds_at)
    , propose_hypotheses_(propose_hypotheses)
    , find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , variables_(std::move(variables))
    , point_symbol_(point_symbol)
    , max_width_(max_width)
    , max_holes_(max_holes)
    , refits_()
    , hypotheses_() {}

LOGIC_FORMULA_REFITTER_TEMPLATE void LOGIC_FORMULA_REFITTER::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.empty() || variables_.size() > max_width_)
        return;
    if (find_cost_.cost(e) < 3)
        return;
    const auto known = refits_.find(e);
    const expr* refit = known != refits_.end() ? known->second : find_refit(e);
    if (known == refits_.end())
        refits_.emplace(e, refit);
    if (refit == nullptr)
        return;
    out.push_back([refit]() { return refit; });
}

// The smallest fit of e's truth table, or nullptr when there is none smaller than e.
LOGIC_FORMULA_REFITTER_TEMPLATE const expr* LOGIC_FORMULA_REFITTER::find_refit(const expr* e) {
    std::set<uint32_t> hole_set;
    if (!collect_holes(e, hole_set) || hole_set.size() > max_holes_)
        return nullptr;
    const std::vector<uint32_t> holes(hole_set.begin(), hole_set.end());
    std::vector<const expr*> filled;
    for (uint64_t assignment = 0; assignment < (uint64_t(1) << holes.size()); ++assignment)
        filled.push_back(substitute(e, holes, assignment));
    // the rows the holes cannot change, as data points
    std::vector<pw_data_point> points;
    for (uint64_t row = 0; row < (uint64_t(1) << variables_.size()); ++row) {
        std::vector<const expr*> coordinates;
        coordinates.reserve(variables_.size());
        for (size_t index = 0; index < variables_.size(); ++index)
            coordinates.push_back(make_literal_.make_literal(integer((row >> index) & 1)));
        const expr* input = make_functor_.make_functor(point_symbol_, coordinates);
        bool seen_true = false;
        bool seen_false = false;
        for (const expr* formula : filled) {
            const std::optional<bool> truth = holds_at_.holds_at(formula, input);
            if (!truth.has_value())
                return nullptr;
            seen_true = seen_true || truth.value();
            seen_false = seen_false || !truth.value();
        }
        if (seen_true && seen_false)
            continue;
        const expr* output = make_literal_.make_literal(integer(seen_true ? 1 : 0));
        points.push_back(pw_data_point{.input = input, .output = output});
    }
    const size_t limit = find_cost_.cost(e);
    const std::optional<bool> constant = find_constant(points);
    if (constant.has_value())
        return make_literal_.make_literal(integer(constant.value() ? 1 : 0));
    hypotheses_.clear();
    propose_hypotheses_.propose_hypotheses(points, hypotheses_);
    const expr* best = nullptr;
    size_t best_cost = limit;
    for (const expr* candidate : hypotheses_) {
        const size_t cost = find_cost_.cost(candidate);
        if (cost >= best_cost)
            continue;
        bool exact = true;
        for (const pw_data_point& point : points) {
            const std::optional<bool> truth = holds_at_.holds_at(candidate, point.input);
            const integer expected = find_literal_value_.find_value(point.output).value();
            exact = exact && truth.has_value() && (truth.value() ? 1 : 0) == expected;
            if (!exact)
                break;
        }
        if (!exact)
            continue;
        best = candidate;
        best_cost = cost;
    }
    return best;
}

// Whether e is in the vocabulary, collecting the holes it holds.
LOGIC_FORMULA_REFITTER_TEMPLATE bool LOGIC_FORMULA_REFITTER::collect_holes(const expr* e,
                                                                           std::set<uint32_t>& holes) const {
    const expr::var* hole = std::get_if<expr::var>(&e->content);
    if (hole != nullptr) {
        holes.insert(hole->index);
        return true;
    }
    const expr::functor& f = std::get<expr::functor>(e->content);
    if (f.args.empty())
        return true;
    const bool connective = f.id == symbols_.conjunction || f.id == symbols_.disjunction || f.id == symbols_.exclusion
                            || f.id == symbols_.negation || f.id == symbols_.equivalence
                            || f.id == symbols_.alternative_denial || f.id == symbols_.joint_denial
                            || f.id == symbols_.implication || f.id == symbols_.majority;
    if (!connective)
        return false;
    for (const expr* arg : f.args)
        if (!collect_holes(arg, holes))
            return false;
    return true;
}

// e with the i-th hole replaced by bit i of `assignment`.
LOGIC_FORMULA_REFITTER_TEMPLATE const expr* LOGIC_FORMULA_REFITTER::substitute(
    const expr* e, const std::vector<uint32_t>& holes, uint64_t assignment) {
    const expr::var* hole = std::get_if<expr::var>(&e->content);
    if (hole != nullptr) {
        size_t index = 0;
        while (holes[index] != hole->index)
            ++index;
        return make_literal_.make_literal(integer((assignment >> index) & 1));
    }
    const expr::functor& f = std::get<expr::functor>(e->content);
    if (f.args.empty())
        return e;
    std::vector<const expr*> args;
    args.reserve(f.args.size());
    for (const expr* arg : f.args)
        args.push_back(substitute(arg, holes, assignment));
    return make_functor_.make_functor(f.id, args);
}

// The one output every point has, if they all agree (none counts as 0: the holes can give anything).
LOGIC_FORMULA_REFITTER_TEMPLATE std::optional<bool> LOGIC_FORMULA_REFITTER::find_constant(
    const std::vector<pw_data_point>& points) {
    if (points.empty())
        return false;
    for (const pw_data_point& point : points)
        if (point.output != points.front().output)
            return std::nullopt;
    return find_literal_value_.find_value(points.front().output).value() == 1;
}

#undef LOGIC_FORMULA_REFITTER
#undef LOGIC_FORMULA_REFITTER_TEMPLATE

#endif
