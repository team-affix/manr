#ifndef PROG_INTERPRETER_HPP
#define PROG_INTERPRETER_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <set>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/prog_symbols.hpp"
#include "value_objects/prog_value.hpp"

constexpr size_t k_prog_default_max_steps = 200000;

// Runs a program of `prog_symbols` on data of any shape. Values are interned expressions (literals, lists, trees, any
// other constructor as a record, a tuple `point(...)`) or closures; indices `#k` read an environment, `#0` the
// innermost binding. `evaluate_at(model, input)` applies a function model to an input (`lams(k, b)` to `point(a1, ...,
// ak)`, the first `#(k-1)` and the last `#0`; `lam(b)` to any value, or to `point(a)`), and evaluates anything else as
// it is. Arithmetic on a value that is not a literal (one holding a hole `?k`) is kept symbolic (`add(?1, 1)`), so the
// piecewise hole matcher can solve it; a comparison or a structural step on one has no value. No value either for an
// unknown shape, a failed step (the head of an empty list), or a run past `max_steps`.
template<typename IFindIndex, typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
struct prog_interpreter {
    prog_interpreter(IFindIndex& find_index,
                     IFindLiteralValue& find_literal_value,
                     IMakeLiteral& make_literal,
                     IMakeFunctor& make_functor,
                     prog_symbols symbols,
                     size_t max_steps = k_prog_default_max_steps);
    std::optional<const expr*> evaluate_at(const expr* model, const expr* input);
    std::optional<bool> holds_at(const expr* condition, const expr* input);
    // the data value of e with these values for its indices, the innermost (`#0`) last
    std::optional<const expr*> evaluate_with(const expr* e, const std::vector<const expr*>& env);
private:
    using env_t = std::shared_ptr<const prog_frame>;
    using values_t = std::vector<const expr*>;

    env_t bind_input(size_t arity, const expr* input) const;
    std::optional<prog_value> evaluate(const expr* e, const env_t& env, size_t& steps);
    std::optional<const expr*> evaluate_data(const expr* e, const env_t& env, size_t& steps);
    std::optional<prog_value> evaluate_list_program(const expr::functor& f, const env_t& env, size_t& steps);
    std::optional<const expr*> evaluate_arithmetic(const expr::functor& f, const env_t& env, size_t& steps);
    std::optional<prog_value> apply(const prog_value& function, const values_t& args, size_t& steps);
    std::optional<const expr*> apply_data(const prog_value& function, const values_t& args, size_t& steps);
    std::optional<const expr*> fold_tree(const prog_value& function, const expr* base, const expr* tree, size_t& steps);
    std::optional<values_t> find_elements(const expr* list) const;
    const expr* make_list(const values_t& elements);
    const expr* make_truth(bool truth);
    static prog_value make_data(const expr* data);
    static env_t push(const env_t& env, const prog_value& value);
    static bool holds_hole(const expr* e);

    IFindIndex& find_index_;
    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    prog_symbols symbols_;
    size_t max_steps_;
};

#define PROG_INTERPRETER_TEMPLATE \
    template<typename IFindIndex, typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
#define PROG_INTERPRETER prog_interpreter<IFindIndex, IFindLiteralValue, IMakeLiteral, IMakeFunctor>

PROG_INTERPRETER_TEMPLATE PROG_INTERPRETER::prog_interpreter(IFindIndex& find_index,
                                                             IFindLiteralValue& find_literal_value,
                                                             IMakeLiteral& make_literal,
                                                             IMakeFunctor& make_functor,
                                                             prog_symbols symbols,
                                                             size_t max_steps)
    : find_index_(find_index)
    , find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , max_steps_(max_steps) {}

PROG_INTERPRETER_TEMPLATE std::optional<const expr*> PROG_INTERPRETER::evaluate_at(const expr* model,
                                                                                   const expr* input) {
    size_t steps = max_steps_;
    const std::optional<prog_value> value = evaluate(model, {}, steps);
    if (!value.has_value())
        return std::nullopt;
    if (value.value().data != nullptr)
        return value.value().data;
    const expr::functor* tuple = std::get_if<expr::functor>(&input->content);
    const bool spread = tuple != nullptr && tuple->id == symbols_.point && tuple->args.size() == value.value().arity;
    if (!spread && value.value().arity != 1)
        return std::nullopt;
    return apply_data(value.value(), spread ? tuple->args : values_t{input}, steps);
}

