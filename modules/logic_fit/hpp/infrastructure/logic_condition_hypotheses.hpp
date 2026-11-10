#ifndef LOGIC_CONDITION_HYPOTHESES_HPP
#define LOGIC_CONDITION_HYPOTHESES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/logic_symbols.hpp"
#include "value_objects/pw_data_point.hpp"

constexpr size_t k_logic_default_max_condition_proposals = 4;

// A detector for condition data (`point(v) -> 1` where a condition holds, `-> 0` where it fails) as a Boolean
// function of one or two atomic predicates. The atoms come from another domain (thresholds, residues, primality),
// split outer-inner (`docs/decomposing-detection.md`): each atom is decided at every point, which turns the data into
// a truth table over the atoms. One atom, plain or negated, that reproduces every label is proposed first. Otherwise
// every pair of atoms is read as one of the two-input connectives (`and`, `or`, `xor`, `xnor`, `nand`, `nor`, `imp`
// either way, `and` with one operand negated) that reproduces every label, where a pair of atoms that gives two
// labels for the same pair of truth values refutes the pair. The smallest readings are proposed, at most
// `max_proposals`.
template<typename IProposeAtoms, typename IHoldsAt, typename IFindCost, typename IFindLiteralValue,
         typename IMakeFunctor>
struct logic_condition_hypotheses {
    logic_condition_hypotheses(IProposeAtoms& propose_atoms,
                               IHoldsAt& holds_at,
                               IFindCost& find_cost,
                               IFindLiteralValue& find_literal_value,
                               IMakeFunctor& make_functor,
                               logic_symbols symbols,
                               size_t max_proposals = k_logic_default_max_condition_proposals);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    struct connective {
        uint32_t id;
        bool swap;
        bool negate_left;
        bool negate_right;
    };
    bool compute(const connective& kind, bool left, bool right) const;
    const expr* render(const connective& kind, const expr* left, const expr* right);

    IProposeAtoms& propose_atoms_;
    IHoldsAt& holds_at_;
    IFindCost& find_cost_;
    IFindLiteralValue& find_literal_value_;
    IMakeFunctor& make_functor_;
    logic_symbols symbols_;
    size_t max_proposals_;
    std::vector<connective> connectives_;
    std::vector<const expr*> atoms_;
};

#define LOGIC_CONDITION_HYPOTHESES_TEMPLATE                                                                   \
    template<typename IProposeAtoms, typename IHoldsAt, typename IFindCost, typename IFindLiteralValue, \
             typename IMakeFunctor>
#define LOGIC_CONDITION_HYPOTHESES \
    logic_condition_hypotheses<IProposeAtoms, IHoldsAt, IFindCost, IFindLiteralValue, IMakeFunctor>

LOGIC_CONDITION_HYPOTHESES_TEMPLATE LOGIC_CONDITION_HYPOTHESES::logic_condition_hypotheses(
    IProposeAtoms& propose_atoms,
    IHoldsAt& holds_at,
    IFindCost& find_cost,
    IFindLiteralValue& find_literal_value,
    IMakeFunctor& make_functor,
    logic_symbols symbols,
    size_t max_proposals)
    : propose_atoms_(propose_atoms)
    , holds_at_(holds_at)
    , find_cost_(find_cost)
    , find_literal_value_(find_literal_value)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , max_proposals_(max_proposals)
    , connectives_{{.id = symbols.conjunction, .swap = false, .negate_left = false, .negate_right = false},
                   {.id = symbols.disjunction, .swap = false, .negate_left = false, .negate_right = false},
                   {.id = symbols.exclusion, .swap = false, .negate_left = false, .negate_right = false},
                   {.id = symbols.equivalence, .swap = false, .negate_left = false, .negate_right = false},
                   {.id = symbols.alternative_denial, .swap = false, .negate_left = false, .negate_right = false},
                   {.id = symbols.joint_denial, .swap = false, .negate_left = false, .negate_right = false},
                   {.id = symbols.implication, .swap = false, .negate_left = false, .negate_right = false},
                   {.id = symbols.implication, .swap = true, .negate_left = false, .negate_right = false},
                   {.id = symbols.conjunction, .swap = false, .negate_left = false, .negate_right = true},
                   {.id = symbols.conjunction, .swap = false, .negate_left = true, .negate_right = false}}
    , atoms_() {}

