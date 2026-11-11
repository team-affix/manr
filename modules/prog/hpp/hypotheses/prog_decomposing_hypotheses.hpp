#ifndef PROG_DECOMPOSING_HYPOTHESES_HPP
#define PROG_DECOMPOSING_HYPOTHESES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/prog_hole.hpp"
#include "value_objects/prog_spec.hpp"
#include "value_objects/prog_symbols.hpp"
#include "value_objects/pw_data_point.hpp"

constexpr size_t k_prog_default_max_depth = 4;
constexpr size_t k_prog_default_top_terms = 250000;
constexpr size_t k_prog_default_sub_terms = 15000;
constexpr size_t k_prog_default_hole_size = 8;
constexpr size_t k_prog_default_split_size = 5;
constexpr size_t k_prog_default_max_solves = 32;
constexpr size_t k_prog_default_decomposed_min_points = 3;
constexpr size_t k_prog_default_decomposed_searches = 3;
constexpr uint32_t k_prog_hole_marker = 1u << 29;
constexpr size_t k_prog_default_unbounded = 1u << 20;

// Hypothesis source: programs found by decomposing the data top-down, so a program of many steps is found as many
// small searches (`docs/prog.md`, composition). A specification (`prog_spec`: environments, outputs, a relation) is
// solved by the enumerator (`find_program`) when small, else split by the step its outputs show:
//
// - a step whose input the outputs determine (its inverse is computed from the data, and the rest is a new
//   specification, solved recursively): `reverse` (the outputs reversed); `sort` and `sort(unique(..))` (sorted
//   outputs: any permutation of them, or the same set, under the relation); `map(f, L)` over an input L as long as
//   each output (f's specification: each element to its image); `filter(p, L)` over a source L holding each output as
//   a subsequence (an input, or the smallest program that does; p's specification: each element to whether it is
//   kept); `append(A, B)` split at an input's length (A's and B's specifications: the parts); a tree map, a tree of the
//   input's shape (`tfold(lams(3, node(#2, f, #0)), leaf, T)`, f's specification: each value to its image);
// - a step whose input they do not determine, as a frame with a hole the enumerator fills, checked through the frame
//   (`fill_hole`): a count `length(filter(lam(□), L))`, a sum `fold(lam(lam(#1 + □)), 0, L)`, a tree sum;
// - a helper: an output that combines one function of two inputs, `let(lam(g), c(app(#0, a), app(#0, b)))` for c
//   an arithmetic operation or `append`, the helper g a hole (or a sum or count of one) filled once for both uses, so
//   it is searched at its own size, not twice its size.
//
// The enumerator searches each specification first (`top_terms` terms at the top, `sub_terms` below), and a
// specification it solves in at most `split_size` nodes is not split (no split is smaller). Each split recurses, up
// to `max_depth` steps, so the depth of a program is not bounded by the enumerator's size; at most `max_solves`
// searches are made per data set (a hole's content has at most `hole_size` nodes). Helpers are tried first at the
// top, a reverse last (it applies to any list). Every result is checked on every sample, and the smallest wins.
// Memoized per data set, at most `max_searches` sets.
template<typename IFindProgram, typename IFillHole, typename IEvaluateWith, typename IMatchRelation,
         typename IFindLiteralValue, typename IMakeLiteral, typename IMakeIndex, typename IMakeFunctor,
         typename IMakeVar>
struct prog_decomposing_hypotheses {
    prog_decomposing_hypotheses(IFindProgram& find_program,
                                IFillHole& fill_hole,
                                IEvaluateWith& evaluate_with,
                                IMatchRelation& match_relation,
                                IFindLiteralValue& find_literal_value,
                                IMakeLiteral& make_literal,
                                IMakeIndex& make_index,
                                IMakeFunctor& make_functor,
                                IMakeVar& make_var,
                                prog_symbols symbols,
                                size_t max_depth = k_prog_default_max_depth,
                                size_t top_terms = k_prog_default_top_terms,
                                size_t sub_terms = k_prog_default_sub_terms,
                                size_t hole_size = k_prog_default_hole_size,
                                size_t max_solves = k_prog_default_max_solves,
                                size_t min_points = k_prog_default_decomposed_min_points,
                                size_t max_searches = k_prog_default_decomposed_searches);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    using values_t = std::vector<const expr*>;

    std::optional<const expr*> solve(const prog_spec& spec, size_t depth, size_t bound);
    static size_t find_room(const std::optional<const expr*>& best, size_t overhead);
    void split_list(const prog_spec& spec, size_t depth, std::optional<const expr*>& best);
    void split_tree(const prog_spec& spec, size_t depth, std::optional<const expr*>& best);
    void fill_aggregates(const prog_spec& spec, std::optional<const expr*>& best);
    void fill_unordered(const prog_spec& spec, std::optional<const expr*>& best);
    void fill_helpers(const prog_spec& spec, std::optional<const expr*>& best);
    void filter_from(const prog_spec& spec, size_t depth, const expr* source, std::optional<const expr*>& best);
    void filter_before_map(const prog_spec& spec, size_t depth, const expr* body, size_t position,
                           std::optional<const expr*>& best);
    void keep(const expr* candidate, const prog_spec& spec, std::optional<const expr*>& best);
    void try_fill(const prog_hole& hole, const prog_spec& spec, std::optional<const expr*>& best);
    std::optional<values_t> find_list(const expr* list) const;
    std::optional<std::vector<values_t>> find_column_lists(const prog_spec& spec, size_t position) const;
    bool is_tree(const expr* value) const;
    bool same_shape(const expr* left, const expr* right) const;
    void collect_nodes(const expr* in, const expr* out, const values_t& env, prog_spec& body) const;
    void collect_values(const expr* tree, values_t& values) const;
    const expr* make_list(const values_t& elements);
    const expr* make_placeholder(size_t index);
    const expr* make_function(size_t parameters, const expr* body);
    const expr* make_input(const prog_spec& spec, size_t position);
    static size_t count_nodes(const expr* e);
    static bool holds_hole(const expr* e);