PROG_INTERPRETER_TEMPLATE std::optional<bool> PROG_INTERPRETER::holds_at(const expr* condition, const expr* input) {
    size_t steps = max_steps_;
    const expr::functor* f = std::get_if<expr::functor>(&input->content);
    const size_t arity = f != nullptr && f->id == symbols_.point ? f->args.size() : 1;
    const std::optional<const expr*> truth = evaluate_data(condition, bind_input(arity, input), steps);
    const std::optional<integer> value = truth.has_value() ? find_literal_value_.find_value(truth.value())
                                                           : std::optional<integer>{};
    if (!value.has_value())
        return std::nullopt;
    return value.value() != 0;
}

PROG_INTERPRETER_TEMPLATE std::optional<const expr*> PROG_INTERPRETER::evaluate_with(
    const expr* e, const std::vector<const expr*>& env) {
    size_t steps = max_steps_;
    env_t bound;
    for (const expr* value : env)
        bound = push(bound, make_data(value));
    return evaluate_data(e, bound, steps);
}

// the input as `arity` parameters: the arguments of a `point` of that many, or the input itself for one
PROG_INTERPRETER_TEMPLATE typename PROG_INTERPRETER::env_t PROG_INTERPRETER::bind_input(size_t arity,
                                                                                       const expr* input) const {
    const expr::functor* f = std::get_if<expr::functor>(&input->content);
    const bool tuple = f != nullptr && f->id == symbols_.point && f->args.size() == arity;
    env_t env;
    if (tuple) {
        for (const expr* arg : f->args)
            env = push(env, make_data(arg));
        return env;
    }
    if (arity == 1)
        env = push(env, make_data(input));
    return env;
}

PROG_INTERPRETER_TEMPLATE std::optional<prog_value> PROG_INTERPRETER::evaluate(const expr* e, const env_t& env,
                                                                               size_t& steps) {
    if (steps == 0)
        return std::nullopt;
    --steps;
    if (std::holds_alternative<expr::var>(e->content))
        return make_data(e);
    const expr::functor& f = std::get<expr::functor>(e->content);
    const std::optional<size_t> index = find_index_.find_index(e);
    if (index.has_value()) {
        env_t frame = env;
        for (size_t k = 0; k < index.value() && frame != nullptr; ++k)
            frame = frame->next;
        if (frame == nullptr)
            return std::nullopt;
        return frame->value;
    }
    if (f.args.empty())
        return make_data(e);
    if (f.id == symbols_.lambda && f.args.size() == 1)
        return prog_value{.data = nullptr, .body = f.args[0], .arity = 1, .captured = env};
    if (f.id == symbols_.lambdas && f.args.size() == 2) {
        const std::optional<integer> count = find_literal_value_.find_value(f.args[0]);
        if (!count.has_value() || count.value() < 1 || count.value() > 8)
            return std::nullopt;
        return prog_value{.data = nullptr, .body = f.args[1], .arity = static_cast<size_t>(count.value()),
                          .captured = env};
    }
    if (f.id == symbols_.application && f.args.size() >= 2) {
        const std::optional<prog_value> function = evaluate(f.args[0], env, steps);
        if (!function.has_value())
            return std::nullopt;
        values_t args;
        for (size_t i = 1; i < f.args.size(); ++i) {
            const std::optional<const expr*> arg = evaluate_data(f.args[i], env, steps);
            if (!arg.has_value())
                return std::nullopt;
            args.push_back(arg.value());
        }
        return apply(function.value(), args, steps);
    }
    if (f.id == symbols_.binding && f.args.size() == 2) {
        const std::optional<prog_value> bound = evaluate(f.args[0], env, steps);
        if (!bound.has_value())
            return std::nullopt;
        return evaluate(f.args[1], push(env, bound.value()), steps);
    }
    if (f.id == symbols_.piecewise) {
        for (size_t i = 0; i + 1 < f.args.size(); ++i) {
            const expr::functor* branch = std::get_if<expr::functor>(&f.args[i]->content);
            if (branch == nullptr || branch->id != symbols_.region || branch->args.size() != 2)
                return std::nullopt;
            const std::optional<const expr*> truth = evaluate_data(branch->args[0], env, steps);
            const std::optional<integer> value =
                truth.has_value() ? find_literal_value_.find_value(truth.value()) : std::optional<integer>{};
            if (!value.has_value())
                return std::nullopt;
            if (value.value() != 0)
                return evaluate(branch->args[1], env, steps);
        }
        return evaluate(f.args.back(), env, steps);
    }
    const bool program = f.id == symbols_.map || f.id == symbols_.filter || f.id == symbols_.fold
                         || f.id == symbols_.scan || f.id == symbols_.zip_with || f.id == symbols_.tree_fold;
    if (program)
        return evaluate_list_program(f, env, steps);
    const std::optional<const expr*> arithmetic = evaluate_arithmetic(f, env, steps);
    if (arithmetic.has_value())
        return make_data(arithmetic.value());
    return std::nullopt;
}

