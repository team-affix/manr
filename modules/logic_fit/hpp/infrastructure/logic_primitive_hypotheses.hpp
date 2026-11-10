#ifndef LOGIC_PRIMITIVE_HYPOTHESES_HPP
#define LOGIC_PRIMITIVE_HYPOTHESES_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/logic_symbols.hpp"
#include "value_objects/logic_truth_table.hpp"
#include "value_objects/pw_data_point.hpp"

constexpr size_t k_logic_default_max_primitive_proposals = 8;

// A detector that reads a truth table as one primitive over some of its variables: `and`, `or`, `xor`, `xnor`,
// `nand`, `nor`, `imp` of two and `maj`, `mux` of three, each argument a variable or its negation. Every choice of
// variables and negations is checked against the rows the data gives, so unseen rows are don't cares and a
// primitive is found from part of its table. It proposes the readings with the fewest negations that agree with every
// given row, at most `max_proposals` (a table of few rows agrees with many).
// Nothing for a table the reader refutes, or one no primitive reproduces.
template<typename IFindTable, typename IMakeFunctor>
struct logic_primitive_hypotheses {
    logic_primitive_hypotheses(IFindTable& find_table,
                               IMakeFunctor& make_functor,
                               logic_symbols symbols,
                               std::vector<uint32_t> variables,
                               size_t max_proposals = k_logic_default_max_primitive_proposals);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    struct primitive {
        uint32_t id;
        size_t arity;
        bool symmetric;
    };
    bool compute(uint32_t id, const std::vector<bool>& arguments) const;
    bool reproduces(const logic_truth_table& table, const primitive& kind, const std::vector<size_t>& positions,
                    uint64_t negated) const;
    const expr* render(const primitive& kind, const std::vector<size_t>& positions, uint64_t negated);

    IFindTable& find_table_;
    IMakeFunctor& make_functor_;
    logic_symbols symbols_;
    std::vector<uint32_t> variables_;
    std::vector<primitive> primitives_;
    size_t max_proposals_;
};

#define LOGIC_PRIMITIVE_HYPOTHESES_TEMPLATE template<typename IFindTable, typename IMakeFunctor>
#define LOGIC_PRIMITIVE_HYPOTHESES          logic_primitive_hypotheses<IFindTable, IMakeFunctor>

LOGIC_PRIMITIVE_HYPOTHESES_TEMPLATE LOGIC_PRIMITIVE_HYPOTHESES::logic_primitive_hypotheses(
    IFindTable& find_table,
    IMakeFunctor& make_functor,
    logic_symbols symbols,
    std::vector<uint32_t> variables,
    size_t max_proposals)
    : find_table_(find_table)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , variables_(std::move(variables))
    , primitives_{{.id = symbols.conjunction, .arity = 2, .symmetric = true},
                  {.id = symbols.disjunction, .arity = 2, .symmetric = true},
                  {.id = symbols.exclusion, .arity = 2, .symmetric = true},
                  {.id = symbols.equivalence, .arity = 2, .symmetric = true},
                  {.id = symbols.alternative_denial, .arity = 2, .symmetric = true},
                  {.id = symbols.joint_denial, .arity = 2, .symmetric = true},
                  {.id = symbols.implication, .arity = 2, .symmetric = false},
                  {.id = symbols.majority, .arity = 3, .symmetric = true},
                  {.id = symbols.multiplexer, .arity = 3, .symmetric = false}}
    , max_proposals_(max_proposals) {}

LOGIC_PRIMITIVE_HYPOTHESES_TEMPLATE void LOGIC_PRIMITIVE_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    const std::optional<logic_truth_table> table = find_table_.find_table(points);
    if (!table.has_value() || table.value().rows.empty())
        return;
    const size_t width = variables_.size();
    // every reading that reproduces the table, by its number of negations: fewer is smaller
    std::vector<std::pair<size_t, const expr*>> found;
    for (const primitive& kind : primitives_) {
        if (kind.arity > width)
            continue;
        std::vector<size_t> positions(kind.arity, 0);
        while (true) {
            bool distinct = true;
            for (size_t first = 0; first < kind.arity; ++first)
                for (size_t second = first + 1; second < kind.arity; ++second)
                    distinct = distinct && (kind.symmetric ? positions[first] < positions[second]
                                                           : positions[first] != positions[second]);
            for (uint64_t negated = 0; distinct && negated < (uint64_t(1) << kind.arity); ++negated) {
                if (!reproduces(table.value(), kind, positions, negated))
                    continue;
                const size_t negations = static_cast<size_t>(__builtin_popcountll(negated));
                found.emplace_back(negations, render(kind, positions, negated));
            }
            size_t digit = 0;
            while (digit < kind.arity && ++positions[digit] == width) {
                positions[digit] = 0;
                ++digit;
            }
            if (digit == kind.arity)
                break;
        }
    }
    if (found.empty())
        return;
    size_t fewest = found.front().first;
    for (const auto& [negations, formula] : found)
        fewest = negations < fewest ? negations : fewest;
    size_t proposed = 0;
    for (const auto& [negations, formula] : found) {
        if (negations != fewest || proposed == max_proposals_)
            continue;
        out.push_back(formula);
        ++proposed;
    }
}

LOGIC_PRIMITIVE_HYPOTHESES_TEMPLATE bool LOGIC_PRIMITIVE_HYPOTHESES::compute(
    uint32_t id, const std::vector<bool>& arguments) const {
    if (id == symbols_.multiplexer)
        return arguments[0] ? arguments[1] : arguments[2];
    if (id == symbols_.majority)
        return (arguments[0] ? 1 : 0) + (arguments[1] ? 1 : 0) + (arguments[2] ? 1 : 0) >= 2;
    const bool left = arguments[0];
    const bool right = arguments[1];
    if (id == symbols_.conjunction)
        return left && right;
    if (id == symbols_.disjunction)
        return left || right;
    if (id == symbols_.exclusion)
        return left != right;
    if (id == symbols_.equivalence)
        return left == right;
    if (id == symbols_.alternative_denial)
        return !(left && right);
    if (id == symbols_.joint_denial)
        return !(left || right);
    return !left || right;
}

LOGIC_PRIMITIVE_HYPOTHESES_TEMPLATE bool LOGIC_PRIMITIVE_HYPOTHESES::reproduces(
    const logic_truth_table& table, const primitive& kind, const std::vector<size_t>& positions,
    uint64_t negated) const {
    std::vector<bool> arguments(kind.arity);
    for (const auto& [row, output] : table.rows) {
        for (size_t index = 0; index < kind.arity; ++index) {
            const bool bit = (row >> positions[index]) & 1;
            arguments[index] = ((negated >> index) & 1) ? !bit : bit;
        }
        if (compute(kind.id, arguments) != output)
            return false;
    }
    return true;
}

LOGIC_PRIMITIVE_HYPOTHESES_TEMPLATE const expr* LOGIC_PRIMITIVE_HYPOTHESES::render(
    const primitive& kind, const std::vector<size_t>& positions, uint64_t negated) {
    std::vector<const expr*> arguments;
    arguments.reserve(kind.arity);
    for (size_t index = 0; index < kind.arity; ++index) {
        const expr* variable = make_functor_.make_functor(variables_[positions[index]], {});
        const bool plain = ((negated >> index) & 1) == 0;
        arguments.push_back(plain ? variable : make_functor_.make_functor(symbols_.negation, {variable}));
    }
    return make_functor_.make_functor(kind.id, arguments);
}

#undef LOGIC_PRIMITIVE_HYPOTHESES
#undef LOGIC_PRIMITIVE_HYPOTHESES_TEMPLATE

#endif
