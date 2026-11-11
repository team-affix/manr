#ifndef PROG_ENUMERATIVE_HYPOTHESES_HPP
#define PROG_ENUMERATIVE_HYPOTHESES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/prog_hole.hpp"
#include "value_objects/prog_spec.hpp"
#include "value_objects/prog_symbols.hpp"
#include "value_objects/pw_data_point.hpp"

constexpr size_t k_prog_default_max_size = 15;
constexpr size_t k_prog_default_max_terms = 250000;
constexpr size_t k_prog_default_max_searches = 3;
constexpr size_t k_prog_default_min_points = 3;
constexpr size_t k_prog_default_max_body_size = 5;
constexpr size_t k_prog_default_max_unary_body_size = 8;
constexpr size_t k_prog_default_max_deduced_list_size = 3;
constexpr size_t k_prog_default_max_fold_body_size = 3;
constexpr size_t k_prog_default_max_base_size = 2;
constexpr size_t k_prog_default_max_bank = 2000;
constexpr size_t k_prog_default_max_probes = 24;
constexpr size_t k_prog_default_max_scope_envs = 64;

// Hypothesis source: the smallest program of `prog_symbols` that reproduces every sample, found by bottom-up
// enumeration by size, so the first program that matches is the smallest one the grammar has (the MDL model, up to
// the grammar). Data of any shape: the inputs and outputs are typed by their values (an integer, a list of integers,
// a list of lists, a tree of `leaf` and `node(l, v, r)`, a record `c(a, b, ...)` of integers, a list of records), a
// tuple input `point(a, ..., z)` is several inputs (`#(k-1)` to `#0`), and the program is proposed over them, closed
// by the piecewise rules. The grammar is typed: arithmetic, comparisons and connectives over integers; the list
// primitives; `map`, `filter`, `fold`, `scan` and `zipwith` with lambda bodies; `tfold` and `node` over trees; `field`
// of records; `reachable` over a list of edges. A lambda's body is enumerated in its own context, the enclosing one
// plus its parameters, so a body can read an outer input (`filter(lam(eq(#0, #2)), #0)`), nested two lambdas deep.
//
// Equivalent terms are pruned by their values (observational equivalence): a term is kept only when its values on
// the context's probe environments differ from every smaller term's. At the top, the environments are the samples'
// inputs (at most `max_probes` of them; a term matching their outputs is then checked on every sample, and the search
// stops at the first that passes); a lambda's parameters range over values pooled from the data (its integers, lists and their suffixes,
// subtrees, records), several per enclosing environment. Samples holding holes are left out of the match (the
// piecewise rules bind them afterwards). The search stops at `max_size` nodes or `max_terms` terms, and needs
// `min_points` samples. A lambda body has at most `max_body_size` nodes (one fewer two lambdas deep), and the base of a
// fold, scan or tree fold at most `max_base_size`; a commutative operation is built in one order of its arguments, and
// no operation on literals alone (its value is a literal already). Needs Results are memoized per set of samples, and at most `max_searches` sets are searched (the
// piecewise rules ask again for every subset a model leaves; a subset rarely needs a program the whole set did not).
//
// Two more entries serve a decomposing search (`prog_decomposing_hypotheses`): `find_program` searches one
// specification (`prog_spec`: environments, outputs, and the relation a value must have to its output, e.g. a
// permutation of it under a `sort`) for programs of at most a given size, and `fill_hole` enumerates the content of a hole in a frame (`prog_hole`: the
// frame `length(filter(lam(□), #0))`, or a helper `let(lam(□), app(#0, #2) - app(#0, #1))`), over the environments
// of the hole's own scope (at most `k_prog_default_max_scope_envs`), each candidate checked by evaluating the filled
// frame on the specification. Both take their own budget of terms and need no minimum of samples.
template<typename IEvaluateWith, typename IMatchRelation, typename IFindLiteralValue, typename IMakeLiteral,
         typename IMakeIndex, typename IMakeFunctor>
struct prog_enumerative_hypotheses {
    prog_enumerative_hypotheses(IEvaluateWith& evaluate_with,
                                IMatchRelation& match_relation,
                                IFindLiteralValue& find_literal_value,
                                IMakeLiteral& make_literal,
                                IMakeIndex& make_index,
                                IMakeFunctor& make_functor,
                                prog_symbols symbols,
                                size_t max_size = k_prog_default_max_size,
                                size_t max_terms = k_prog_default_max_terms,
                                size_t min_points = k_prog_default_min_points,
                                size_t max_searches = k_prog_default_max_searches,
                                size_t max_body_size = k_prog_default_max_body_size,
                                size_t max_base_size = k_prog_default_max_base_size);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
    std::optional<const expr*> find_program(const prog_spec& spec, size_t max_size, size_t max_terms);
    std::optional<const expr*> fill_hole(const prog_hole& hole);
private:
    // value types
    static constexpr uint8_t k_integer = 0;
    static constexpr uint8_t k_integers = 1;
    static constexpr uint8_t k_lists = 2;
    static constexpr uint8_t k_tree = 3;
    static constexpr uint8_t k_record = 4;
    static constexpr uint8_t k_records = 5;
    static constexpr uint8_t k_types = 6;
    static constexpr uint8_t k_unknown = 255;

    using values_t = std::vector<const expr*>;
    using signature_t = std::vector<const expr*>;
    struct term {
        const expr* e;
        signature_t values;
    };
    struct context {
        std::vector<uint8_t> types;
        size_t max_size;
        std::vector<values_t> envs;
        std::vector<std::vector<std::vector<term>>> bank;  // by type, then size
        std::vector<std::map<signature_t, const expr*>> seen;  // by type: each signature's first (smallest) term
        size_t depth;
    };
    // an argument of a production: data of a type, a lambda (its parameters and body type), or a literal index
    struct argument {
        uint8_t type;
        std::vector<uint8_t> parameters;
        bool lambda;
        int literal;
        size_t limit;
    };
    struct production {
        uint32_t symbol;
        uint8_t result;
        std::vector<argument> args;
        bool commutative;
    };

