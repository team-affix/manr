#ifndef LOGIC_BIDECOMPOSITION_HYPOTHESES_HPP
#define LOGIC_BIDECOMPOSITION_HYPOTHESES_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/logic_symbols.hpp"
#include "value_objects/logic_truth_table.hpp"
#include "value_objects/pw_data_point.hpp"

constexpr size_t k_logic_default_max_decompositions = 4000;
constexpr size_t k_logic_default_max_shared_inputs = 2;

// A detector that reads a truth table as a composition of any depth, by bi-decomposition: f = c(g(A), h(B)) for a
// connective c (`or`, `and`, `xor`) and two functions of disjoint groups of inputs A and B, each decomposed again in
// turn, down to leaves the leaf domain fits (primitives, parities, covers). Depth is bounded only by the inputs: each
// level splits them.
//
// For a split of the inputs f depends on, each test is one pass over the table, and the unseen rows stay don't cares:
//   or    g(a) is 1 where every known f(a, .) is 1, h(b) likewise; every known 1 must be g(a) or h(b);
//   and   the same with 0s;
//   xor   g(a) xor h(b) = f(a, b) on every known row, solved by propagating parities over the rows (a refutation
//         when a cycle disagrees); a group of unconstrained values is set to 0.
// Every split of the support into two groups is tried (one table pass each) and the smallest composition, or the
// smallest leaf, is kept; results are memoized per table. `max_decompositions` bounds the work on wide tables.
//
// The two parts may also share inputs: f = c(g(A, C), h(B, C)) for a set C of at most `max_shared` inputs both read
// (`xor(maj(a, b, c), and(a, d))` shares a). The tests are the same with each part's projection taken over its own
// inputs and C, which checks them for each assignment of C; each part still loses inputs (A and B are not empty), so
// the depth stays bounded by the inputs.
//
// Two more outer shapes are read on an input v (or two, x and y), each on strictly less of the table:
//   mux   mux(v, g, h): g is f where v holds and h is f where it fails, each made independent of v (Shannon's
//         expansion; it always applies, and is kept when it is smaller);
//   maj   maj(x, y, g) for inputs x and y, plain or negated: where they agree, f must equal them, and where they
//         differ, g is f.
template<typename IProposeLeaves, typename IFindTable, typename IHoldsAt, typename IFindCost, typename IMakeLiteral,
         typename IMakeFunctor>
struct logic_bidecomposition_hypotheses {
    logic_bidecomposition_hypotheses(IProposeLeaves& propose_leaves,
                                     IFindTable& find_table,
                                     IHoldsAt& holds_at,
                                     IFindCost& find_cost,
                                     IMakeLiteral& make_literal,
                                     IMakeFunctor& make_functor,
                                     logic_symbols symbols,
                                     uint32_t point_symbol,
                                     std::vector<uint32_t> variables,
                                     size_t max_decompositions = k_logic_default_max_decompositions,
                                     size_t max_shared = k_logic_default_max_shared_inputs);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    using parts_t = std::pair<logic_truth_table, logic_truth_table>;
    const expr* decompose(const logic_truth_table& table, bool top);
    const expr* find_leaf(const logic_truth_table& table);
    uint64_t find_support(const logic_truth_table& table) const;
    std::optional<parts_t> split_bounded(const logic_truth_table& table, uint64_t left_inputs, uint64_t right_inputs,
                                         bool value) const;
    std::optional<parts_t> split_exclusive(const logic_truth_table& table, uint64_t left_inputs,
                                           uint64_t right_inputs) const;
    logic_truth_table spread(const std::map<uint64_t, bool>& values, uint64_t group) const;
    std::optional<logic_truth_table> find_cofactor(const logic_truth_table& table, size_t position, bool value) const;
    std::optional<logic_truth_table> find_majority_rest(const logic_truth_table& table, size_t first, bool first_plain,
                                                        size_t second, bool second_plain) const;
    const expr* make_literal_input(size_t position, bool plain);
    std::vector<pw_data_point> find_points(const logic_truth_table& table);

