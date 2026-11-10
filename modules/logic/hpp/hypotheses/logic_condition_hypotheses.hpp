#ifndef LOGIC_CONDITION_HYPOTHESES_HPP
#define LOGIC_CONDITION_HYPOTHESES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <utility>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/logic_symbols.hpp"
#include "value_objects/pw_data_point.hpp"

constexpr size_t k_logic_default_max_condition_proposals = 4;
constexpr size_t k_logic_default_max_condition_starts = 8;
constexpr size_t k_logic_default_max_condition_triples = 200000;

// A detector for condition data (`point(v) -> 1` where a condition holds, `-> 0` where it fails) as a Boolean
// function of one or two atomic predicates. The atoms come from another domain (thresholds, residues, primality),
// split outer-inner (`docs/decomposing-detection.md`): each atom is decided at every point, which turns the data into
// a truth table over the atoms. One atom, plain or negated, that reproduces every label is proposed first. Otherwise
// every pair of atoms is read as one of the two-input connectives (`and`, `or`, `xor`, `xnor`, `nand`, `nor`, `imp`
// either way, `and` with one operand negated) that reproduces every label, where a pair of atoms that gives two
// labels for the same pair of truth values refutes the pair.
//
// Beyond pairs, any Boolean function of several atoms: atoms are chosen greedily until every kept value is told apart
// from every removed one (each time the atom that separates the most pairs still joined), at most one per placeholder
// input, starting once from each of the `max_starts` atoms that separate the most pairs. A greedy choice misses the
// atoms of an xor (each alone separates only half the pairs), so when it needs more than three atoms, every triple of
// atoms is also checked (one pass each, at most `max_triples`), and the cheapest separating triples are kept. The
// chosen atoms' truth values make a truth table with unseen combinations as don't cares, fitted by the logic domain
// (`IProposeFormulas`: primitives, parities, covers, compositions of any depth) over placeholder inputs, which are
// then replaced by the atoms. The smallest readings that reproduce every label are proposed, at most
// `max_proposals`.
template<typename IProposeAtoms, typename IHoldsAt, typename IProposeFormulas, typename IFindCost,
         typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
struct logic_condition_hypotheses {
    logic_condition_hypotheses(IProposeAtoms& propose_atoms,
                               IHoldsAt& holds_at,
                               IProposeFormulas& propose_formulas,
                               IFindCost& find_cost,
                               IFindLiteralValue& find_literal_value,
                               IMakeLiteral& make_literal,
                               IMakeFunctor& make_functor,
                               logic_symbols symbols,
                               std::vector<uint32_t> placeholders,
                               uint32_t point_symbol,
                               size_t max_proposals = k_logic_default_max_condition_proposals,
                               size_t max_starts = k_logic_default_max_condition_starts,
                               size_t max_triples = k_logic_default_max_condition_triples);
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
    void propose_separated(const std::vector<pw_data_point>& points, const std::vector<const expr*>& atoms,
                           const std::vector<std::vector<bool>>& truths, const std::vector<bool>& labels,
                           std::vector<std::pair<size_t, const expr*>>& found);
    std::optional<std::vector<size_t>> find_greedy(size_t start, const std::vector<const expr*>& atoms,
                                                   const std::vector<std::vector<bool>>& truths,
                                                   const std::vector<std::pair<size_t, size_t>>& joined) const;
    std::vector<std::vector<size_t>> find_triples(const std::vector<const expr*>& atoms,
                                                  const std::vector<std::vector<bool>>& truths,
                                                  const std::vector<bool>& labels);
    void fit_chosen(const std::vector<size_t>& chosen, const std::vector<pw_data_point>& points,
                    const std::vector<const expr*>& atoms, const std::vector<std::vector<bool>>& truths,
                    const std::vector<bool>& labels, std::vector<std::pair<size_t, const expr*>>& found);
    const expr* substitute(const expr* formula, const std::vector<const expr*>& chosen);

    IProposeAtoms& propose_atoms_;
    IHoldsAt& holds_at_;
    IProposeFormulas& propose_formulas_;
    IFindCost& find_cost_;
    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    logic_symbols symbols_;
    std::vector<uint32_t> placeholders_;
    uint32_t point_symbol_;
    size_t max_proposals_;
    size_t max_starts_;
    size_t max_triples_;
    std::vector<connective> connectives_;
    std::vector<const expr*> atoms_;
};

#define LOGIC_CONDITION_HYPOTHESES_TEMPLATE                                                                \
    template<typename IProposeAtoms, typename IHoldsAt, typename IProposeFormulas, typename IFindCost, \
             typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
#define LOGIC_CONDITION_HYPOTHESES                                                                       \
    logic_condition_hypotheses<IProposeAtoms, IHoldsAt, IProposeFormulas, IFindCost, IFindLiteralValue, \
                               IMakeLiteral, IMakeFunctor>