LOGIC_CONDITION_HYPOTHESES_TEMPLATE void LOGIC_CONDITION_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    std::vector<bool> labels;
    labels.reserve(points.size());
    for (const pw_data_point& point : points) {
        const std::optional<integer> label = find_literal_value_.find_value(point.output);
        if (!label.has_value() || (label.value() != 0 && label.value() != 1))
            return;
        labels.push_back(label.value() == 1);
    }
    if (labels.empty())
        return;
    atoms_.clear();
    propose_atoms_.propose_atoms(points, atoms_);
    // each atom's truth at every point; an atom undecided somewhere is dropped
    std::vector<const expr*> atoms;
    std::vector<std::vector<bool>> truths;
    for (const expr* atom : atoms_) {
        std::vector<bool> truth;
        truth.reserve(points.size());
        for (const pw_data_point& point : points) {
            const std::optional<bool> holds = holds_at_.holds_at(atom, point.input);
            if (!holds.has_value())
                break;
            truth.push_back(holds.value());
        }
        if (truth.size() != points.size())
            continue;
        atoms.push_back(atom);
        truths.push_back(truth);
    }
    std::vector<std::pair<size_t, const expr*>> found;
    for (size_t index = 0; index < atoms.size(); ++index) {
        std::vector<bool> negated(truths[index].size());
        for (size_t point = 0; point < negated.size(); ++point)
            negated[point] = !truths[index][point];
        if (truths[index] == labels)
            found.emplace_back(find_cost_.cost(atoms[index]), atoms[index]);
        if (negated == labels) {
            const expr* condition = make_functor_.make_functor(symbols_.negation, {atoms[index]});
            found.emplace_back(find_cost_.cost(condition), condition);
        }
    }
    const bool single = !found.empty();
    for (size_t first = 0; !single && first < atoms.size(); ++first)
        for (size_t second = first + 1; second < atoms.size(); ++second) {
            // the label each pair of truth values gives, or a conflict
            std::optional<bool> given[2][2];
            bool conflict = false;
            for (size_t point = 0; point < labels.size() && !conflict; ++point) {
                std::optional<bool>& slot = given[truths[first][point]][truths[second][point]];
                conflict = slot.has_value() && slot.value() != labels[point];
                slot = labels[point];
            }
            if (conflict)
                continue;
            for (const connective& kind : connectives_) {
                bool reproduces = true;
                for (const bool left : {false, true})
                    for (const bool right : {false, true})
                        reproduces = reproduces && (!given[left][right].has_value()
                                                    || compute(kind, left, right) == given[left][right].value());
                if (!reproduces)
                    continue;
                const expr* condition = render(kind, atoms[first], atoms[second]);
                found.emplace_back(find_cost_.cost(condition), condition);
            }
        }
    std::sort(found.begin(), found.end());
    for (size_t index = 0; index < found.size() && index < max_proposals_; ++index)
        out.push_back(found[index].second);
}

// The connective's value for its two atoms' truth values, in the order the atoms were paired.
LOGIC_CONDITION_HYPOTHESES_TEMPLATE bool LOGIC_CONDITION_HYPOTHESES::compute(const connective& kind, bool left,
                                                                             bool right) const {
    const bool first = (kind.swap ? right : left) != kind.negate_left;
    const bool second = (kind.swap ? left : right) != kind.negate_right;
    if (kind.id == symbols_.conjunction)
        return first && second;
    if (kind.id == symbols_.disjunction)
        return first || second;
    if (kind.id == symbols_.exclusion)
        return first != second;
    if (kind.id == symbols_.equivalence)
        return first == second;
    if (kind.id == symbols_.alternative_denial)
        return !(first && second);
    if (kind.id == symbols_.joint_denial)
        return !(first || second);
    return !first || second;
}

LOGIC_CONDITION_HYPOTHESES_TEMPLATE const expr* LOGIC_CONDITION_HYPOTHESES::render(const connective& kind,
                                                                                   const expr* left,
                                                                                   const expr* right) {
    const expr* first = kind.swap ? right : left;
    const expr* second = kind.swap ? left : right;
    if (kind.negate_left)
        first = make_functor_.make_functor(symbols_.negation, {first});
    if (kind.negate_right)
        second = make_functor_.make_functor(symbols_.negation, {second});
    return make_functor_.make_functor(kind.id, {first, second});
}

#undef LOGIC_CONDITION_HYPOTHESES
#undef LOGIC_CONDITION_HYPOTHESES_TEMPLATE

#endif