    IFindProgram& find_program_;
    IFillHole& fill_hole_;
    IEvaluateWith& evaluate_with_;
    IMatchRelation& match_relation_;
    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IMakeIndex& make_index_;
    IMakeFunctor& make_functor_;
    IMakeVar& make_var_;
    prog_symbols symbols_;
    size_t max_depth_;
    size_t top_terms_;
    size_t sub_terms_;
    size_t hole_size_;
    size_t max_solves_;
    size_t min_points_;
    size_t max_searches_;
    size_t solves_;
    std::map<prog_spec, std::pair<std::optional<const expr*>, size_t>> solved_;
    std::map<std::vector<const expr*>, std::optional<const expr*>> memo_;
};

#define PROG_DECOMPOSING_HYPOTHESES_TEMPLATE                                                                    \
    template<typename IFindProgram, typename IFillHole, typename IEvaluateWith, typename IMatchRelation,     \
             typename IFindLiteralValue, typename IMakeLiteral, typename IMakeIndex, typename IMakeFunctor,   \
             typename IMakeVar>
#define PROG_DECOMPOSING_HYPOTHESES                                                                             \
    prog_decomposing_hypotheses<IFindProgram, IFillHole, IEvaluateWith, IMatchRelation, IFindLiteralValue,    \
                                IMakeLiteral, IMakeIndex, IMakeFunctor, IMakeVar>

PROG_DECOMPOSING_HYPOTHESES_TEMPLATE PROG_DECOMPOSING_HYPOTHESES::prog_decomposing_hypotheses(
    IFindProgram& find_program,
    IFillHole& fill_hole,
    IEvaluateWith& evaluate_with,
    IMatchRelation& match_relation,
    IFindLiteralValue& find_literal_value,
    IMakeLiteral& make_literal,
    IMakeIndex& make_index,
    IMakeFunctor& make_functor,
    IMakeVar& make_var,
    prog_symbols symbols,
    size_t max_depth,
    size_t top_terms,
    size_t sub_terms,
    size_t hole_size,
    size_t max_solves,
    size_t min_points,
    size_t max_searches)
    : find_program_(find_program)
    , fill_hole_(fill_hole)
    , evaluate_with_(evaluate_with)
    , match_relation_(match_relation)
    , find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , make_index_(make_index)
    , make_functor_(make_functor)
    , make_var_(make_var)
    , symbols_(symbols)
    , max_depth_(max_depth)
    , top_terms_(top_terms)
    , sub_terms_(sub_terms)
    , hole_size_(hole_size)
    , max_solves_(max_solves)
    , min_points_(min_points)
    , max_searches_(max_searches)
    , solves_(0)
    , solved_()
    , memo_() {}

PROG_DECOMPOSING_HYPOTHESES_TEMPLATE void PROG_DECOMPOSING_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    std::vector<const expr*> key;
    for (const pw_data_point& point : points) {
        key.push_back(point.input);
        key.push_back(point.output);
    }
    auto known = memo_.find(key);
    if (known == memo_.end() && memo_.size() >= max_searches_)
        return;
    if (known == memo_.end()) {
        // the samples without holes, their inputs as environments (a tuple is several inputs)
        prog_spec spec{.envs = {}, .outputs = {}, .relation = k_prog_relation_equal};
        for (const pw_data_point& point : points) {
            if (holds_hole(point.input) || holds_hole(point.output))
                continue;
            const expr::functor* f = std::get_if<expr::functor>(&point.input->content);
            const bool tuple = f != nullptr && f->id == symbols_.point;
            spec.envs.push_back(tuple ? f->args : values_t{point.input});
            spec.outputs.push_back(point.output);
        }
        solves_ = 0;
        solved_.clear();
        const std::optional<const expr*> found =
            spec.envs.size() < min_points_ ? std::nullopt : solve(spec, 0, k_prog_default_unbounded);
        known = memo_.emplace(key, found).first;
    }
    if (known->second.has_value())
        out.push_back(known->second.value());
}

// the smallest program found for the specification of at most `bound` nodes: the enumerator's, then every split, each
// searched only for programs smaller than the best so far
PROG_DECOMPOSING_HYPOTHESES_TEMPLATE std::optional<const expr*> PROG_DECOMPOSING_HYPOTHESES::solve(
    const prog_spec& spec, size_t depth, size_t bound) {
    if (bound == 0)
        return std::nullopt;
    const auto known = solved_.find(spec);
    // known: a program (kept when within the bound), or none within a bound at least this one
    const bool settled = known != solved_.end()
                         && (known->second.first.has_value() || known->second.second >= bound);
    if (settled && known->second.first.has_value() && count_nodes(known->second.first.value()) > bound)
        return std::nullopt;
    if (settled)
        return known->second.first;
    // a specification being solved is not solved again inside itself (a reverse of a reverse)
    solved_[spec] = {std::nullopt, k_prog_default_unbounded};
    std::optional<const expr*> best;
    if (solves_ < max_solves_) {
        ++solves_;
        const std::optional<const expr*> found =
            find_program_.find_program(spec, bound, depth == 0 ? top_terms_ : sub_terms_);
        if (found.has_value() && count_nodes(found.value()) <= bound)
            keep(found.value(), spec, best);
    }
    const bool small = best.has_value() && count_nodes(best.value()) <= k_prog_default_split_size;
    const bool deeper = depth < max_depth_ && solves_ < max_solves_ && !small;
    if (deeper && depth == 0)
        fill_helpers(spec, best);
    if (deeper) {
        split_list(spec, depth, best);
        split_tree(spec, depth, best);
        fill_aggregates(spec, best);
        fill_unordered(spec, best);
    }
    const bool within = best.has_value() && count_nodes(best.value()) <= bound;
    solved_[spec] = {within ? best : std::nullopt, bound};
    return within ? best : std::nullopt;
}