PROG_INTERPRETER_TEMPLATE std::optional<const expr*> PROG_INTERPRETER::evaluate_data(const expr* e, const env_t& env,
                                                                                    size_t& steps) {
    const std::optional<prog_value> value = evaluate(e, env, steps);
    if (!value.has_value() || value.value().data == nullptr)
        return std::nullopt;
    return value.value().data;
}

// map, filter, fold, scan, zipwith and tfold: a function and the data it runs over
PROG_INTERPRETER_TEMPLATE std::optional<prog_value> PROG_INTERPRETER::evaluate_list_program(const expr::functor& f,
                                                                                            const env_t& env,
                                                                                            size_t& steps) {
    const size_t arity = f.id == symbols_.map || f.id == symbols_.filter ? 2 : 3;
    if (f.args.size() != arity)
        return std::nullopt;
    const std::optional<prog_value> function = evaluate(f.args[0], env, steps);
    if (!function.has_value() || function.value().data != nullptr)
        return std::nullopt;
    values_t data;
    for (size_t i = 1; i < f.args.size(); ++i) {
        const std::optional<const expr*> value = evaluate_data(f.args[i], env, steps);
        if (!value.has_value())
            return std::nullopt;
        data.push_back(value.value());
    }
    if (f.id == symbols_.tree_fold) {
        const std::optional<const expr*> folded = fold_tree(function.value(), data[0], data[1], steps);
        return folded.has_value() ? std::optional<prog_value>(make_data(folded.value())) : std::nullopt;
    }
    const std::optional<values_t> elements = find_elements(data.back());
    if (!elements.has_value())
        return std::nullopt;
    values_t result;
    if (f.id == symbols_.zip_with) {
        const std::optional<values_t> others = find_elements(data[1]);
        const std::optional<values_t> firsts = find_elements(data[0]);
        if (!others.has_value() || !firsts.has_value())
            return std::nullopt;
        for (size_t i = 0; i < firsts.value().size() && i < others.value().size(); ++i) {
            const std::optional<const expr*> value = apply_data(function.value(), {firsts.value()[i], others.value()[i]},
                                                                steps);
            if (!value.has_value())
                return std::nullopt;
            result.push_back(value.value());
        }
        return make_data(make_list(result));
    }
    const expr* running = f.id == symbols_.fold || f.id == symbols_.scan ? data[0] : nullptr;
    const bool right = f.id == symbols_.fold;
    for (size_t position = 0; position < elements.value().size(); ++position) {
        const expr* element = elements.value()[right ? elements.value().size() - 1 - position : position];
        if (running != nullptr) {
            const std::optional<const expr*> value = apply_data(function.value(), {running, element}, steps);
            if (!value.has_value())
                return std::nullopt;
            running = value.value();
            if (f.id == symbols_.scan)
                result.push_back(running);
            continue;
        }
        const std::optional<const expr*> value = apply_data(function.value(), {element}, steps);
        if (!value.has_value())
            return std::nullopt;
        if (f.id == symbols_.map) {
            result.push_back(value.value());
            continue;
        }
        const std::optional<integer> kept = find_literal_value_.find_value(value.value());
        if (!kept.has_value())
            return std::nullopt;
        if (kept.value() != 0)
            result.push_back(element);
    }
    if (f.id == symbols_.fold)
        return make_data(running);
    return make_data(make_list(result));
}