    IProposeLeaves& propose_leaves_;
    IFindTable& find_table_;
    IHoldsAt& holds_at_;
    IFindCost& find_cost_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    logic_symbols symbols_;
    uint32_t point_symbol_;
    std::vector<uint32_t> variables_;
    size_t width_;
    size_t max_decompositions_;
    size_t max_shared_;
    size_t decompositions_;
    std::map<logic_truth_table, const expr*> decomposed_;
};

#define LOGIC_BIDECOMPOSITION_HYPOTHESES_TEMPLATE                                                         \
    template<typename IProposeLeaves, typename IFindTable, typename IHoldsAt, typename IFindCost, \
             typename IMakeLiteral, typename IMakeFunctor>
#define LOGIC_BIDECOMPOSITION_HYPOTHESES                                                                    \
    logic_bidecomposition_hypotheses<IProposeLeaves, IFindTable, IHoldsAt, IFindCost, IMakeLiteral, \
                                     IMakeFunctor>

LOGIC_BIDECOMPOSITION_HYPOTHESES_TEMPLATE LOGIC_BIDECOMPOSITION_HYPOTHESES::logic_bidecomposition_hypotheses(
    IProposeLeaves& propose_leaves,
    IFindTable& find_table,
    IHoldsAt& holds_at,
    IFindCost& find_cost,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    logic_symbols symbols,
    uint32_t point_symbol,
    std::vector<uint32_t> variables,
    size_t max_decompositions,
    size_t max_shared)
    : propose_leaves_(propose_leaves)
    , find_table_(find_table)
    , holds_at_(holds_at)
    , find_cost_(find_cost)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , point_symbol_(point_symbol)
    , variables_(std::move(variables))
    , width_(variables_.size())
    , max_decompositions_(max_decompositions)
    , max_shared_(max_shared)
    , decompositions_(0)
    , decomposed_() {}

LOGIC_BIDECOMPOSITION_HYPOTHESES_TEMPLATE void LOGIC_BIDECOMPOSITION_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    const std::optional<logic_truth_table> table = find_table_.find_table(points);
    if (!table.has_value() || table.value().rows.empty())
        return;
    decompositions_ = 0;
    const expr* composition = decompose(table.value(), true);
    if (composition == nullptr)
        return;
    out.push_back(composition);
}