// the nodes a part may have so that, with `overhead` nodes around it, the whole is smaller than the best so far
PROG_DECOMPOSING_HYPOTHESES_TEMPLATE size_t PROG_DECOMPOSING_HYPOTHESES::find_room(
    const std::optional<const expr*>& best, size_t overhead) {
    const size_t limit = best.has_value() ? count_nodes(best.value()) - 1 : k_prog_default_unbounded;
    return limit > overhead ? limit - overhead : 0;
}

// the splits of a specification whose outputs are lists
PROG_DECOMPOSING_HYPOTHESES_TEMPLATE void PROG_DECOMPOSING_HYPOTHESES::split_list(const prog_spec& spec, size_t depth,
                                                                                  std::optional<const expr*>& best) {
    if (spec.relation != k_prog_relation_equal)
        return;
    std::vector<values_t> outputs;
    bool long_enough = false;
    bool sorted = true;
    bool strict = true;
    for (const expr* output : spec.outputs) {
        const std::optional<values_t> elements = find_list(output);
        if (!elements.has_value())
            return;
        outputs.push_back(elements.value());
        long_enough = long_enough || elements.value().size() >= 2;
        for (size_t i = 1; i < elements.value().size(); ++i) {
            const std::optional<integer> before = find_literal_value_.find_value(elements.value()[i - 1]);
            const std::optional<integer> after = find_literal_value_.find_value(elements.value()[i]);
            const bool ordered = before.has_value() && after.has_value() && before.value() <= after.value();
            sorted = sorted && ordered;
            strict = strict && ordered && before.value() < after.value();
        }
    }
    // sort(P), sort(unique(P)): P's outputs any permutation of the outputs, or any list of their elements
    if (long_enough && sorted) {
        const std::optional<const expr*> inner =
            solve(prog_spec{.envs = spec.envs, .outputs = spec.outputs, .relation = k_prog_relation_permutation},
                  depth + 1, find_room(best, 1));
        if (inner.has_value())
            keep(make_functor_.make_functor(symbols_.sort, {inner.value()}), spec, best);
    }
    if (long_enough && strict) {
        const std::optional<const expr*> inner =
            solve(prog_spec{.envs = spec.envs, .outputs = spec.outputs, .relation = k_prog_relation_set}, depth + 1,
                  find_room(best, 2));
        if (inner.has_value())
            keep(make_functor_.make_functor(
                     symbols_.sort, {make_functor_.make_functor(symbols_.unique, {inner.value()})}),
                 spec, best);
    }
    const size_t positions = spec.envs.empty() ? 0 : spec.envs[0].size();
    bool filtered = false;
    for (size_t position = 0; position < positions; ++position) {
        const std::optional<std::vector<values_t>> inputs = find_column_lists(spec, position);
        if (!inputs.has_value())
            continue;
        // map(f, L): f's outputs are the outputs' elements, at the environment and L's element
        bool aligned = true;
        bool subsequence = true;
        bool fits = true;
        for (size_t j = 0; j < spec.envs.size(); ++j) {
            aligned = aligned && inputs.value()[j].size() == outputs[j].size();
            subsequence = subsequence && match_relation_.matches(k_prog_relation_supersequence,
                                                                 spec.envs[j][position], spec.outputs[j]);
            fits = fits && inputs.value()[j].size() <= outputs[j].size();
        }
        if (aligned) {
            prog_spec body{.envs = {}, .outputs = {}, .relation = k_prog_relation_equal};
            for (size_t j = 0; j < spec.envs.size(); ++j) {
                for (size_t i = 0; i < outputs[j].size(); ++i) {
                    values_t env = spec.envs[j];
                    env.push_back(inputs.value()[j][i]);
                    body.envs.push_back(env);
                    body.outputs.push_back(outputs[j][i]);
                }
            }
            const std::optional<const expr*> inner = body.envs.empty() ? std::nullopt
                                                                          : solve(body, depth + 1, find_room(best, 3));
            if (inner.has_value())
                keep(make_functor_.make_functor(symbols_.map,
                                                {make_function(1, inner.value()), make_input(spec, position)}),
                     spec, best);
        }
        if (subsequence) {
            filtered = true;
            filter_from(spec, depth, make_input(spec, position), best);
        }
        // append(A, B): the outputs split where an input's length says
        if (!fits)
            continue;
        for (const bool prefix : {true, false}) {
            prog_spec first{.envs = spec.envs, .outputs = {}, .relation = k_prog_relation_equal};
            prog_spec second{.envs = spec.envs, .outputs = {}, .relation = k_prog_relation_equal};
            bool first_empty = true;
            bool second_empty = true;
            for (size_t j = 0; j < spec.envs.size(); ++j) {
                const size_t length = inputs.value()[j].size();
                const size_t cut = prefix ? length : outputs[j].size() - length;
                first.outputs.push_back(make_list(values_t(outputs[j].begin(), outputs[j].begin() + cut)));
                second.outputs.push_back(make_list(values_t(outputs[j].begin() + cut, outputs[j].end())));
                first_empty = first_empty && cut == 0;
                second_empty = second_empty && cut == outputs[j].size();
            }
            if (first_empty || second_empty)
                continue;
            const std::optional<const expr*> left = solve(first, depth + 1, find_room(best, 2));
            if (!left.has_value())
                continue;
            const std::optional<const expr*> right =
                solve(second, depth + 1, find_room(best, 1 + count_nodes(left.value())));
            if (right.has_value())
                keep(make_functor_.make_functor(symbols_.append, {left.value(), right.value()}), spec, best);
        }
    }
    // filter(p, S) over a computed source holding every output as a subsequence: a map of an input (its function a
    // hole; then also the filter before the map), else the smallest program that does
    std::optional<const expr*> element;
    for (const values_t& elements : outputs)
        if (!element.has_value() && !elements.empty())
            element = elements[0];
    const expr* hole = make_var_.make_var(k_prog_hole_marker);
    const expr* exemplar = !element.has_value() || find_literal_value_.find_value(element.value()).has_value()
                               ? make_literal_.make_literal(integer(0))
                               : make_functor_.make_functor(symbols_.nil, {});
    const prog_spec containing{.envs = spec.envs, .outputs = spec.outputs,
                               .relation = k_prog_relation_supersequence};
    for (size_t position = 0; position < positions && !filtered && element.has_value(); ++position) {
        const std::optional<std::vector<values_t>> inputs = find_column_lists(spec, position);
        if (!inputs.has_value() || solves_ >= max_solves_)
            continue;
        prog_hole mapped{.frames = {make_functor_.make_functor(
                             symbols_.map, {make_function(1, hole), make_input(spec, position)})},
                         .marker = hole, .scope_envs = {}, .exemplar = exemplar, .spec = containing,
                         .max_size = hole_size_, .max_terms = sub_terms_};
        for (size_t j = 0; j < spec.envs.size(); ++j) {
            for (const expr* value : inputs.value()[j]) {
                values_t env = spec.envs[j];
                env.push_back(value);
                mapped.scope_envs.push_back(env);
            }
        }
        if (mapped.scope_envs.empty())
            continue;
        ++solves_;
        const std::optional<const expr*> source = fill_hole_.fill_hole(mapped);
        if (!source.has_value())
            continue;
        filtered = true;
        filter_from(spec, depth, source.value(), best);
        const expr* function = std::get<expr::functor>(source.value()->content).args[0];
        filter_before_map(spec, depth, std::get<expr::functor>(function->content).args[0], position, best);
    }
    if (!filtered && solves_ < max_solves_) {
        ++solves_;
        const std::optional<const expr*> source =
            find_program_.find_program(containing, find_room(best, 3), sub_terms_);
        if (source.has_value())
            filter_from(spec, depth, source.value(), best);
    }
    // reverse(P): P's outputs are the outputs reversed
    if (long_enough) {
        prog_spec reversed{.envs = spec.envs, .outputs = {}, .relation = k_prog_relation_equal};
        for (const values_t& elements : outputs)
            reversed.outputs.push_back(make_list(values_t(elements.rbegin(), elements.rend())));
        const std::optional<const expr*> inner = solve(reversed, depth + 1, find_room(best, 1));
        if (inner.has_value())
            keep(make_functor_.make_functor(symbols_.reverse, {inner.value()}), spec, best);
    }
}