LOGIC_CONDITION_HYPOTHESES_TEMPLATE LOGIC_CONDITION_HYPOTHESES::logic_condition_hypotheses(
    IProposeAtoms& propose_atoms,
    IHoldsAt& holds_at,
    IProposeFormulas& propose_formulas,
    IFindCost& find_cost,
    IFindLiteralValue& find_literal_value,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    logic_symbols symbols,
    std::vector<uint32_t> placeholders,
    uint32_t point_symbol,
    size_t max_proposals,
    size_t max_starts,
    size_t max_triples)
    : propose_atoms_(propose_atoms)
    , holds_at_(holds_at)
    , propose_formulas_(propose_formulas)
    , find_cost_(find_cost)
    , find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , placeholders_(std::move(placeholders))
    , point_symbol_(point_symbol)
    , max_proposals_(max_proposals)
    , max_starts_(max_starts)
    , max_triples_(max_triples)
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
    if (!single)
        propose_separated(points, atoms, truths, labels, found);
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

// The atoms that separate every kept value from every removed one, chosen greedily, and the logic domain's exact fits
// of the labels over them.
LOGIC_CONDITION_HYPOTHESES_TEMPLATE void LOGIC_CONDITION_HYPOTHESES::propose_separated(
    const std::vector<pw_data_point>& points, const std::vector<const expr*>& atoms,
    const std::vector<std::vector<bool>>& truths, const std::vector<bool>& labels,
    std::vector<std::pair<size_t, const expr*>>& found) {
    std::vector<std::pair<size_t, size_t>> joined;
    for (size_t kept = 0; kept < labels.size(); ++kept)
        for (size_t removed = 0; removed < labels.size(); ++removed)
            if (labels[kept] && !labels[removed])
                joined.emplace_back(kept, removed);
    // the atoms by how many pairs they separate, most first
    std::vector<std::pair<size_t, size_t>> ranked;
    for (size_t atom = 0; atom < atoms.size(); ++atom) {
        size_t separated = 0;
        for (const auto& [kept, removed] : joined)
            separated += truths[atom][kept] != truths[atom][removed] ? 1 : 0;
        if (separated > 0)
            ranked.emplace_back(separated, atom);
    }
    std::sort(ranked.rbegin(), ranked.rend());
    // each set of atoms is fitted once, however many starts choose it
    std::set<std::vector<size_t>> fitted;
    bool wide = true;
    for (size_t start = 0; start < ranked.size() && start < max_starts_; ++start) {
        std::optional<std::vector<size_t>> chosen = find_greedy(ranked[start].second, atoms, truths, joined);
        if (!chosen.has_value())
            continue;
        wide = wide && chosen.value().size() > 3;
        std::vector<size_t> key = chosen.value();
        std::sort(key.begin(), key.end());
        if (!fitted.insert(key).second)
            continue;
        fit_chosen(chosen.value(), points, atoms, truths, labels, found);
    }
    if (!wide)
        return;
    for (const std::vector<size_t>& chosen : find_triples(atoms, truths, labels))
        if (fitted.insert(chosen).second)
            fit_chosen(chosen, points, atoms, truths, labels, found);
}

// The atoms a greedy choice takes from `start` on until every joined pair is told apart; nothing when it runs out of
// placeholders or atoms first.
LOGIC_CONDITION_HYPOTHESES_TEMPLATE std::optional<std::vector<size_t>> LOGIC_CONDITION_HYPOTHESES::find_greedy(
    size_t start, const std::vector<const expr*>& atoms, const std::vector<std::vector<bool>>& truths,
    const std::vector<std::pair<size_t, size_t>>& joined) const {
    std::vector<size_t> chosen{start};
    std::vector<std::pair<size_t, size_t>> still;
    for (const auto& [kept, removed] : joined)
        if (truths[start][kept] == truths[start][removed])
            still.emplace_back(kept, removed);
    while (!still.empty() && chosen.size() < placeholders_.size()) {
        size_t best = atoms.size();
        size_t best_separated = 0;
        for (size_t atom = 0; atom < atoms.size(); ++atom) {
            size_t separated = 0;
            for (const auto& [kept, removed] : still)
                separated += truths[atom][kept] != truths[atom][removed] ? 1 : 0;
            if (separated <= best_separated)
                continue;
            best = atom;
            best_separated = separated;
        }
        if (best == atoms.size())
            return std::nullopt;
        chosen.push_back(best);
        std::vector<std::pair<size_t, size_t>> next;
        for (const auto& [kept, removed] : still)
            if (truths[best][kept] == truths[best][removed])
                next.emplace_back(kept, removed);
        still = next;
    }
    if (!still.empty())
        return std::nullopt;
    return chosen;
}