// every first-order operation on data: arithmetic, comparisons, Boolean connectives, lists, records, graphs, and any
// other constructor as data
PROG_INTERPRETER_TEMPLATE std::optional<const expr*> PROG_INTERPRETER::evaluate_arithmetic(const expr::functor& f,
                                                                                          const env_t& env,
                                                                                          size_t& steps) {
    values_t args;
    for (const expr* arg : f.args) {
        const std::optional<const expr*> value = evaluate_data(arg, env, steps);
        if (!value.has_value())
            return std::nullopt;
        args.push_back(value.value());
    }
    std::vector<std::optional<integer>> numbers;
    bool literal = true;
    for (const expr* arg : args) {
        numbers.push_back(find_literal_value_.find_value(arg));
        literal = literal && numbers.back().has_value();
    }
    const bool binary = args.size() == 2;
    const bool symbolic = !literal && binary && (holds_hole(args[0]) || holds_hole(args[1]));
    if (binary && (f.id == symbols_.add || f.id == symbols_.sub || f.id == symbols_.mul)) {
        if (symbolic)
            return make_functor_.make_functor(f.id, args);
        if (!literal)
            return std::nullopt;
        const integer a = numbers[0].value();
        const integer b = numbers[1].value();
        return make_literal_.make_literal(f.id == symbols_.add ? integer(a + b) : f.id == symbols_.sub ? integer(a - b) : integer(a * b));
    }
    if (binary && f.id == symbols_.pow) {
        const bool small = numbers[1].has_value() && numbers[1].value() >= 0 && numbers[1].value() <= 64;
        if (small && holds_hole(args[0]))
            return make_functor_.make_functor(f.id, args);
        if (!literal || !small)
            return std::nullopt;
        return make_literal_.make_literal(
            boost::multiprecision::pow(numbers[0].value(), static_cast<unsigned>(numbers[1].value())));
    }
    const bool numeric = f.id == symbols_.mod || f.id == symbols_.max || f.id == symbols_.min || f.id == symbols_.less
                         || f.id == symbols_.less_equal || f.id == symbols_.conjunction
                         || f.id == symbols_.disjunction;
    if (binary && numeric) {
        if (!literal)
            return std::nullopt;
        const integer a = numbers[0].value();
        const integer b = numbers[1].value();
        if (f.id == symbols_.mod) {
            if (b <= 0)
                return std::nullopt;
            return make_literal_.make_literal(integer(((a % b) + b) % b));
        }
        if (f.id == symbols_.max || f.id == symbols_.min)
            return make_literal_.make_literal((f.id == symbols_.max) == (a < b) ? b : a);
        if (f.id == symbols_.less)
            return make_truth(a < b);
        if (f.id == symbols_.less_equal)
            return make_truth(a <= b);
        if (f.id == symbols_.conjunction)
            return make_truth(a != 0 && b != 0);
        return make_truth(a != 0 || b != 0);
    }
    if (f.id == symbols_.negation && args.size() == 1) {
        if (!literal)
            return std::nullopt;
        return make_truth(numbers[0].value() == 0);
    }
    if (binary && f.id == symbols_.equal) {
        if (holds_hole(args[0]) || holds_hole(args[1]))
            return std::nullopt;
        return make_truth(args[0] == args[1]);
    }
    if (binary && f.id == symbols_.cons)
        return make_functor_.make_functor(f.id, args);
    const bool unary_list = args.size() == 1
                            && (f.id == symbols_.head || f.id == symbols_.tail || f.id == symbols_.length
                                || f.id == symbols_.reverse || f.id == symbols_.sort || f.id == symbols_.unique);
    if (unary_list) {
        std::optional<values_t> elements = find_elements(args[0]);
        if (!elements.has_value())
            return std::nullopt;
        if (f.id == symbols_.length)
            return make_literal_.make_literal(integer(static_cast<long>(elements.value().size())));
        if ((f.id == symbols_.head || f.id == symbols_.tail) && elements.value().empty())
            return std::nullopt;
        if (f.id == symbols_.head)
            return elements.value().front();
        if (f.id == symbols_.tail)
            return make_list(values_t(elements.value().begin() + 1, elements.value().end()));
        if (f.id == symbols_.reverse) {
            std::reverse(elements.value().begin(), elements.value().end());
            return make_list(elements.value());
        }
        if (f.id == symbols_.unique) {
            values_t firsts;
            for (const expr* element : elements.value())
                if (std::find(firsts.begin(), firsts.end(), element) == firsts.end())
                    firsts.push_back(element);
            return make_list(firsts);
        }
        std::vector<integer> sorted;
        for (const expr* element : elements.value()) {
            const std::optional<integer> value = find_literal_value_.find_value(element);
            if (!value.has_value())
                return std::nullopt;
            sorted.push_back(value.value());
        }
        std::sort(sorted.begin(), sorted.end());
        values_t made;
        for (const integer& value : sorted)
            made.push_back(make_literal_.make_literal(value));
        return make_list(made);
    }
    if (binary && f.id == symbols_.append) {
        std::optional<values_t> first = find_elements(args[0]);
        const std::optional<values_t> second = find_elements(args[1]);
        if (!first.has_value() || !second.has_value())
            return std::nullopt;
        first.value().insert(first.value().end(), second.value().begin(), second.value().end());
        return make_list(first.value());
    }
    if (binary && f.id == symbols_.member) {
        const std::optional<values_t> elements = find_elements(args[1]);
        if (!elements.has_value() || holds_hole(args[0]))
            return std::nullopt;
        return make_truth(std::find(elements.value().begin(), elements.value().end(), args[0])
                          != elements.value().end());
    }
    if (binary && f.id == symbols_.field) {
        const expr::functor* record = std::get_if<expr::functor>(&args[1]->content);
        const bool readable = numbers[0].has_value() && record != nullptr && numbers[0].value() >= 0
                              && numbers[0].value() < static_cast<long>(record->args.size());
        if (!readable)
            return std::nullopt;
        return record->args[static_cast<size_t>(numbers[0].value())];
    }
    if (binary && f.id == symbols_.reachable) {
        const std::optional<values_t> edges = find_elements(args[0]);
        if (!edges.has_value())
            return std::nullopt;
        std::vector<std::pair<const expr*, const expr*>> pairs;
        for (const expr* edge : edges.value()) {
            const expr::functor* fields = std::get_if<expr::functor>(&edge->content);
            if (fields == nullptr || fields->args.size() != 2)
                return std::nullopt;
            pairs.emplace_back(fields->args[0], fields->args[1]);
        }
        std::set<const expr*> seen{args[1]};
        std::deque<const expr*> frontier{args[1]};
        while (!frontier.empty()) {
            const expr* at = frontier.front();
            frontier.pop_front();
            for (const auto& [from, to] : pairs) {
                if (from != at || seen.contains(to))
                    continue;
                seen.insert(to);
                frontier.push_back(to);
            }
        }
        std::vector<integer> reached;
        for (const expr* node : seen) {
            const std::optional<integer> value = find_literal_value_.find_value(node);
            if (!value.has_value())
                return std::nullopt;
            reached.push_back(value.value());
        }
        std::sort(reached.begin(), reached.end());
        values_t made;
        for (const integer& value : reached)
            made.push_back(make_literal_.make_literal(value));
        return make_list(made);
    }
    const bool vocabulary = f.id == symbols_.add || f.id == symbols_.sub || f.id == symbols_.mul || f.id == symbols_.pow
                            || numeric || f.id == symbols_.negation || f.id == symbols_.equal || f.id == symbols_.head
                            || f.id == symbols_.tail || f.id == symbols_.length || f.id == symbols_.reverse
                            || f.id == symbols_.sort || f.id == symbols_.unique || f.id == symbols_.append || f.id == symbols_.member
                            || f.id == symbols_.field || f.id == symbols_.reachable || f.id == symbols_.cons;
    if (vocabulary)
        return std::nullopt;
    // any other constructor is data: a tree node, a record, a tuple
    return make_functor_.make_functor(f.id, args);
}