// filter(p, S): p's outputs say which of S's elements each output keeps (S's values hold the outputs in order)
PROG_DECOMPOSING_HYPOTHESES_TEMPLATE void PROG_DECOMPOSING_HYPOTHESES::filter_from(const prog_spec& spec, size_t depth,
                                                                                   const expr* source,
                                                                                   std::optional<const expr*>& best) {
    prog_spec predicate{.envs = {}, .outputs = {}, .relation = k_prog_relation_truth};
    bool dropped = false;
    for (size_t j = 0; j < spec.envs.size(); ++j) {
        const std::optional<const expr*> value = evaluate_with_.evaluate_with(source, spec.envs[j]);
        const std::optional<values_t> elements = value.has_value() ? find_list(value.value()) : std::nullopt;
        const std::optional<values_t> wanted = find_list(spec.outputs[j]);
        if (!elements.has_value() || !wanted.has_value())
            return;
        size_t matched = 0;
        for (const expr* element : elements.value()) {
            const bool kept = matched < wanted.value().size() && wanted.value()[matched] == element;
            matched += kept ? 1 : 0;
            dropped = dropped || !kept;
            values_t env = spec.envs[j];
            env.push_back(element);
            predicate.envs.push_back(env);
            predicate.outputs.push_back(make_literal_.make_literal(integer(kept ? 1 : 0)));
        }
        if (matched != wanted.value().size())
            return;
    }
    if (!dropped || predicate.envs.empty())
        return;
    const std::optional<const expr*> inner =
        solve(predicate, depth + 1, find_room(best, 2 + count_nodes(source)));
    if (inner.has_value())
        keep(make_functor_.make_functor(symbols_.filter, {make_function(1, inner.value()), source}), spec, best);
}

// map(f, filter(p, L)): the outputs are f's images of some of L's elements, in order; p's outputs say which (the
// first match of each output among the images), at the environment and the element itself
PROG_DECOMPOSING_HYPOTHESES_TEMPLATE void PROG_DECOMPOSING_HYPOTHESES::filter_before_map(
    const prog_spec& spec, size_t depth, const expr* body, size_t position, std::optional<const expr*>& best) {
    // two readings of which elements were kept when images repeat: each output's first match, and its last
    for (const bool forward : {true, false}) {
        prog_spec predicate{.envs = {}, .outputs = {}, .relation = k_prog_relation_truth};
        bool dropped = false;
        bool complete = true;
        for (size_t j = 0; j < spec.envs.size() && complete; ++j) {
            const std::optional<values_t> elements = find_list(spec.envs[j][position]);
            const std::optional<values_t> wanted = find_list(spec.outputs[j]);
            complete = elements.has_value() && wanted.has_value();
            const size_t count = complete ? elements.value().size() : 0;
            std::vector<bool> kept(count, false);
            size_t matched = 0;
            for (size_t step = 0; step < count && complete; ++step) {
                const size_t at = forward ? step : count - 1 - step;
                values_t env = spec.envs[j];
                env.push_back(elements.value()[at]);
                const std::optional<const expr*> image = evaluate_with_.evaluate_with(body, env);
                complete = image.has_value();
                const size_t next = forward ? matched : wanted.value().size() - 1 - matched;
                kept[at] = complete && matched < wanted.value().size() && wanted.value()[next] == image.value();
                matched += kept[at] ? 1 : 0;
                dropped = dropped || !kept[at];
            }
            complete = complete && matched == wanted.value().size();
            for (size_t at = 0; at < count && complete; ++at) {
                values_t env = spec.envs[j];
                env.push_back(elements.value()[at]);
                predicate.envs.push_back(env);
                predicate.outputs.push_back(make_literal_.make_literal(integer(kept[at] ? 1 : 0)));
            }
        }
        if (!complete || !dropped || predicate.envs.empty())
            continue;
        const std::optional<const expr*> inner = solve(predicate, depth + 1, find_room(best, 5 + count_nodes(body)));
        if (!inner.has_value())
            continue;
        const expr* filtered = make_functor_.make_functor(
            symbols_.filter, {make_function(1, inner.value()), make_input(spec, position)});
        keep(make_functor_.make_functor(symbols_.map, {make_function(1, body), filtered}), spec, best);
    }
}