// The smallest formula found for the table: a leaf, or a connective of two decomposed parts.
LOGIC_BIDECOMPOSITION_HYPOTHESES_TEMPLATE const expr* LOGIC_BIDECOMPOSITION_HYPOTHESES::decompose(
    const logic_truth_table& table, bool top) {
    const auto known = decomposed_.find(table);
    if (known != decomposed_.end())
        return known->second;
    ++decompositions_;
    const expr* best = find_leaf(table);
    size_t best_cost = best == nullptr ? SIZE_MAX : find_cost_.cost(best);
    const uint64_t support = find_support(table);
    bool composed = false;
    const uint64_t all = (uint64_t(1) << width_) - 1;
    const bool splittable = __builtin_popcountll(support) >= 2 && decompositions_ < max_decompositions_;
    // every shared set C of at most `max_shared` support inputs, and every split of the rest into A (holding the
    // rest's lowest input) and B, both not empty: g reads A and C, h everything but A
    for (uint64_t subset = support; splittable; subset = (subset - 1) & support) {
        const uint64_t shared = subset;
        const bool last = subset == 0;
        if (static_cast<size_t>(__builtin_popcountll(shared)) > max_shared_) {
            if (last)
                break;
            continue;
        }
        const uint64_t rest = support & ~shared;
        if (__builtin_popcountll(rest) < 2) {
            if (last)
                break;
            continue;
        }
        const uint64_t lowest = rest & (~rest + 1);
        for (uint64_t group = rest; group != 0; group = (group - 1) & rest) {
            const bool proper = (group & lowest) != 0 && group != rest;
            if (!proper)
                continue;
            const uint64_t left_inputs = group | shared;
            const uint64_t right_inputs = all & ~group;
            for (const uint32_t combiner : {symbols_.disjunction, symbols_.conjunction, symbols_.exclusion}) {
                const std::optional<parts_t> parts =
                    combiner == symbols_.exclusion
                        ? split_exclusive(table, left_inputs, right_inputs)
                        : split_bounded(table, left_inputs, right_inputs, combiner == symbols_.disjunction);
                if (!parts.has_value())
                    continue;
                const expr* left = decompose(parts.value().first, false);
                const expr* right = decompose(parts.value().second, false);
                if (left == nullptr || right == nullptr)
                    continue;
                const expr* composition = make_functor_.make_functor(combiner, {left, right});
                composed = true;
                const size_t cost = find_cost_.cost(composition);
                if (cost >= best_cost)
                    continue;
                best = composition;
                best_cost = cost;
            }
        }
        if (last)
            break;
    }
    // mux on one input, maj with two inputs: at the top, and below it where no split by a connective was found (a mux
    // always applies, so trying it everywhere would explore every restriction of the table), while nothing smaller
    // than 5 is found
    const bool selectable = splittable && (top || !composed) && best_cost > 4;
    for (size_t position = 0; selectable && position < width_; ++position) {
        if ((support >> position & 1) == 0)
            continue;
        const std::optional<logic_truth_table> then = find_cofactor(table, position, true);
        const std::optional<logic_truth_table> otherwise = find_cofactor(table, position, false);
        if (then.has_value() && otherwise.has_value()) {
            const expr* left = decompose(then.value(), false);
            const expr* right = decompose(otherwise.value(), false);
            if (left != nullptr && right != nullptr) {
                const expr* select = make_functor_.make_functor(variables_[position], {});
                const expr* composition = make_functor_.make_functor(symbols_.multiplexer, {select, left, right});
                const size_t cost = find_cost_.cost(composition);
                if (cost < best_cost) {
                    best = composition;
                    best_cost = cost;
                }
            }
        }
        for (size_t other = position + 1; other < width_; ++other) {
            if ((support >> other & 1) == 0)
                continue;
            for (const bool first_plain : {true, false})
                for (const bool second_plain : {true, false}) {
                    const std::optional<logic_truth_table> rest =
                        find_majority_rest(table, position, first_plain, other, second_plain);
                    if (!rest.has_value())
                        continue;
                    const expr* third = decompose(rest.value(), false);
                    if (third == nullptr)
                        continue;
                    const expr* composition = make_functor_.make_functor(
                        symbols_.majority,
                        {make_literal_input(position, first_plain), make_literal_input(other, second_plain), third});
                    const size_t cost = find_cost_.cost(composition);
                    if (cost >= best_cost)
                        continue;
                    best = composition;
                    best_cost = cost;
                }
        }
    }
    decomposed_.emplace(table, best);
    return best;
}

// The smallest leaf fit that reproduces every known row; a constant table is its literal.
LOGIC_BIDECOMPOSITION_HYPOTHESES_TEMPLATE const expr* LOGIC_BIDECOMPOSITION_HYPOTHESES::find_leaf(
    const logic_truth_table& table) {
    bool seen_true = false;
    bool seen_false = false;
    for (const auto& [row, output] : table.rows) {
        seen_true = seen_true || output;
        seen_false = seen_false || !output;
    }
    if (!seen_true || !seen_false)
        return make_literal_.make_literal(integer(seen_true ? 1 : 0));
    const std::vector<pw_data_point> points = find_points(table);
    std::vector<const expr*> leaves;
    propose_leaves_.propose_hypotheses(points, leaves);
    const expr* best = nullptr;
    size_t best_cost = SIZE_MAX;
    for (const expr* leaf : leaves) {
        const size_t cost = find_cost_.cost(leaf);
        if (cost >= best_cost)
            continue;
        bool exact = true;
        size_t index = 0;
        for (const auto& [row, output] : table.rows) {
            const std::optional<bool> truth = holds_at_.holds_at(leaf, points[index].input);
            exact = exact && truth.has_value() && truth.value() == output;
            ++index;
            if (!exact)
                break;
        }
        if (!exact)
            continue;
        best = leaf;
        best_cost = cost;
    }
    return best;
}