    std::optional<const expr*> search(const std::vector<pw_data_point>& points);
    std::optional<const expr*> run(const std::vector<values_t>& scope_envs, std::optional<uint8_t> output_type,
                                   size_t max_size, size_t max_terms);
    std::optional<uint8_t> find_common_type(const values_t& values) const;
    const expr* plug(const expr* frame, const expr* content);
    uint8_t find_type(const expr* value) const;
    void make_productions();
    static argument make_data_argument(uint8_t type);
    argument make_base_argument(uint8_t type) const;
    static argument make_function_argument(std::vector<uint8_t> parameters, uint8_t body);
    static argument make_literal_argument(int value);
    size_t find_context(const std::vector<uint8_t>& types, const std::vector<values_t>& envs, size_t depth,
                        size_t limit);
    void deduce(size_t size);
    void deduce_arithmetic(size_t size);
    void deduce_element_wise(size_t size);
    void deduce_folds(size_t size);
    void deduce_folds_over_terms();
    std::optional<size_t> find_production(uint32_t symbol, const std::vector<uint8_t>& parameters, uint8_t over) const;
    const expr* find_body(size_t child, uint8_t type, const std::vector<values_t>& envs, const values_t& outputs);
    void propose(const expr* e);
    static size_t count_nodes(const expr* e);
    std::optional<values_t> find_list(const expr* list) const;
    void make_child_contexts(size_t index);
    std::vector<values_t> extend_envs(const std::vector<values_t>& envs, const std::vector<uint8_t>& parameters);
    void grow(size_t index, size_t size);
    void combine(size_t index, const production& p, size_t position, size_t remaining, std::vector<const expr*>& args);
    bool admit(size_t index, uint8_t type, size_t size, const expr* e);
    const expr* make_lambda(size_t parameters, const expr* body);
    bool available(const context& c, uint8_t type) const;
    bool check(const term& candidate);
    void collect_pools(const expr* value);
    static bool holds_hole(const expr* e);

    IEvaluateWith& evaluate_with_;
    IMatchRelation& match_relation_;
    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IMakeIndex& make_index_;
    IMakeFunctor& make_functor_;
    prog_symbols symbols_;
    size_t max_size_;
    size_t max_terms_;
    size_t min_points_;
    size_t max_searches_;
    size_t max_body_size_;
    size_t max_base_size_;
    std::vector<production> productions_;
    std::vector<context> contexts_;
    std::map<std::vector<uint8_t>, size_t> context_index_;
    std::vector<std::vector<const expr*>> pools_;  // by type
    std::vector<size_t> lambda_children_;           // per (context, production, argument): the child context
    std::map<std::vector<size_t>, size_t> children_;
    size_t terms_;
    size_t current_size_;
    size_t top_;
    uint8_t output_type_;
    std::vector<values_t> checked_envs_;
    values_t checked_outputs_;
    std::optional<const expr*> found_;
    std::optional<const expr*> deduced_;
    size_t deduced_size_;
    values_t probe_outputs_;
    std::map<std::vector<const expr*>, std::optional<const expr*>> memo_;
    size_t budget_;
    uint8_t relation_;
    std::vector<const expr*> frames_;
    const expr* marker_;
    prog_spec framed_;
};

#define PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE                                                           \
    template<typename IEvaluateWith, typename IMatchRelation, typename IFindLiteralValue, typename IMakeLiteral, \
             typename IMakeIndex, typename IMakeFunctor>
#define PROG_ENUMERATIVE_HYPOTHESES                                                                                \
    prog_enumerative_hypotheses<IEvaluateWith, IMatchRelation, IFindLiteralValue, IMakeLiteral, IMakeIndex,       \
                                IMakeFunctor>

PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE PROG_ENUMERATIVE_HYPOTHESES::prog_enumerative_hypotheses(
    IEvaluateWith& evaluate_with,
    IMatchRelation& match_relation,
    IFindLiteralValue& find_literal_value,
    IMakeLiteral& make_literal,
    IMakeIndex& make_index,
    IMakeFunctor& make_functor,
    prog_symbols symbols,
    size_t max_size,
    size_t max_terms,
    size_t min_points,
    size_t max_searches,
    size_t max_body_size,
    size_t max_base_size)
    : evaluate_with_(evaluate_with)
    , match_relation_(match_relation)
    , find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , make_index_(make_index)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , max_size_(max_size)
    , max_terms_(max_terms)
    , min_points_(min_points)
    , max_searches_(max_searches)
    , max_body_size_(max_body_size)
    , max_base_size_(max_base_size)
    , productions_()
    , contexts_()
    , context_index_()
    , pools_()
    , lambda_children_()
    , children_()
    , terms_(0)
    , current_size_(0)
    , top_(0)
    , output_type_(k_unknown)
    , checked_envs_()
    , checked_outputs_()
    , found_()
    , deduced_()
    , deduced_size_(0)
    , probe_outputs_()
    , memo_()
    , budget_(max_terms)
    , relation_(k_prog_relation_equal)
    , frames_()
    , marker_(nullptr)
    , framed_() {
    make_productions();
}

PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE void PROG_ENUMERATIVE_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    std::vector<const expr*> key;
    for (const pw_data_point& point : points) {
        key.push_back(point.input);
        key.push_back(point.output);
    }
    auto known = memo_.find(key);
    if (known == memo_.end() && memo_.size() >= max_searches_)
        return;
    if (known == memo_.end())
        known = memo_.emplace(key, search(points)).first;
    if (known->second.has_value())
        out.push_back(known->second.value());
}

PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE std::optional<const expr*> PROG_ENUMERATIVE_HYPOTHESES::search(
    const std::vector<pw_data_point>& points) {
    // the samples without holes, their inputs as environments
    prog_spec spec{.envs = {}, .outputs = {}, .relation = k_prog_relation_equal};
    for (const pw_data_point& point : points) {
        if (holds_hole(point.input) || holds_hole(point.output))
            continue;
        const expr::functor* f = std::get_if<expr::functor>(&point.input->content);
        const bool tuple = f != nullptr && f->id == symbols_.point;
        spec.envs.push_back(tuple ? f->args : values_t{point.input});
        spec.outputs.push_back(point.output);
    }
    if (spec.envs.size() < min_points_)
        return std::nullopt;
    return find_program(spec, max_size_, max_terms_);
}

PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE std::optional<const expr*> PROG_ENUMERATIVE_HYPOTHESES::find_program(
    const prog_spec& spec, size_t max_size, size_t max_terms) {
    if (spec.envs.empty())
        return std::nullopt;
    frames_.clear();
    relation_ = spec.relation;
    // the probes are the first samples; the rest only check a candidate that matches them
    const size_t probes = std::min(spec.envs.size(), k_prog_default_max_probes);
    checked_envs_.assign(spec.envs.begin() + static_cast<long>(probes), spec.envs.end());
    checked_outputs_ = spec.outputs;
    probe_outputs_.assign(spec.outputs.begin(), spec.outputs.begin() + static_cast<long>(probes));
    const std::vector<values_t> envs(spec.envs.begin(), spec.envs.begin() + static_cast<long>(probes));
    return run(envs, std::nullopt, std::min(max_size, max_size_), max_terms);
}

PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE std::optional<const expr*> PROG_ENUMERATIVE_HYPOTHESES::fill_hole(
    const prog_hole& hole) {
    if (hole.scope_envs.empty() || hole.spec.envs.empty())
        return std::nullopt;
    frames_ = hole.frames;
    marker_ = hole.marker;
    framed_ = hole.spec;
    relation_ = hole.spec.relation;
    checked_envs_.clear();
    checked_outputs_.clear();
    probe_outputs_.clear();
    const size_t scope = std::min(hole.scope_envs.size(), k_prog_default_max_scope_envs);
    const std::vector<values_t> envs(hole.scope_envs.begin(), hole.scope_envs.begin() + static_cast<long>(scope));
    const std::optional<const expr*> found = run(envs, find_type(hole.exemplar), hole.max_size, hole.max_terms);
    frames_.clear();
    return found;
}

// the type all these values share (an empty list is any list: the most specific type seen)
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE std::optional<uint8_t> PROG_ENUMERATIVE_HYPOTHESES::find_common_type(
    const values_t& values) const {
    uint8_t common = k_unknown;
    for (const expr* value : values) {
        const uint8_t type = find_type(value);
        if (type == k_unknown)
            return std::nullopt;
        const bool refine = common == k_integers && (type == k_lists || type == k_records);
        const bool general = type == k_integers && (common == k_lists || common == k_records);
        if (common == k_unknown || refine)
            common = type;
        if (type != common && !general)
            return std::nullopt;
    }
    if (common == k_unknown)
        return std::nullopt;
    return common;
}

// the search over environments (the probes, or a hole's scope) for a term of the output's type
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE std::optional<const expr*> PROG_ENUMERATIVE_HYPOTHESES::run(
    const std::vector<values_t>& scope_envs, std::optional<uint8_t> output_type, size_t max_size, size_t max_terms) {
    std::vector<uint8_t> input_types;
    for (size_t position = 0; position < scope_envs[0].size(); ++position) {
        values_t column;
        for (const values_t& env : scope_envs) {
            if (env.size() != scope_envs[0].size())
                return std::nullopt;
            column.push_back(env[position]);
        }
        const std::optional<uint8_t> type = find_common_type(column);
        if (!type.has_value())
            return std::nullopt;
        input_types.push_back(type.value());
    }
    if (!output_type.has_value())
        output_type = find_common_type(checked_outputs_);
    if (!output_type.has_value() || output_type.value() == k_unknown)
        return std::nullopt;
    contexts_.clear();
    context_index_.clear();
    children_.clear();
    terms_ = 0;
    budget_ = max_terms;
    pools_.assign(k_types, {});
    pools_[k_integer] = {make_literal_.make_literal(integer(0)), make_literal_.make_literal(integer(1))};
    pools_[k_integers] = {make_functor_.make_functor(symbols_.nil, {})};
    pools_[k_tree] = {make_functor_.make_functor(symbols_.leaf, {})};
    for (const values_t& env : scope_envs)
        for (const expr* input : env)
            collect_pools(input);
    for (const expr* output : frames_.empty() ? checked_outputs_ : framed_.outputs)
        collect_pools(output);
    found_.reset();
    deduced_.reset();
    output_type_ = output_type.value();
    top_ = find_context(input_types, scope_envs, 0, max_size);
    // contexts are made before any term: every lambda body needs its bank from size 1
    for (size_t index = 0; index < contexts_.size(); ++index)
        make_child_contexts(index);
    // bottom-up by size; after each size, the deductions may propose a larger program, kept while nothing smaller
    // turns up
    size_t reached = 0;
    for (size_t size = 1; size <= max_size && terms_ < budget_ && !found_.has_value(); ++size) {
        for (size_t index = 0; index < contexts_.size() && !found_.has_value(); ++index)
            grow(index, size);
        if (found_.has_value() || !frames_.empty())
            continue;
        reached = size;
        deduce(size);
        if (deduced_.has_value() && deduced_size_ <= size + 1)
            break;
    }
    // a budget spent before the bodies were complete: the deductions still read what was built
    const bool short_of_bodies = reached < k_prog_default_max_unary_body_size && !deduced_.has_value();
    if (frames_.empty() && !found_.has_value() && short_of_bodies)
        deduce(k_prog_default_max_unary_body_size);
    return found_.has_value() ? found_ : deduced_;
}

PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE uint8_t PROG_ENUMERATIVE_HYPOTHESES::find_type(const expr* value) const {
    if (find_literal_value_.find_value(value).has_value())
        return k_integer;
    const expr::functor* f = std::get_if<expr::functor>(&value->content);
    if (f == nullptr)
        return k_unknown;
    if (f->id == symbols_.leaf || f->id == symbols_.node)
        return k_tree;
    if (f->id == symbols_.nil)
        return k_integers;
    if (f->id == symbols_.cons && f->args.size() == 2) {
        const uint8_t element = find_type(f->args[0]);
        return element == k_integer ? k_integers : element == k_integers ? k_lists
                                                 : element == k_record   ? k_records
                                                                         : k_unknown;
    }
    if (f->args.empty())
        return k_unknown;
    for (const expr* arg : f->args)
        if (!find_literal_value_.find_value(arg).has_value())
            return k_unknown;
    return k_record;
}