// a tree map: an input tree of each output's shape; f's outputs are the output's values, at the environment and the
// tree fold's left result, value and right result
PROG_DECOMPOSING_HYPOTHESES_TEMPLATE void PROG_DECOMPOSING_HYPOTHESES::split_tree(const prog_spec& spec, size_t depth,
                                                                                  std::optional<const expr*>& best) {
    if (spec.relation != k_prog_relation_equal || spec.envs.empty())
        return;
    for (const expr* output : spec.outputs)
        if (!is_tree(output))
            return;
    for (size_t position = 0; position < spec.envs[0].size(); ++position) {
        bool shaped = true;
        for (size_t j = 0; j < spec.envs.size() && shaped; ++j)
            shaped = same_shape(spec.envs[j][position], spec.outputs[j]);
        if (!shaped)
            continue;
        prog_spec body{.envs = {}, .outputs = {}, .relation = k_prog_relation_equal};
        for (size_t j = 0; j < spec.envs.size(); ++j)
            collect_nodes(spec.envs[j][position], spec.outputs[j], spec.envs[j], body);
        if (body.envs.empty())
            continue;
        const std::optional<const expr*> inner = solve(body, depth + 1, find_room(best, 8));
        if (!inner.has_value())
            continue;
        const expr* step = make_functor_.make_functor(
            symbols_.node, {make_index_.make_index(2), inner.value(), make_index_.make_index(0)});
        keep(make_functor_.make_functor(symbols_.tree_fold,
                                        {make_function(3, step), make_functor_.make_functor(symbols_.leaf, {}),
                                         make_input(spec, position)}),
             spec, best);
    }
}

// a count, a sum or a tree sum of a function of the elements: the function a hole, checked through the frame
PROG_DECOMPOSING_HYPOTHESES_TEMPLATE void PROG_DECOMPOSING_HYPOTHESES::fill_aggregates(
    const prog_spec& spec, std::optional<const expr*>& best) {
    if (spec.relation != k_prog_relation_equal || spec.envs.empty())
        return;
    for (const expr* output : spec.outputs)
        if (!find_literal_value_.find_value(output).has_value())
            return;
    const expr* hole = make_var_.make_var(k_prog_hole_marker);
    const expr* zero = make_literal_.make_literal(integer(0));
    for (size_t position = 0; position < spec.envs[0].size(); ++position) {
        const expr* input = make_input(spec, position);
        const std::optional<std::vector<values_t>> inputs = find_column_lists(spec, position);
        if (inputs.has_value()) {
            prog_hole count{.frames = {make_functor_.make_functor(
                                symbols_.length,
                                {make_functor_.make_functor(symbols_.filter, {make_function(1, hole), input})})},
                            .marker = hole, .scope_envs = {}, .exemplar = zero, .spec = spec,
                            .max_size = hole_size_, .max_terms = sub_terms_};
            prog_hole sum{.frames = {make_functor_.make_functor(
                              symbols_.fold,
                              {make_function(2, make_functor_.make_functor(symbols_.add,
                                                                           {make_index_.make_index(1), hole})),
                               zero, input})},
                          .marker = hole, .scope_envs = {}, .exemplar = zero, .spec = spec, .max_size = hole_size_,
                          .max_terms = sub_terms_};
            for (size_t j = 0; j < spec.envs.size(); ++j) {
                for (const expr* element : inputs.value()[j]) {
                    values_t env = spec.envs[j];
                    env.push_back(element);
                    count.scope_envs.push_back(env);
                    values_t accumulated = spec.envs[j];
                    accumulated.push_back(make_placeholder(sum.scope_envs.size()));
                    accumulated.push_back(element);
                    sum.scope_envs.push_back(accumulated);
                }
            }
            try_fill(count, spec, best);
            try_fill(sum, spec, best);
            continue;
        }
        bool trees = true;
        for (const std::vector<const expr*>& env : spec.envs)
            trees = trees && is_tree(env[position]);
        if (!trees)
            continue;
        const expr* step = make_functor_.make_functor(
            symbols_.add, {make_functor_.make_functor(symbols_.add, {make_index_.make_index(2), hole}),
                           make_index_.make_index(0)});
        prog_hole sum{.frames = {make_functor_.make_functor(symbols_.tree_fold,
                                                            {make_function(3, step), zero, input})},
                      .marker = hole, .scope_envs = {}, .exemplar = zero, .spec = spec, .max_size = hole_size_,
                      .max_terms = sub_terms_};
        for (size_t j = 0; j < spec.envs.size(); ++j) {
            values_t values;
            collect_values(spec.envs[j][position], values);
            for (const expr* value : values) {
                values_t env = spec.envs[j];
                env.push_back(make_placeholder(sum.scope_envs.size()));
                env.push_back(value);
                env.push_back(make_placeholder(sum.scope_envs.size() + 5));
                sum.scope_envs.push_back(env);
            }
        }
        try_fill(sum, spec, best);
    }
}