PROG_INTERPRETER_TEMPLATE std::optional<prog_value> PROG_INTERPRETER::apply(const prog_value& function,
                                                                            const values_t& args, size_t& steps) {
    // curried: a closure of fewer parameters takes its share, and its value takes the rest
    prog_value current = function;
    size_t taken = 0;
    while (true) {
        if (current.data != nullptr || current.arity > args.size() - taken)
            return std::nullopt;
        env_t env = current.captured;
        for (size_t i = 0; i < current.arity; ++i)
            env = push(env, make_data(args[taken + i]));
        taken += current.arity;
        const std::optional<prog_value> value = evaluate(current.body, env, steps);
        if (!value.has_value() || taken == args.size())
            return value;
        current = value.value();
    }
}

PROG_INTERPRETER_TEMPLATE std::optional<const expr*> PROG_INTERPRETER::apply_data(const prog_value& function,
                                                                                 const values_t& args, size_t& steps) {
    const std::optional<prog_value> value = apply(function, args, steps);
    if (!value.has_value() || value.value().data == nullptr)
        return std::nullopt;
    return value.value().data;
}

// b at a leaf; at node(l, v, r), the function over the folded left, the value and the folded right
PROG_INTERPRETER_TEMPLATE std::optional<const expr*> PROG_INTERPRETER::fold_tree(const prog_value& function,
                                                                                const expr* base, const expr* tree,
                                                                                size_t& steps) {
    const expr::functor* f = std::get_if<expr::functor>(&tree->content);
    if (f == nullptr)
        return std::nullopt;
    if (f->id == symbols_.leaf && f->args.empty())
        return base;
    if (f->id != symbols_.node || f->args.size() != 3)
        return std::nullopt;
    const std::optional<const expr*> left = fold_tree(function, base, f->args[0], steps);
    if (!left.has_value())
        return std::nullopt;
    const std::optional<const expr*> right = fold_tree(function, base, f->args[2], steps);
    if (!right.has_value())
        return std::nullopt;
    return apply_data(function, {left.value(), f->args[1], right.value()}, steps);
}