// The inputs the table depends on: those where two known rows differing in that input alone differ in output.
LOGIC_BIDECOMPOSITION_HYPOTHESES_TEMPLATE uint64_t LOGIC_BIDECOMPOSITION_HYPOTHESES::find_support(
    const logic_truth_table& table) const {
    uint64_t support = 0;
    for (const auto& [row, output] : table.rows)
        for (size_t position = 0; position < width_; ++position) {
            const auto neighbour = table.rows.find(row ^ (uint64_t(1) << position));
            if (neighbour != table.rows.end() && neighbour->second != output)
                support |= uint64_t(1) << position;
        }
    return support;
}

// An `or` (value 1) or `and` (value 0) split: g and h take `value` on an assignment of their inputs where every known
// row with that assignment has it.
LOGIC_BIDECOMPOSITION_HYPOTHESES_TEMPLATE std::optional<typename LOGIC_BIDECOMPOSITION_HYPOTHESES::parts_t>
LOGIC_BIDECOMPOSITION_HYPOTHESES::split_bounded(const logic_truth_table& table, uint64_t left_inputs,
                                                uint64_t right_inputs, bool value) const {
    std::map<uint64_t, bool> left;
    std::map<uint64_t, bool> right;
    for (const auto& [row, output] : table.rows) {
        const bool agrees = output == value;
        auto first = left.emplace(row & left_inputs, agrees).first;
        first->second = first->second && agrees;
        auto second = right.emplace(row & right_inputs, agrees).first;
        second->second = second->second && agrees;
    }
    // every known row of the bounding value must be covered by one side
    for (const auto& [row, output] : table.rows) {
        if (output != value)
            continue;
        const bool covered = left.at(row & left_inputs) || right.at(row & right_inputs);
        if (!covered)
            return std::nullopt;
    }
    // as functions: the side's value where it is bounded, the other value elsewhere
    std::map<uint64_t, bool> g;
    std::map<uint64_t, bool> h;
    for (const auto& [part, bounded] : left)
        g.emplace(part, bounded ? value : !value);
    for (const auto& [part, bounded] : right)
        h.emplace(part, bounded ? value : !value);
    return parts_t{spread(g, left_inputs), spread(h, right_inputs)};
}

// An `xor` split: g xor h = f on every known row, by parity propagation over a graph whose nodes are the two sides'
// input assignments and whose edges are the known rows.
LOGIC_BIDECOMPOSITION_HYPOTHESES_TEMPLATE std::optional<typename LOGIC_BIDECOMPOSITION_HYPOTHESES::parts_t>
LOGIC_BIDECOMPOSITION_HYPOTHESES::split_exclusive(const logic_truth_table& table, uint64_t left_inputs,
                                                  uint64_t right_inputs) const {
    // nodes: a left assignment as itself, a right assignment tagged by the top bit
    const uint64_t tag = uint64_t(1) << 63;
    std::map<uint64_t, std::vector<std::pair<uint64_t, bool>>> edges;
    for (const auto& [row, output] : table.rows) {
        const uint64_t a = row & left_inputs;
        const uint64_t b = (row & right_inputs) | tag;
        edges[a].emplace_back(b, output);
        edges[b].emplace_back(a, output);
    }
    std::map<uint64_t, bool> values;
    for (const auto& [start, unused] : edges) {
        if (values.contains(start))
            continue;
        values.emplace(start, false);
        std::vector<uint64_t> pending{start};
        while (!pending.empty()) {
            const uint64_t node = pending.back();
            pending.pop_back();
            for (const auto& [next, parity] : edges.at(node)) {
                const bool wanted = values.at(node) != parity;
                const auto known = values.find(next);
                if (known != values.end()) {
                    if (known->second != wanted)
                        return std::nullopt;
                    continue;
                }
                values.emplace(next, wanted);
                pending.push_back(next);
            }
        }
    }
    std::map<uint64_t, bool> g;
    std::map<uint64_t, bool> h;
    for (const auto& [node, value] : values)
        ((node & tag) != 0 ? h : g).emplace(node & ~tag, value);
    return parts_t{spread(g, left_inputs), spread(h, right_inputs)};
}