// under a `sort` (a permutation or set of the outputs), a map or filter whose elements' order is lost: the function a
// hole over the input's elements, checked through the frame
PROG_DECOMPOSING_HYPOTHESES_TEMPLATE void PROG_DECOMPOSING_HYPOTHESES::fill_unordered(
    const prog_spec& spec, std::optional<const expr*>& best) {
    const bool unordered = spec.relation == k_prog_relation_permutation || spec.relation == k_prog_relation_set;
    if (!unordered || spec.envs.empty())
        return;
    std::optional<const expr*> element;
    for (const expr* output : spec.outputs) {
        const std::optional<values_t> elements = find_list(output);
        if (!elements.has_value())
            return;
        if (!element.has_value() && !elements.value().empty())
            element = elements.value()[0];
    }
    if (!element.has_value())
        return;
    const expr* hole = make_var_.make_var(k_prog_hole_marker);
    const expr* zero = make_literal_.make_literal(integer(0));
    const expr* exemplar =
        find_literal_value_.find_value(element.value()).has_value() ? zero
                                                                     : make_functor_.make_functor(symbols_.nil, {});
    const expr* nil = make_functor_.make_functor(symbols_.nil, {});
    for (size_t position = 0; position < spec.envs[0].size(); ++position) {
        const std::optional<std::vector<values_t>> inputs = find_column_lists(spec, position);
        const expr* input = make_input(spec, position);
        bool trees = true;
        for (const values_t& env : spec.envs)
            trees = trees && is_tree(env[position]);
        // a tree's values listed (in order, the order lost anyway): a function of each value a hole
        if (trees) {
            const expr* step = make_functor_.make_functor(
                symbols_.append, {make_index_.make_index(2),
                                  make_functor_.make_functor(symbols_.cons, {hole, make_index_.make_index(0)})});
            prog_hole listed{.frames = {make_functor_.make_functor(symbols_.tree_fold,
                                                                   {make_function(3, step), nil, input})},
                             .marker = hole, .scope_envs = {}, .exemplar = exemplar, .spec = spec,
                             .max_size = hole_size_, .max_terms = sub_terms_};
            for (size_t j = 0; j < spec.envs.size(); ++j) {
                values_t values;
                collect_values(spec.envs[j][position], values);
                for (const expr* value : values) {
                    values_t env = spec.envs[j];
                    env.push_back(nil);
                    env.push_back(value);
                    env.push_back(nil);
                    listed.scope_envs.push_back(env);
                }
            }
            try_fill(listed, spec, best);
        }
        if (!inputs.has_value())
            continue;
        prog_hole mapped{.frames = {make_functor_.make_functor(symbols_.map, {make_function(1, hole), input})},
                         .marker = hole, .scope_envs = {}, .exemplar = exemplar, .spec = spec,
                         .max_size = hole_size_, .max_terms = sub_terms_};
        prog_hole filtered{.frames = {make_functor_.make_functor(symbols_.filter, {make_function(1, hole), input})},
                           .marker = hole, .scope_envs = {}, .exemplar = zero, .spec = spec,
                           .max_size = hole_size_, .max_terms = sub_terms_};
        for (size_t j = 0; j < spec.envs.size(); ++j) {
            for (const expr* value : inputs.value()[j]) {
                values_t env = spec.envs[j];
                env.push_back(value);
                mapped.scope_envs.push_back(env);
                filtered.scope_envs.push_back(env);
            }
        }
        try_fill(mapped, spec, best);
        try_fill(filtered, spec, best);
    }
}