// The grammar, by result type. Within a type, the order is the order terms enter a bank, which matters when a bank
// fills: higher-order programs and list primitives first, arithmetic last.
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE void PROG_ENUMERATIVE_HYPOTHESES::make_productions() {
    const prog_symbols& s = symbols_;
    const argument integer_data = make_data_argument(k_integer);
    const argument integers_data = make_data_argument(k_integers);
    const argument integer_base = make_base_argument(k_integer);
    const argument integers_base = make_base_argument(k_integers);
    const std::vector<uint8_t> two_integers{k_integer, k_integer};
    // integers
    productions_.push_back({s.fold, k_integer,
                            {make_function_argument(two_integers, k_integer), integer_base, integers_data}, false});
    productions_.push_back({s.fold, k_integer,
                            {make_function_argument({k_integer, k_integers}, k_integer), integer_base,
                             make_data_argument(k_lists)}, false});
    productions_.push_back({s.fold, k_integer,
                            {make_function_argument({k_integer, k_record}, k_integer), integer_base,
                             make_data_argument(k_records)}, false});
    productions_.push_back({s.tree_fold, k_integer,
                            {make_function_argument({k_integer, k_integer, k_integer}, k_integer), integer_base,
                             make_data_argument(k_tree)}, false});
    for (const uint8_t list : {k_integers, k_lists, k_records})
        productions_.push_back({s.length, k_integer, {make_data_argument(list)}, false});
    productions_.push_back({s.head, k_integer, {integers_data}, false});
    productions_.push_back({s.member, k_integer, {integer_data, integers_data}, false});
    productions_.push_back({s.field, k_integer, {make_literal_argument(0), make_data_argument(k_record)}, false});
    productions_.push_back({s.field, k_integer, {make_literal_argument(1), make_data_argument(k_record)}, false});
    productions_.push_back({s.equal, k_integer, {integers_data, integers_data}, true});
    for (const uint32_t op : {s.add, s.sub, s.mul, s.max, s.min, s.less, s.less_equal, s.equal, s.conjunction,
                              s.disjunction}) {
        const bool commutative = op != s.sub && op != s.less && op != s.less_equal;
        productions_.push_back({op, k_integer, {integer_data, integer_data}, commutative});
    }
    productions_.push_back({s.negation, k_integer, {integer_data}, false});
    productions_.push_back({s.mod, k_integer, {integer_data, integer_data}, false});
    // lists of integers
    productions_.push_back({s.map, k_integers, {make_function_argument({k_integer}, k_integer), integers_data}, false});
    productions_.push_back({s.map, k_integers,
                            {make_function_argument({k_integers}, k_integer), make_data_argument(k_lists)}, false});
    productions_.push_back({s.map, k_integers,
                            {make_function_argument({k_record}, k_integer), make_data_argument(k_records)}, false});
    productions_.push_back({s.filter, k_integers,
                            {make_function_argument({k_integer}, k_integer), integers_data}, false});
    productions_.push_back({s.scan, k_integers,
                            {make_function_argument(two_integers, k_integer), integer_base, integers_data}, false});
    productions_.push_back({s.zip_with, k_integers,
                            {make_function_argument(two_integers, k_integer), integers_data, integers_data}, false});
    productions_.push_back({s.fold, k_integers,
                            {make_function_argument({k_integers, k_integer}, k_integers), integers_base,
                             integers_data}, false});
    productions_.push_back({s.fold, k_integers,
                            {make_function_argument({k_integers, k_integers}, k_integers), integers_base,
                             make_data_argument(k_lists)}, false});
    productions_.push_back({s.tree_fold, k_integers,
                            {make_function_argument({k_integers, k_integer, k_integers}, k_integers), integers_base,
                             make_data_argument(k_tree)}, false});
    productions_.push_back({s.reachable, k_integers, {make_data_argument(k_records), integer_data}, false});
    for (const uint32_t op : {s.tail, s.reverse, s.sort, s.unique})
        productions_.push_back({op, k_integers, {integers_data}, false});
    productions_.push_back({s.head, k_integers, {make_data_argument(k_lists)}, false});
    productions_.push_back({s.append, k_integers, {integers_data, integers_data}, false});
    productions_.push_back({s.cons, k_integers, {integer_data, integers_data}, false});
    // lists of lists, trees, records
    for (const uint32_t op : {s.tail, s.reverse})
        productions_.push_back({op, k_lists, {make_data_argument(k_lists)}, false});
    productions_.push_back({s.tree_fold, k_tree,
                            {make_function_argument({k_tree, k_integer, k_tree}, k_tree), make_base_argument(k_tree),
                             make_data_argument(k_tree)}, false});
    productions_.push_back({s.node, k_tree,
                            {make_data_argument(k_tree), integer_data, make_data_argument(k_tree)}, false});
    productions_.push_back({s.head, k_record, {make_data_argument(k_records)}, false});
    productions_.push_back({s.tail, k_records, {make_data_argument(k_records)}, false});
    productions_.push_back({s.filter, k_records,
                            {make_function_argument({k_record}, k_integer), make_data_argument(k_records)}, false});
}

PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE typename PROG_ENUMERATIVE_HYPOTHESES::argument
PROG_ENUMERATIVE_HYPOTHESES::make_data_argument(uint8_t type) {
    return argument{.type = type, .parameters = {}, .lambda = false, .literal = -1, .limit = 0};
}

// the base of a fold, scan or tree fold: data of a few nodes
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE typename PROG_ENUMERATIVE_HYPOTHESES::argument
PROG_ENUMERATIVE_HYPOTHESES::make_base_argument(uint8_t type) const {
    return argument{.type = type, .parameters = {}, .lambda = false, .literal = -1, .limit = max_base_size_};
}

PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE typename PROG_ENUMERATIVE_HYPOTHESES::argument
PROG_ENUMERATIVE_HYPOTHESES::make_function_argument(std::vector<uint8_t> parameters, uint8_t body) {
    return argument{.type = body, .parameters = std::move(parameters), .lambda = true, .literal = -1, .limit = 0};
}

PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE typename PROG_ENUMERATIVE_HYPOTHESES::argument
PROG_ENUMERATIVE_HYPOTHESES::make_literal_argument(int value) {
    return argument{.type = k_integer, .parameters = {}, .lambda = false, .literal = value, .limit = 0};
}

// the context of these variable types (innermost last) over these environments, made when new
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE size_t PROG_ENUMERATIVE_HYPOTHESES::find_context(
    const std::vector<uint8_t>& types, const std::vector<values_t>& envs, size_t depth, size_t limit) {
    const auto known = context_index_.find(types);
    if (known != context_index_.end())
        return known->second;
    context made{.types = types, .max_size = limit, .envs = envs, .bank = {}, .seen = {}, .depth = depth};
    made.bank.assign(k_types, std::vector<std::vector<term>>(max_size_ + 1));
    made.seen.assign(k_types, {});
    contexts_.push_back(std::move(made));
    context_index_.emplace(types, contexts_.size() - 1);
    return contexts_.size() - 1;
}

// the contexts of the lambdas the productions can use here, two lambdas deep at most
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE void PROG_ENUMERATIVE_HYPOTHESES::make_child_contexts(size_t index) {
    if (contexts_[index].depth >= 2)
        return;
    for (size_t p = 0; p < productions_.size(); ++p) {
        for (size_t a = 0; a < productions_[p].args.size(); ++a) {
            const argument& arg = productions_[p].args[a];
            if (!arg.lambda)
                continue;
            const uint8_t over = productions_[p].args.back().type;
            if (!available(contexts_[index], over))
                continue;
            std::vector<uint8_t> types = contexts_[index].types;
            types.insert(types.end(), arg.parameters.begin(), arg.parameters.end());
            const std::vector<values_t> envs = extend_envs(contexts_[index].envs, arg.parameters);
            const size_t depth = contexts_[index].depth + 1;
            const size_t body = arg.parameters.size() == 1 ? k_prog_default_max_unary_body_size : max_body_size_;
            // nested two deep: a filter or map inside a step (one node fewer), a fold inside a map (two fewer)
            const size_t nested = arg.parameters.size() == 1 ? max_body_size_ - 1 : max_body_size_ - 2;
            const size_t child = find_context(types, envs, depth, depth == 1 ? body : nested);
            children_.emplace(std::vector<size_t>{index, p, a}, child);
        }
    }
}