PROG_INTERPRETER_TEMPLATE std::optional<typename PROG_INTERPRETER::values_t> PROG_INTERPRETER::find_elements(
    const expr* list) const {
    values_t elements;
    const expr* rest = list;
    while (true) {
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

PROG_INTERPRETER_TEMPLATE const expr* PROG_INTERPRETER::make_list(const values_t& elements) {
    const expr* list = make_functor_.make_functor(symbols_.nil, {});
    for (auto element = elements.rbegin(); element != elements.rend(); ++element)
        list = make_functor_.make_functor(symbols_.cons, {*element, list});
    return list;
}

PROG_INTERPRETER_TEMPLATE const expr* PROG_INTERPRETER::make_truth(bool truth) {
    return make_literal_.make_literal(integer(truth ? 1 : 0));
}

PROG_INTERPRETER_TEMPLATE prog_value PROG_INTERPRETER::make_data(const expr* data) {
    return prog_value{.data = data, .body = nullptr, .arity = 0, .captured = nullptr};
}

PROG_INTERPRETER_TEMPLATE typename PROG_INTERPRETER::env_t PROG_INTERPRETER::push(const env_t& env,
                                                                                const prog_value& value) {
    return std::make_shared<const prog_frame>(prog_frame{.value = value, .next = env});
}

PROG_INTERPRETER_TEMPLATE bool PROG_INTERPRETER::holds_hole(const expr* e) {
    if (std::holds_alternative<expr::var>(e->content))
        return true;
    for (const expr* arg : std::get<expr::functor>(e->content).args)
        if (holds_hole(arg))
            return true;
    return false;
}

#undef PROG_INTERPRETER
#undef PROG_INTERPRETER_TEMPLATE

#endif