// a helper used twice: c(g(a), g(b)) for two inputs a, b of one kind, g found once, as a hole (or a count or sum of
// one) over the inputs' values, and written `let(lam(g), c(app(#0, a), app(#0, b)))`
PROG_DECOMPOSING_HYPOTHESES_TEMPLATE void PROG_DECOMPOSING_HYPOTHESES::fill_helpers(const prog_spec& spec,
                                                                                    std::optional<const expr*>& best) {
    if (spec.relation != k_prog_relation_equal || spec.envs.empty() || spec.envs[0].size() < 2)
        return;
    bool numbers = true;
    bool lists = true;
    for (const expr* output : spec.outputs) {
        numbers = numbers && find_literal_value_.find_value(output).has_value();
        lists = lists && find_list(output).has_value();
    }
    if (!numbers && !lists)
        return;
    const expr* hole = make_var_.make_var(k_prog_hole_marker);
    const expr* zero = make_literal_.make_literal(integer(0));
    const expr* nil = make_functor_.make_functor(symbols_.nil, {});
    const size_t positions = spec.envs[0].size();
    // the uses: ordered pairs of inputs of one kind (lists, or trees)
    std::vector<std::pair<size_t, size_t>> uses;
    std::vector<const expr*> arguments;
    for (size_t first = 0; first < positions; ++first) {
        for (size_t second = 0; second < positions; ++second) {
            const bool both_lists = find_column_lists(spec, first).has_value()
                                    && find_column_lists(spec, second).has_value();
            bool both_trees = true;
            for (const values_t& env : spec.envs)
                both_trees = both_trees && is_tree(env[first]) && is_tree(env[second]);
            if (first != second && (both_lists || both_trees))
                uses.emplace_back(first, second);
        }
    }
    if (uses.empty())
        return;
    for (const values_t& env : spec.envs)
        for (const auto& [first, second] : uses)
            for (const size_t position : {first, second})
                if (std::find(arguments.begin(), arguments.end(), env[position]) == arguments.end())
                    arguments.push_back(env[position]);
    const bool over_lists = find_list(arguments[0]).has_value();
    // the helper's forms: a hole over the argument; a sum or a count of a hole over its elements
    const expr* input = make_index_.make_index(0);
    std::vector<std::pair<const expr*, std::vector<values_t>>> forms;
    std::vector<values_t> plain;
    for (const expr* argument : arguments)
        plain.push_back(values_t{argument});
    forms.emplace_back(hole, plain);
    if (numbers && over_lists) {
        std::vector<values_t> summed;
        std::vector<values_t> counted;
        for (const expr* argument : arguments) {
            const values_t elements = find_list(argument).value();
            for (const expr* element : elements) {
                summed.push_back(values_t{argument, make_placeholder(summed.size()), element});
                counted.push_back(values_t{argument, element});
            }
        }
        forms.emplace_back(make_functor_.make_functor(
                               symbols_.fold, {make_function(2, make_functor_.make_functor(
                                                                    symbols_.add, {make_index_.make_index(1), hole})),
                                               zero, input}),
                           summed);
        forms.emplace_back(make_functor_.make_functor(
                               symbols_.length,
                               {make_functor_.make_functor(symbols_.filter, {make_function(1, hole), input})}),
                           counted);
    }
    if (numbers && !over_lists) {
        std::vector<values_t> summed;
        for (const expr* argument : arguments) {
            values_t values;
            collect_values(argument, values);
            for (const expr* value : values)
                summed.push_back(values_t{argument, make_placeholder(summed.size()), value,
                                          make_placeholder(summed.size() + 5)});
        }
        const expr* step = make_functor_.make_functor(
            symbols_.add, {make_functor_.make_functor(symbols_.add, {make_index_.make_index(2), hole}),
                           make_index_.make_index(0)});
        forms.emplace_back(make_functor_.make_functor(symbols_.tree_fold, {make_function(3, step), zero, input}),
                           summed);
    }
    std::vector<uint32_t> combiners;
    if (numbers)
        combiners = {symbols_.add, symbols_.sub, symbols_.mul, symbols_.max, symbols_.min};
    if (lists)
        combiners = {symbols_.append};
    for (const auto& [form, scope_envs] : forms) {
        if (scope_envs.empty())
            continue;
        // a helper is worth a larger search: it is the whole program but its combination
        prog_hole helper{.frames = {}, .marker = hole, .scope_envs = scope_envs,
                         .exemplar = form == hole && lists ? nil : zero, .spec = spec, .max_size = hole_size_,
                         .max_terms = top_terms_ / 4};
        for (const auto& [first, second] : uses) {
            for (const uint32_t combiner : combiners) {
                const bool symmetric = combiner != symbols_.sub && combiner != symbols_.append;
                if (symmetric && second < first)
                    continue;
                // under the `let`, the helper is #0 and every input one further out
                const expr* call_first = make_functor_.make_functor(
                    symbols_.application,
                    {make_index_.make_index(0), make_index_.make_index(positions - first)});
                const expr* call_second = make_functor_.make_functor(
                    symbols_.application,
                    {make_index_.make_index(0), make_index_.make_index(positions - second)});
                helper.frames.push_back(make_functor_.make_functor(
                    symbols_.binding, {make_function(1, form),
                                       make_functor_.make_functor(combiner, {call_first, call_second})}));
            }
        }
        try_fill(helper, spec, best);
    }
}

PROG_DECOMPOSING_HYPOTHESES_TEMPLATE void PROG_DECOMPOSING_HYPOTHESES::try_fill(const prog_hole& hole,
                                                                                const prog_spec& spec,
                                                                                std::optional<const expr*>& best) {
    if (hole.scope_envs.empty() || hole.frames.empty() || solves_ >= max_solves_)
        return;
    // the content may make the whole no larger than the best so far less one (the frame's hole counts one node)
    const size_t room = find_room(best, count_nodes(hole.frames[0]) - 2);
    if (room == 0)
        return;
    prog_hole bounded = hole;
    bounded.max_size = std::min(hole.max_size, room);
    ++solves_;
    const std::optional<const expr*> filled = fill_hole_.fill_hole(bounded);
    if (filled.has_value())
        keep(filled.value(), spec, best);
}

// the candidate, when it meets the specification at every environment and is smaller than the best so far
PROG_DECOMPOSING_HYPOTHESES_TEMPLATE void PROG_DECOMPOSING_HYPOTHESES::keep(const expr* candidate,
                                                                            const prog_spec& spec,
                                                                            std::optional<const expr*>& best) {
    if (best.has_value() && count_nodes(best.value()) <= count_nodes(candidate))
        return;
    for (size_t j = 0; j < spec.envs.size(); ++j) {
        const std::optional<const expr*> value = evaluate_with_.evaluate_with(candidate, spec.envs[j]);
        if (!value.has_value() || !match_relation_.matches(spec.relation, value.value(), spec.outputs[j]))
            return;
    }
    best = candidate;
}

// the elements of a `cons` chain ending in `nil`
PROG_DECOMPOSING_HYPOTHESES_TEMPLATE std::optional<typename PROG_DECOMPOSING_HYPOTHESES::values_t>
PROG_DECOMPOSING_HYPOTHESES::find_list(const expr* list) const {
    values_t elements;
    for (const expr* rest = list;;) {
        const expr::functor* f = std::get_if<expr::functor>(&rest->content);
        if (f == nullptr)
            return std::nullopt;
        if (f->id == symbols_.nil && f->args.empty())
            return elements;
        if (f->id != symbols_.cons || f->args.size() != 2)
            return std::nullopt;
        elements.push_back(f->args[0]);
        rest = f->args[1];
    }
}