// each environment extended by parameter values from the pools, a bounded number per environment
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE std::vector<typename PROG_ENUMERATIVE_HYPOTHESES::values_t>
PROG_ENUMERATIVE_HYPOTHESES::extend_envs(const std::vector<values_t>& envs, const std::vector<uint8_t>& parameters) {
    std::vector<values_t> extended;
    const size_t per_env = std::max<size_t>(2, 48 / std::max<size_t>(1, envs.size()));
    for (size_t e = 0; e < envs.size(); ++e) {
        for (size_t k = 0; k < per_env; ++k) {
            values_t env = envs[e];
            bool complete = true;
            for (size_t p = 0; p < parameters.size(); ++p) {
                const std::vector<const expr*>& pool = pools_[parameters[p]];
                if (pool.empty()) {
                    complete = false;
                    break;
                }
                env.push_back(pool[(e * 7 + k * (3 + 2 * p) + p * 5) % pool.size()]);
            }
            if (complete)
                extended.push_back(env);
        }
    }
    return extended;
}

PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE void PROG_ENUMERATIVE_HYPOTHESES::grow(size_t index, size_t size) {
    context& c = contexts_[index];
    if (size > c.max_size)
        return;
    if (size == 1) {
        for (size_t k = 0; k < c.types.size(); ++k)
            admit(index, c.types[c.types.size() - 1 - k], 1, make_index_.make_index(k));
        for (const int value : {0, 1, 2})
            admit(index, k_integer, 1, make_literal_.make_literal(integer(value)));
        admit(index, k_integers, 1, make_functor_.make_functor(symbols_.nil, {}));
        if (available(c, k_tree))
            admit(index, k_tree, 1, make_functor_.make_functor(symbols_.leaf, {}));
        // the data's own integers as literals at the top only (a node to search for, a constant offset)
        for (size_t k = 0; k < pools_[k_integer].size() && c.depth == 0; ++k)
            admit(index, k_integer, 1, pools_[k_integer][k]);
        return;
    }
    for (const production& p : productions_) {
        bool usable = available(c, p.result);
        for (const argument& arg : p.args)
            usable = usable && (arg.lambda || arg.literal >= 0 || available(c, arg.type));
        if (!usable)
            continue;
        current_size_ = size;
        std::vector<const expr*> args;
        combine(index, p, 0, size - 1, args);
        if (terms_ >= budget_)
            return;
    }
}

// every way to fill the production's arguments from `position` on with `remaining` nodes
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE void PROG_ENUMERATIVE_HYPOTHESES::combine(size_t index, const production& p,
                                                                              size_t position, size_t remaining,
                                                                              std::vector<const expr*>& args) {
    if (terms_ >= budget_)
        return;
    if (position == p.args.size()) {
        if (remaining != 0)
            return;
        // one order of a commutative operation's arguments, and nothing on literals alone
        if (p.commutative && args.size() == 2 && args[1] < args[0])
            return;
        bool literal = true;
        for (const expr* arg : args)
            literal = literal && find_literal_value_.find_value(arg).has_value();
        if (literal)
            return;
        admit(index, p.result, current_size_, make_functor_.make_functor(p.symbol, args));
        return;
    }
    // a full bank takes no more terms of this size
    // a full bank stops the whole production at this size
    if (contexts_[index].bank[p.result][current_size_].size() >= k_prog_default_max_bank)
        return;
    const argument& arg = p.args[position];
    const size_t later = p.args.size() - position - 1;  // at least one node for each later argument
    if (remaining < 1 + later)
        return;
    if (arg.literal >= 0) {
        args.push_back(make_literal_.make_literal(integer(arg.literal)));
        combine(index, p, position + 1, remaining - 1, args);
        args.pop_back();
        return;
    }
    const size_t overhead = arg.lambda ? (arg.parameters.size() == 3 ? 2 : arg.parameters.size()) : 0;
    const size_t source = arg.lambda ? 0 : index;
    std::optional<size_t> child;
    if (arg.lambda) {
        const auto found = children_.find(std::vector<size_t>{index, static_cast<size_t>(&p - productions_.data()),
                                                              position});
        if (found == children_.end())
            return;
        child = found->second;
    }
    const size_t bank = child.has_value() ? child.value() : source;
    for (size_t size = 1; size + overhead + later <= remaining && (arg.limit == 0 || size <= arg.limit); ++size) {
        for (const term& t : contexts_[bank].bank[arg.type][size]) {
            args.push_back(arg.lambda ? make_lambda(arg.parameters.size(), t.e) : t.e);
            combine(index, p, position + 1, remaining - size - overhead, args);
            args.pop_back();
            if (terms_ >= budget_)
                return;
        }
    }
}

// keeps a term of this type whose values differ from every kept term's, at its size (0: computed from its nodes)
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE bool PROG_ENUMERATIVE_HYPOTHESES::admit(size_t index, uint8_t type, size_t size,
                                                                            const expr* e) {
    ++terms_;
    context& c = contexts_[index];
    const size_t nodes = size == 0 ? count_nodes(e) : size;
    if (nodes > c.max_size || c.bank[type][nodes].size() >= k_prog_default_max_bank)
        return false;
    signature_t values;
    values.reserve(c.envs.size());
    bool any = false;
    for (const values_t& env : c.envs) {
        const std::optional<const expr*> value = evaluate_with_.evaluate_with(e, env);
        values.push_back(value.has_value() ? value.value() : nullptr);
        any = any || value.has_value();
    }
    if (!any || !c.seen[type].emplace(values, e).second)
        return false;
    c.bank[type][nodes].push_back(term{.e = e, .values = std::move(values)});
    const bool candidate = index == top_ && type == output_type_;
    if (candidate && check(c.bank[type][nodes].back())) {
        found_ = frames_.empty() ? e : found_;
        terms_ = budget_;
    }
    return true;
}

// The deductions: programs read backwards from the outputs, over the terms the enumeration has so far.
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE void PROG_ENUMERATIVE_HYPOTHESES::deduce(size_t size) {
    const bool exact = relation_ == k_prog_relation_equal;
    if (exact && output_type_ == k_integer)
        deduce_arithmetic(size);
    if (exact && output_type_ == k_integers)
        deduce_element_wise(size);
    deduce_folds(size);
}