// A function of one group as a full table: every row whose group bits have a value gets it.
LOGIC_BIDECOMPOSITION_HYPOTHESES_TEMPLATE logic_truth_table LOGIC_BIDECOMPOSITION_HYPOTHESES::spread(
    const std::map<uint64_t, bool>& values, uint64_t group) const {
    logic_truth_table table{.width = width_, .rows = {}};
    for (uint64_t row = 0; row < (uint64_t(1) << width_); ++row) {
        const auto value = values.find(row & group);
        if (value == values.end())
            continue;
        table.rows.emplace(row, value->second);
    }
    return table;
}

// f where the input at `position` has `value`, as a table independent of that input; nothing when no known row has it.
LOGIC_BIDECOMPOSITION_HYPOTHESES_TEMPLATE std::optional<logic_truth_table>
LOGIC_BIDECOMPOSITION_HYPOTHESES::find_cofactor(const logic_truth_table& table, size_t position, bool value) const {
    const uint64_t bit = uint64_t(1) << position;
    logic_truth_table cofactor{.width = width_, .rows = {}};
    for (const auto& [row, output] : table.rows) {
        if (((row & bit) != 0) != value)
            continue;
        cofactor.rows.emplace(row & ~bit, output);
        cofactor.rows.emplace(row | bit, output);
    }
    if (cofactor.rows.empty())
        return std::nullopt;
    return cofactor;
}

// For maj(x, y, g): where the two literals agree, f must equal them; g is f on the rows where they differ. Nothing
// when a row where they agree contradicts that, or no row has them agree (g would be all of f).
LOGIC_BIDECOMPOSITION_HYPOTHESES_TEMPLATE std::optional<logic_truth_table>
LOGIC_BIDECOMPOSITION_HYPOTHESES::find_majority_rest(const logic_truth_table& table, size_t first, bool first_plain,
                                                     size_t second, bool second_plain) const {
    logic_truth_table rest{.width = width_, .rows = {}};
    bool agreed = false;
    for (const auto& [row, output] : table.rows) {
        const bool x = ((row >> first & 1) != 0) == first_plain;
        const bool y = ((row >> second & 1) != 0) == second_plain;
        if (x == y) {
            if (output != x)
                return std::nullopt;
            agreed = true;
            continue;
        }
        rest.rows.emplace(row, output);
    }
    if (!agreed || rest.rows.empty())
        return std::nullopt;
    return rest;
}

LOGIC_BIDECOMPOSITION_HYPOTHESES_TEMPLATE const expr* LOGIC_BIDECOMPOSITION_HYPOTHESES::make_literal_input(
    size_t position, bool plain) {
    const expr* variable = make_functor_.make_functor(variables_[position], {});
    return plain ? variable : make_functor_.make_functor(symbols_.negation, {variable});
}

LOGIC_BIDECOMPOSITION_HYPOTHESES_TEMPLATE std::vector<pw_data_point> LOGIC_BIDECOMPOSITION_HYPOTHESES::find_points(
    const logic_truth_table& table) {
    std::vector<pw_data_point> points;
    points.reserve(table.rows.size());
    for (const auto& [row, output] : table.rows) {
        std::vector<const expr*> coordinates;
        coordinates.reserve(width_);
        for (size_t position = 0; position < width_; ++position)
            coordinates.push_back(make_literal_.make_literal(integer((row >> position) & 1)));
        const expr* input = make_functor_.make_functor(point_symbol_, coordinates);
        points.push_back(pw_data_point{.input = input, .output = make_literal_.make_literal(integer(output ? 1 : 0))});
    }
    return points;
}

#undef LOGIC_BIDECOMPOSITION_HYPOTHESES
#undef LOGIC_BIDECOMPOSITION_HYPOTHESES_TEMPLATE

#endif