// the elements of the input at this position in every environment, when it is a list in each
PROG_DECOMPOSING_HYPOTHESES_TEMPLATE std::optional<std::vector<typename PROG_DECOMPOSING_HYPOTHESES::values_t>>
PROG_DECOMPOSING_HYPOTHESES::find_column_lists(const prog_spec& spec, size_t position) const {
    std::vector<values_t> lists;
    for (const values_t& env : spec.envs) {
        const std::optional<values_t> elements = find_list(env[position]);
        if (!elements.has_value())
            return std::nullopt;
        lists.push_back(elements.value());
    }
    return lists;
}

PROG_DECOMPOSING_HYPOTHESES_TEMPLATE bool PROG_DECOMPOSING_HYPOTHESES::is_tree(const expr* value) const {
    const expr::functor* f = std::get_if<expr::functor>(&value->content);
    const bool leaf = f != nullptr && f->id == symbols_.leaf && f->args.empty();
    const bool node = f != nullptr && f->id == symbols_.node && f->args.size() == 3;
    return leaf || node;
}

PROG_DECOMPOSING_HYPOTHESES_TEMPLATE bool PROG_DECOMPOSING_HYPOTHESES::same_shape(const expr* left,
                                                                                  const expr* right) const {
    if (!is_tree(left) || !is_tree(right))
        return false;
    const expr::functor& l = std::get<expr::functor>(left->content);
    const expr::functor& r = std::get<expr::functor>(right->content);
    if (l.id != r.id)
        return false;
    return l.id == symbols_.leaf || (same_shape(l.args[0], r.args[0]) && same_shape(l.args[2], r.args[2]));
}

// each node's value, at the environment with the fold's left result, the value and the right result
PROG_DECOMPOSING_HYPOTHESES_TEMPLATE void PROG_DECOMPOSING_HYPOTHESES::collect_nodes(const expr* in, const expr* out,
                                                                                     const values_t& env,
                                                                                     prog_spec& body) const {
    const expr::functor& i = std::get<expr::functor>(in->content);
    if (i.id != symbols_.node)
        return;
    const expr::functor& o = std::get<expr::functor>(out->content);
    values_t extended = env;
    extended.push_back(o.args[0]);
    extended.push_back(i.args[1]);
    extended.push_back(o.args[2]);
    body.envs.push_back(extended);
    body.outputs.push_back(o.args[1]);
    collect_nodes(i.args[0], o.args[0], env, body);
    collect_nodes(i.args[2], o.args[2], env, body);
}

PROG_DECOMPOSING_HYPOTHESES_TEMPLATE void PROG_DECOMPOSING_HYPOTHESES::collect_values(const expr* tree,
                                                                                      values_t& values) const {
    const expr::functor* f = std::get_if<expr::functor>(&tree->content);
    if (f == nullptr || f->id != symbols_.node || f->args.size() != 3)
        return;
    values.push_back(f->args[1]);
    collect_values(f->args[0], values);
    collect_values(f->args[2], values);
}

// a value for a variable the hole's content should not read (a sum's running total): varied, so a constant is not
// mistaken for it (were it always 0, the literal 0 would be pruned as the same term as the variable)
PROG_DECOMPOSING_HYPOTHESES_TEMPLATE const expr* PROG_DECOMPOSING_HYPOTHESES::make_placeholder(size_t index) {
    return make_literal_.make_literal(integer(static_cast<long>((index * 7) % 13) - 6));
}

PROG_DECOMPOSING_HYPOTHESES_TEMPLATE const expr* PROG_DECOMPOSING_HYPOTHESES::make_list(const values_t& elements) {
    const expr* list = make_functor_.make_functor(symbols_.nil, {});
    for (auto element = elements.rbegin(); element != elements.rend(); ++element)
        list = make_functor_.make_functor(symbols_.cons, {*element, list});
    return list;
}

// `lam(b)`, `lam(lam(b))`, or `lams(k, b)` for three parameters or more
PROG_DECOMPOSING_HYPOTHESES_TEMPLATE const expr* PROG_DECOMPOSING_HYPOTHESES::make_function(size_t parameters,
                                                                                            const expr* body) {
    if (parameters >= 3)
        return make_functor_.make_functor(symbols_.lambdas,
                                          {make_literal_.make_literal(integer(static_cast<long>(parameters))), body});
    const expr* function = body;
    for (size_t k = 0; k < parameters; ++k)
        function = make_functor_.make_functor(symbols_.lambda, {function});
    return function;
}

// the index of the variable at this position of the environments (the last is `#0`)
PROG_DECOMPOSING_HYPOTHESES_TEMPLATE const expr* PROG_DECOMPOSING_HYPOTHESES::make_input(const prog_spec& spec,
                                                                                         size_t position) {
    return make_index_.make_index(spec.envs[0].size() - 1 - position);
}

PROG_DECOMPOSING_HYPOTHESES_TEMPLATE size_t PROG_DECOMPOSING_HYPOTHESES::count_nodes(const expr* e) {
    size_t nodes = 0;
    std::vector<const expr*> stack{e};
    while (!stack.empty()) {
        const expr* at = stack.back();
        stack.pop_back();
        ++nodes;
        if (const expr::functor* f = std::get_if<expr::functor>(&at->content))
            for (const expr* arg : f->args)
                stack.push_back(arg);
    }
    return nodes;
}

PROG_DECOMPOSING_HYPOTHESES_TEMPLATE bool PROG_DECOMPOSING_HYPOTHESES::holds_hole(const expr* e) {
    if (std::holds_alternative<expr::var>(e->content))
        return true;
    for (const expr* arg : std::get<expr::functor>(e->content).args)
        if (holds_hole(arg))
            return true;
    return false;
}

#undef PROG_DECOMPOSING_HYPOTHESES
#undef PROG_DECOMPOSING_HYPOTHESES_TEMPLATE

#endif