// fold(f, b, L) over list terms the enumeration built (not only the inputs), with small bodies (`max_fold_body_size`)
// and the base read from the samples where L is empty (or 0 and 1): a fold of a computed list (a dot product)
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE void PROG_ENUMERATIVE_HYPOTHESES::deduce_folds_over_terms() {
    context& top = contexts_[top_];
    if (output_type_ != k_integer)
        return;
    const std::optional<size_t> production = find_production(symbols_.fold, {k_integer, k_integer}, k_integers);
    if (!production.has_value())
        return;
    const auto child = children_.find(std::vector<size_t>{top_, production.value(), 0});
    if (child == children_.end())
        return;
    const context& bodies = contexts_[child->second];
    const expr* nil = make_functor_.make_functor(symbols_.nil, {});
    for (size_t list_size = 2; list_size <= std::min<size_t>(top.max_size, 8); ++list_size) {
        for (const term& list : top.bank[k_integers][list_size]) {
            std::vector<const expr*> bases;
            for (size_t j = 0; j < list.values.size(); ++j)
                if (list.values[j] == nil && std::find(bases.begin(), bases.end(), probe_outputs_[j]) == bases.end())
                    bases.push_back(probe_outputs_[j]);
            if (bases.size() > 1)
                continue;
            if (bases.empty())
                bases = {make_literal_.make_literal(integer(0)), make_literal_.make_literal(integer(1))};
            for (const expr* base : bases)
                for (size_t body_size = k_prog_default_max_fold_body_size; body_size <= k_prog_default_max_fold_body_size; ++body_size)
                    for (const term& body : bodies.bank[k_integer][body_size])
                        propose(make_functor_.make_functor(symbols_.fold, {make_lambda(2, body.e), base, list.e}));
        }
    }
}

// fold(f, b, L) and tfold(f, b, T) over the inputs: b is the output at an empty input (or a small term), and f is
// screened on the inputs whose parts are all empty (a one-element list, a node over two leaves), then checked whole
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE void PROG_ENUMERATIVE_HYPOTHESES::deduce_folds(size_t size) {
    if (size != k_prog_default_max_unary_body_size)
        return;
    deduce_folds_over_terms();
    context& top = contexts_[top_];
    const uint8_t element_of[k_types] = {k_unknown, k_integer, k_integers, k_unknown, k_unknown, k_record};
    for (size_t k = 0; k < top.types.size(); ++k) {
        const uint8_t type = top.types[top.types.size() - 1 - k];
        const bool tree = type == k_tree;
        const uint8_t element = tree ? k_integer : element_of[type];
        if (element == k_unknown)
            continue;
        const std::vector<uint8_t> parameters =
            tree ? std::vector<uint8_t>{output_type_, k_integer, output_type_}
                 : std::vector<uint8_t>{output_type_, element};
        const std::optional<size_t> production =
            find_production(tree ? symbols_.tree_fold : symbols_.fold, parameters, type);
        if (!production.has_value())
            continue;
        const auto child = children_.find(std::vector<size_t>{top_, production.value(), 0});
        if (child == children_.end())
            continue;
        const expr* input = make_index_.make_index(k);
        // the base: the output at an empty input, else every small term of the output's type
        std::vector<const expr*> bases;
        for (size_t j = 0; j < top.envs.size(); ++j) {
            const expr::functor* f = std::get_if<expr::functor>(&top.envs[j][top.envs[j].size() - 1 - k]->content);
            const bool empty = f != nullptr && f->args.empty() && (f->id == symbols_.nil || f->id == symbols_.leaf);
            if (empty && std::find(bases.begin(), bases.end(), probe_outputs_[j]) == bases.end())
                bases.push_back(probe_outputs_[j]);
        }
        if (bases.empty())
            for (size_t base_size = 1; base_size <= max_base_size_; ++base_size)
                for (const term& base : top.bank[output_type_][base_size])
                    bases.push_back(base.e);
        for (const expr* base : bases) {
            // the screen: f(b, h) for one-element lists, f(b, v, b) for nodes over two leaves
            std::vector<values_t> envs;
            values_t wanted;
            for (size_t j = 0; j < top.envs.size(); ++j) {
                const expr* value = top.envs[j][top.envs[j].size() - 1 - k];
                const expr::functor* f = std::get_if<expr::functor>(&value->content);
                if (f == nullptr)
                    continue;
                values_t env = top.envs[j];
                const bool single = !tree && f->id == symbols_.cons && f->args.size() == 2
                                    && std::get<expr::functor>(f->args[1]->content).id == symbols_.nil;
                const bool twig = tree && f->id == symbols_.node && f->args.size() == 3
                                  && std::get<expr::functor>(f->args[0]->content).id == symbols_.leaf
                                  && std::get<expr::functor>(f->args[2]->content).id == symbols_.leaf;
                if (single) {
                    env.push_back(base);
                    env.push_back(f->args[0]);
                }
                if (twig) {
                    env.push_back(base);
                    env.push_back(f->args[1]);
                    env.push_back(base);
                }
                if (!single && !twig)
                    continue;
                envs.push_back(env);
                wanted.push_back(probe_outputs_[j]);
            }
            const context& bodies = contexts_[child->second];
            for (size_t body_size = 1; body_size <= bodies.max_size; ++body_size) {
                for (const term& body : bodies.bank[output_type_][body_size]) {
                    bool screened = true;
                    for (size_t i = 0; i < envs.size() && screened; ++i) {
                        const std::optional<const expr*> value = evaluate_with_.evaluate_with(body.e, envs[i]);
                        screened = value.has_value() && match_relation_.matches(relation_, value.value(), wanted[i]);
                    }
                    if (!screened)
                        continue;
                    const expr* function = make_lambda(parameters.size(), body.e);
                    propose(make_functor_.make_functor(tree ? symbols_.tree_fold : symbols_.fold,
                                                       {function, base, input}));
                }
            }
        }
    }
}