// The triples of atoms that separate every kept value from every removed one (no two points of different labels
// share their three truth values), the cheapest `max_starts` of them.
LOGIC_CONDITION_HYPOTHESES_TEMPLATE std::vector<std::vector<size_t>> LOGIC_CONDITION_HYPOTHESES::find_triples(
    const std::vector<const expr*>& atoms, const std::vector<std::vector<bool>>& truths,
    const std::vector<bool>& labels) {
    std::vector<std::pair<size_t, std::vector<size_t>>> triples;
    size_t checked = 0;
    for (size_t first = 0; first < atoms.size(); ++first)
        for (size_t second = first + 1; second < atoms.size(); ++second)
            for (size_t third = second + 1; third < atoms.size() && checked < max_triples_; ++third) {
                ++checked;
                bool seen[2][8] = {};
                bool separates = true;
                for (size_t point = 0; point < labels.size() && separates; ++point) {
                    const size_t code = (truths[first][point] ? 1 : 0) | (truths[second][point] ? 2 : 0)
                                        | (truths[third][point] ? 4 : 0);
                    seen[labels[point] ? 1 : 0][code] = true;
                    separates = !seen[labels[point] ? 0 : 1][code];
                }
                if (!separates)
                    continue;
                const size_t cost =
                    find_cost_.cost(atoms[first]) + find_cost_.cost(atoms[second]) + find_cost_.cost(atoms[third]);
                triples.emplace_back(cost, std::vector<size_t>{first, second, third});
            }
    std::sort(triples.begin(), triples.end());
    std::vector<std::vector<size_t>> cheapest;
    for (size_t index = 0; index < triples.size() && index < max_starts_; ++index)
        cheapest.push_back(triples[index].second);
    return cheapest;
}

// The labels as a truth table over the chosen atoms, the i-th atom as the i-th placeholder, fitted by the logic domain;
// each exact fit, with the atoms put back, is found.
LOGIC_CONDITION_HYPOTHESES_TEMPLATE void LOGIC_CONDITION_HYPOTHESES::fit_chosen(
    const std::vector<size_t>& chosen, const std::vector<pw_data_point>& points, const std::vector<const expr*>& atoms,
    const std::vector<std::vector<bool>>& truths, const std::vector<bool>& labels,
    std::vector<std::pair<size_t, const expr*>>& found) {
    if (chosen.size() < 2)
        return;
    std::vector<pw_data_point> table;
    std::vector<bool> seen(uint64_t(1) << chosen.size(), false);
    for (size_t point = 0; point < labels.size(); ++point) {
        uint64_t row = 0;
        for (size_t index = 0; index < chosen.size(); ++index)
            row |= truths[chosen[index]][point] ? uint64_t(1) << index : 0;
        if (seen[row])
            continue;
        seen[row] = true;
        // the placeholders beyond the chosen atoms take every value, so they are irrelevant inputs rather than don't
        // cares (a table of mostly don't cares makes the cover's search explode)
        const size_t unused = placeholders_.size() - chosen.size();
        for (uint64_t filler = 0; filler < (uint64_t(1) << unused); ++filler) {
            const uint64_t full = row | (filler << chosen.size());
            std::vector<const expr*> coordinates;
            coordinates.reserve(placeholders_.size());
            for (size_t index = 0; index < placeholders_.size(); ++index)
                coordinates.push_back(make_literal_.make_literal(integer((full >> index) & 1)));
            const expr* input = make_functor_.make_functor(point_symbol_, coordinates);
            const expr* label = make_literal_.make_literal(integer(labels[point] ? 1 : 0));
            table.push_back(pw_data_point{.input = input, .output = label});
        }
    }
    std::vector<const expr*> atoms_chosen;
    for (const size_t index : chosen)
        atoms_chosen.push_back(atoms[index]);
    std::vector<const expr*> formulas;
    propose_formulas_.propose_hypotheses(table, formulas);
    // only a condition that reproduces every label: the logic domain may also propose near fits
    for (const expr* formula : formulas) {
        const expr* condition = substitute(formula, atoms_chosen);
        bool exact = true;
        for (size_t point = 0; point < points.size() && exact; ++point) {
            const std::optional<bool> holds = holds_at_.holds_at(condition, points[point].input);
            exact = holds.has_value() && holds.value() == labels[point];
        }
        if (!exact)
            continue;
        found.emplace_back(find_cost_.cost(condition), condition);
    }
}

// The formula with the i-th placeholder replaced by the i-th chosen atom, and an unused placeholder by 0 (the table
// does not depend on it).
LOGIC_CONDITION_HYPOTHESES_TEMPLATE const expr* LOGIC_CONDITION_HYPOTHESES::substitute(
    const expr* formula, const std::vector<const expr*>& chosen) {
    const expr::functor* f = std::get_if<expr::functor>(&formula->content);
    if (f == nullptr)
        return formula;
    if (f->args.empty()) {
        for (size_t index = 0; index < placeholders_.size(); ++index) {
            if (placeholders_[index] != f->id)
                continue;
            return index < chosen.size() ? chosen[index] : make_literal_.make_literal(integer(0));
        }
        return formula;
    }
    std::vector<const expr*> args;
    args.reserve(f->args.size());
    for (const expr* arg : f->args)
        args.push_back(substitute(arg, chosen));
    return make_functor_.make_functor(f->id, args);
}

#undef LOGIC_CONDITION_HYPOTHESES
#undef LOGIC_CONDITION_HYPOTHESES_TEMPLATE

#endif