// out = A op B for op in +, -, *: for each integer term A, the values B must have, looked up among the terms by value
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE void PROG_ENUMERATIVE_HYPOTHESES::deduce_arithmetic(size_t size) {
    context& top = contexts_[top_];
    std::vector<integer> wanted;
    for (const expr* output : probe_outputs_) {
        const std::optional<integer> value = find_literal_value_.find_value(output);
        if (!value.has_value())
            return;
        wanted.push_back(value.value());
    }
    for (size_t a_size = 1; a_size <= size; ++a_size) {
        for (const term& a : top.bank[k_integer][a_size]) {
            std::vector<integer> values;
            for (const expr* value : a.values) {
                const std::optional<integer> number =
                    value == nullptr ? std::optional<integer>{} : find_literal_value_.find_value(value);
                if (!number.has_value())
                    break;
                values.push_back(number.value());
            }
            if (values.size() != wanted.size())
                continue;
            for (const uint32_t op : {symbols_.add, symbols_.sub, symbols_.mul}) {
                for (const bool a_first : {true, false}) {
                    // out = a + b, a - b, b - a, a * b: the b each sample needs (any b where a and out are 0 for *)
                    signature_t needed;
                    bool free = false;
                    for (size_t i = 0; i < wanted.size(); ++i) {
                        const integer& x = values[i];
                        const integer& y = wanted[i];
                        if (op == symbols_.mul && x == 0 && y == 0) {
                            needed.push_back(nullptr);
                            free = true;
                            continue;
                        }
                        if (op == symbols_.mul && (x == 0 || y % x != 0))
                            break;
                        const integer b = op == symbols_.add ? integer(y - x)
                                          : op == symbols_.mul ? integer(y / x)
                                          : a_first           ? integer(x - y)
                                                              : integer(y + x);
                        needed.push_back(make_literal_.make_literal(b));
                    }
                    if (needed.size() != wanted.size())
                        continue;
                    const expr* b = nullptr;
                    const auto found = free ? top.seen[k_integer].end() : top.seen[k_integer].find(needed);
                    if (found != top.seen[k_integer].end())
                        b = found->second;
                    for (size_t b_size = 1; free && b == nullptr && b_size <= size; ++b_size) {
                        for (const term& other : top.bank[k_integer][b_size]) {
                            bool fits = true;
                            for (size_t i = 0; i < needed.size() && fits; ++i)
                                fits = needed[i] == nullptr || other.values[i] == needed[i];
                            if (!fits)
                                continue;
                            b = other.e;
                            break;
                        }
                    }
                    if (b == nullptr)
                        continue;
                    const std::vector<const expr*> args =
                        a_first ? std::vector<const expr*>{a.e, b} : std::vector<const expr*>{b, a.e};
                    propose(make_functor_.make_functor(op, args));
                    if (op != symbols_.sub)
                        break;
                }
            }
        }
    }
}

// a list read element by element, once the bodies are complete: map(f, L), zipwith(f, A, B) and scan(f, b, L) over
// small list terms, the body
// deduced from the examples the outputs give, searched among the bodies by evaluating them on those examples
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE void PROG_ENUMERATIVE_HYPOTHESES::deduce_element_wise(size_t size) {
    if (size != k_prog_default_max_unary_body_size)
        return;
    context& top = contexts_[top_];
    std::vector<values_t> outputs;
    for (const expr* output : probe_outputs_) {
        const std::optional<values_t> elements = find_list(output);
        if (!elements.has_value())
            return;
        outputs.push_back(elements.value());
    }
    std::vector<const term*> lists[k_types];
    for (const uint8_t type : {k_integers, k_lists, k_records})
        for (size_t list_size = 1; list_size <= k_prog_default_max_deduced_list_size; ++list_size)
            for (const term& t : top.bank[type][list_size])
                lists[type].push_back(&t);
    // map(f, L): out[i] = f(L[i])
    for (const uint8_t type : {k_integers, k_lists, k_records}) {
        const uint8_t element = type == k_integers ? k_integer : type == k_lists ? k_integers : k_record;
        const std::optional<size_t> production = find_production(symbols_.map, {element}, type);
        if (!production.has_value())
            continue;
        const auto child = children_.find(std::vector<size_t>{top_, production.value(), 0});
        if (child == children_.end())
            continue;
        for (const term* list : lists[type]) {
            std::vector<values_t> envs;
            values_t wanted;
            bool aligned = true;
            for (size_t j = 0; j < top.envs.size() && aligned; ++j) {
                const std::optional<values_t> elements =
                    list->values[j] == nullptr ? std::nullopt : find_list(list->values[j]);
                aligned = elements.has_value() && elements.value().size() == outputs[j].size();
                for (size_t i = 0; aligned && i < outputs[j].size(); ++i) {
                    values_t env = top.envs[j];
                    env.push_back(elements.value()[i]);
                    envs.push_back(env);
                    wanted.push_back(outputs[j][i]);
                }
            }
            if (!aligned || wanted.empty())
                continue;
            const expr* body = find_body(child->second, k_integer, envs, wanted);
            if (body != nullptr)
                propose(make_functor_.make_functor(symbols_.map, {make_lambda(1, body), list->e}));
        }
    }
    // zipwith(f, A, B): out[i] = f(A[i], B[i]); scan(f, b, L): out[i] = f(out[i-1], L[i]), f(b, L[0]) = out[0]
    const std::optional<size_t> zip = find_production(symbols_.zip_with, {k_integer, k_integer}, k_integers);
    const std::optional<size_t> scan = find_production(symbols_.scan, {k_integer, k_integer}, k_integers);
    const auto zip_child = zip.has_value() ? children_.find(std::vector<size_t>{top_, zip.value(), 0}) : children_.end();
    const auto scan_child =
        scan.has_value() ? children_.find(std::vector<size_t>{top_, scan.value(), 0}) : children_.end();
    for (const term* first : lists[k_integers]) {
        for (const term* second : lists[k_integers]) {
            if (zip_child == children_.end())
                break;
            std::vector<values_t> envs;
            values_t wanted;
            bool aligned = true;
            for (size_t j = 0; j < top.envs.size() && aligned; ++j) {
                const std::optional<values_t> a = first->values[j] == nullptr ? std::nullopt
                                                                               : find_list(first->values[j]);
                const std::optional<values_t> b = second->values[j] == nullptr ? std::nullopt
                                                                                : find_list(second->values[j]);
                aligned = a.has_value() && b.has_value()
                          && std::min(a.value().size(), b.value().size()) == outputs[j].size();
                for (size_t i = 0; aligned && i < outputs[j].size(); ++i) {
                    values_t env = top.envs[j];
                    env.push_back(a.value()[i]);
                    env.push_back(b.value()[i]);
                    envs.push_back(env);
                    wanted.push_back(outputs[j][i]);
                }
            }
            if (!aligned || wanted.empty())
                continue;
            const expr* body = find_body(zip_child->second, k_integer, envs, wanted);
            if (body != nullptr)
                propose(make_functor_.make_functor(symbols_.zip_with, {make_lambda(2, body), first->e, second->e}));
        }
        if (scan_child == children_.end())
            continue;
        std::vector<values_t> envs;
        values_t wanted;
        bool aligned = true;
        for (size_t j = 0; j < top.envs.size() && aligned; ++j) {
            const std::optional<values_t> l = first->values[j] == nullptr ? std::nullopt
                                                                           : find_list(first->values[j]);
            aligned = l.has_value() && l.value().size() == outputs[j].size();
            for (size_t i = 1; aligned && i < outputs[j].size(); ++i) {
                values_t env = top.envs[j];
                env.push_back(outputs[j][i - 1]);
                env.push_back(l.value()[i]);
                envs.push_back(env);
                wanted.push_back(outputs[j][i]);
            }
        }
        if (!aligned || wanted.empty())
            continue;
        const expr* body = find_body(scan_child->second, k_integer, envs, wanted);
        if (body == nullptr)
            continue;
        // the base: a small integer term b with f(b, L[0]) = out[0] wherever the list is not empty
        for (size_t base_size = 1; base_size <= max_base_size_; ++base_size) {
            for (const term& base : top.bank[k_integer][base_size]) {
                const expr* candidate =
                    make_functor_.make_functor(symbols_.scan, {make_lambda(2, body), base.e, first->e});
                propose(candidate);
            }
        }
    }
}

PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE std::optional<size_t> PROG_ENUMERATIVE_HYPOTHESES::find_production(
    uint32_t symbol, const std::vector<uint8_t>& parameters, uint8_t over) const {
    for (size_t p = 0; p < productions_.size(); ++p) {
        const production& candidate = productions_[p];
        const bool same = candidate.symbol == symbol && candidate.args[0].lambda
                          && candidate.args[0].parameters == parameters && candidate.args.back().type == over;
        if (same)
            return p;
    }
    return std::nullopt;
}

// the smallest body of this type in the child context that gives every wanted output at its environment
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE const expr* PROG_ENUMERATIVE_HYPOTHESES::find_body(
    size_t child, uint8_t type, const std::vector<values_t>& envs, const values_t& outputs) {
    const context& c = contexts_[child];
    for (size_t size = 1; size <= c.max_size; ++size) {
        for (const term& body : c.bank[type][size]) {
            bool matches = true;
            for (size_t i = 0; i < envs.size() && matches; ++i) {
                const std::optional<const expr*> value = evaluate_with_.evaluate_with(body.e, envs[i]);
                matches = value.has_value() && value.value() == outputs[i];
            }
            if (matches)
                return body.e;
        }
    }
    return nullptr;
}

// a deduced program: kept when it reproduces every sample and is the smallest so far
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE void PROG_ENUMERATIVE_HYPOTHESES::propose(const expr* e) {
    const size_t nodes = count_nodes(e);
    if (deduced_.has_value() && deduced_size_ <= nodes)
        return;
    term candidate{.e = e, .values = {}};
    for (const values_t& env : contexts_[top_].envs) {
        const std::optional<const expr*> value = evaluate_with_.evaluate_with(e, env);
        if (!value.has_value())
            return;
        candidate.values.push_back(value.value());
    }
    if (!check(candidate))
        return;
    deduced_ = e;
    deduced_size_ = nodes;
}

// the elements of a `cons` chain ending in `nil`
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE std::optional<typename PROG_ENUMERATIVE_HYPOTHESES::values_t>
PROG_ENUMERATIVE_HYPOTHESES::find_list(const expr* list) const {
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

// the nodes of e, a `lams(k, b)` counting two
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE size_t PROG_ENUMERATIVE_HYPOTHESES::count_nodes(const expr* e) {
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

// whether a top term reproduces every sample: the probes' outputs (the first ones), then the rest by evaluation; in a
// hole, whether a frame filled with it meets the specification (the first that does is found)
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE bool PROG_ENUMERATIVE_HYPOTHESES::check(const term& candidate) {
    for (const expr* frame : frames_) {
        const expr* filled = plug(frame, candidate.e);
        bool meets = true;
        for (size_t i = 0; i < framed_.envs.size() && meets; ++i) {
            const std::optional<const expr*> value = evaluate_with_.evaluate_with(filled, framed_.envs[i]);
            meets = value.has_value() && match_relation_.matches(relation_, value.value(), framed_.outputs[i]);
        }
        if (!meets)
            continue;
        found_ = filled;
        return true;
    }
    if (!frames_.empty())
        return false;
    const size_t probes = candidate.values.size();
    for (size_t i = 0; i < probes; ++i)
        if (candidate.values[i] == nullptr
            || !match_relation_.matches(relation_, candidate.values[i], checked_outputs_[i]))
            return false;
    for (size_t i = 0; i < checked_envs_.size(); ++i) {
        const std::optional<const expr*> value = evaluate_with_.evaluate_with(candidate.e, checked_envs_[i]);
        if (!value.has_value() || !match_relation_.matches(relation_, value.value(), checked_outputs_[probes + i]))
            return false;
    }
    return true;
}

// the frame with its hole (every occurrence) replaced by the content, which is built in the hole's scope
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE const expr* PROG_ENUMERATIVE_HYPOTHESES::plug(const expr* frame,
                                                                                  const expr* content) {
    if (frame == marker_)
        return content;
    const expr::functor* f = std::get_if<expr::functor>(&frame->content);
    if (f == nullptr || f->args.empty())
        return frame;
    std::vector<const expr*> args;
    args.reserve(f->args.size());
    for (const expr* arg : f->args)
        args.push_back(plug(arg, content));
    return make_functor_.make_functor(f->id, args);
}

// `lam(b)`, `lam(lam(b))`, or `lams(k, b)` for three parameters or more
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE const expr* PROG_ENUMERATIVE_HYPOTHESES::make_lambda(size_t parameters,
                                                                                        const expr* body) {
    if (parameters >= 3)
        return make_functor_.make_functor(symbols_.lambdas,
                                          {make_literal_.make_literal(integer(static_cast<long>(parameters))), body});
    const expr* function = body;
    for (size_t k = 0; k < parameters; ++k)
        function = make_functor_.make_functor(symbols_.lambda, {function});
    return function;
}

// integers and lists of integers are always available; the other types only where a variable has them (or a
// variable from which they come: records from a list of records)
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE bool PROG_ENUMERATIVE_HYPOTHESES::available(const context& c, uint8_t type) const {
    if (type == k_integer || type == k_integers)
        return true;
    for (const uint8_t held : c.types)
        if (held == type || (type == k_record && held == k_records))
            return true;
    return false;
}

// the values a lambda's parameters range over: the data's integers, lists and their suffixes, subtrees, records
PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE void PROG_ENUMERATIVE_HYPOTHESES::collect_pools(const expr* value) {
    const uint8_t type = find_type(value);
    if (type == k_unknown)
        return;
    std::vector<const expr*>& pool = pools_[type];
    const size_t cap = type == k_integer ? 10 : 8;
    if (pool.size() < cap && std::find(pool.begin(), pool.end(), value) == pool.end())
        pool.push_back(value);
    const expr::functor* f = std::get_if<expr::functor>(&value->content);
    if (f == nullptr)
        return;
    for (const expr* arg : f->args)
        collect_pools(arg);
}

PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE bool PROG_ENUMERATIVE_HYPOTHESES::holds_hole(const expr* e) {
    if (std::holds_alternative<expr::var>(e->content))
        return true;
    for (const expr* arg : std::get<expr::functor>(e->content).args)
        if (holds_hole(arg))
            return true;
    return false;
}

#undef PROG_ENUMERATIVE_HYPOTHESES
#undef PROG_ENUMERATIVE_HYPOTHESES_TEMPLATE

#endif
