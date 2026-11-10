#ifndef LOGIC_SIMPLIFIER_HPP
#define LOGIC_SIMPLIFIER_HPP

#include <cstddef>
#include <map>
#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/logic_symbols.hpp"
#include "value_objects/rewrite.hpp"

// Local rule: the laws of Boolean algebra, as rewrites that work at any width because they read structure, not truth
// tables. The operands below are any subformulas, not only inputs. Each rewrite makes the node smaller.
//
//   constants        and(a, 1) = a, and(a, 0) = 0, or(a, 0) = a, or(a, 1) = 1, xor(a, 0) = a, xor(a, 1) = not(a),
//                    xnor(a, 1) = a, xnor(a, 0) = not(a), not(0) = 1, not(1) = 0, imp(0, a) = imp(a, 1) = 1,
//                    imp(1, a) = a, imp(a, 0) = not(a), maj(a, b, 0) = and(a, b), maj(a, b, 1) = or(a, b)
//   involution       not(not(a)) = a
//   negated forms    not(and) = nand, not(or) = nor, not(xor) = xnor, and back; xor(not(a), b) = xnor(a, b),
//                    xor(not(a), not(b)) = xor(a, b), xnor(not(a), b) = xor(a, b); not(maj(a, b, c)) =
//                    maj(a', b', c') and not(mux(s, a, b)) = mux(s, a', b') when every operand x has a complement x'
//                    no larger (a negation's operand, the opposite connective, the other constant)
//   De Morgan        and(not(a), not(b)) = nor(a, b), or(not(a), not(b)) = nand(a, b), or(not(a), b) = imp(a, b)
//   idempotence      and(a, a) = or(a, a) = a; maj(a, a, b) = a
//   complement       and(a, not(a)) = 0, or(a, not(a)) = 1, xor(a, a) = 0, xnor(a, a) = 1, xor(a, not(a)) = 1,
//                    maj(a, not(a), b) = b
//   absorption       and(a, or(a, b)) = a, or(a, and(a, b)) = a
//   xor chains       the operands of a nest of `xor`s, where two equal operands cancel (parity at any width)
//   expansions       or(and(a, not(b)), and(not(a), b)) = xor(a, b), or(and(a, b), and(not(a), not(b))) = xnor(a, b),
//                    or(and(a, or(b, c)), and(b, c)) = maj(a, b, c), or(and(s, a), and(not(s), b)) = mux(s, a, b)
//   multiplexer      mux(s, a, a) = a, mux(not(s), a, b) = mux(s, b, a), mux(s, 1, 0) = s, mux(s, 0, 1) = not(s),
//                    mux(s, 1, b) = or(s, b), mux(s, a, 1) = imp(s, a), mux(s, a, 0) = and(s, a),
//                    mux(s, s, b) = or(s, b), mux(s, a, s) = and(s, a)
//
// Commutative connectives are matched in either order.
template<typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
struct logic_simplifier {
    logic_simplifier(IFindLiteralValue& find_literal_value,
                     IMakeLiteral& make_literal,
                     IMakeFunctor& make_functor,
                     logic_symbols symbols);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    const expr* find_simplification(const expr::functor& f);
    const expr* simplify_constant(const expr::functor& f);
    const expr* simplify_binary(uint32_t id, const expr* left, const expr* right);
    const expr* simplify_chain(const expr* e);
    void collect_chain(const expr* e, std::vector<const expr*>& operands) const;
    std::optional<bool> find_truth(const expr* e) const;
    const expr* find_negated(const expr* e) const;
    const expr* find_complement(const expr* e);
    bool is_functor(const expr* e, uint32_t id, size_t arity) const;
    const expr* make(uint32_t id, std::vector<const expr*> args);
    const expr* negate(const expr* e);

    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    logic_symbols symbols_;
};

#define LOGIC_SIMPLIFIER_TEMPLATE template<typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
#define LOGIC_SIMPLIFIER          logic_simplifier<IFindLiteralValue, IMakeLiteral, IMakeFunctor>

LOGIC_SIMPLIFIER_TEMPLATE LOGIC_SIMPLIFIER::logic_simplifier(IFindLiteralValue& find_literal_value,
                                                              IMakeLiteral& make_literal,
                                                              IMakeFunctor& make_functor,
                                                              logic_symbols symbols)
    : find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols) {}

LOGIC_SIMPLIFIER_TEMPLATE void LOGIC_SIMPLIFIER::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.empty())
        return;
    const expr* simpler = find_simplification(*f);
    if (simpler == nullptr || simpler == e)
        return;
    out.push_back([simpler]() { return simpler; });
}

LOGIC_SIMPLIFIER_TEMPLATE const expr* LOGIC_SIMPLIFIER::find_simplification(const expr::functor& f) {
    const expr* constant = simplify_constant(f);
    if (constant != nullptr)
        return constant;
    if (f.id == symbols_.negation && f.args.size() == 1) {
        const expr* inner = f.args[0];
        const expr* twice = find_negated(inner);
        if (twice != nullptr)
            return twice;
        const expr::functor* g = std::get_if<expr::functor>(&inner->content);
        if (g == nullptr)
            return nullptr;
        if (g->args.size() == 2)
            return find_complement(inner);
        const bool self_dual = g->args.size() == 3 && (g->id == symbols_.majority || g->id == symbols_.multiplexer);
        if (!self_dual)
            return nullptr;
        // the selector of a mux keeps its polarity; every other operand is complemented
        std::vector<const expr*> complements;
        for (size_t position = 0; position < 3; ++position) {
            const bool kept = g->id == symbols_.multiplexer && position == 0;
            const expr* complement = kept ? g->args[0] : find_complement(g->args[position]);
            if (complement == nullptr)
                return nullptr;
            complements.push_back(complement);
        }
        return make(g->id, complements);
    }
    if (f.id == symbols_.multiplexer && f.args.size() == 3) {
        const expr* select = f.args[0];
        const expr* then = f.args[1];
        const expr* otherwise = f.args[2];
        if (then == otherwise)
            return then;
        const expr* inner = find_negated(select);
        if (inner != nullptr)
            return make(symbols_.multiplexer, {inner, otherwise, then});
        const std::optional<bool> then_truth = find_truth(then);
        const std::optional<bool> otherwise_truth = find_truth(otherwise);
        if (then_truth.has_value() && otherwise_truth.has_value())
            return then_truth.value() ? select : negate(select);
        if (then_truth.has_value() && then_truth.value())
            return make(symbols_.disjunction, {select, otherwise});
        if (otherwise_truth.has_value())
            return otherwise_truth.value() ? make(symbols_.implication, {select, then})
                                           : make(symbols_.conjunction, {select, then});
        if (then == select)
            return make(symbols_.disjunction, {select, otherwise});
        if (otherwise == select)
            return make(symbols_.conjunction, {select, then});
        return nullptr;
    }
    if (f.id == symbols_.majority && f.args.size() == 3) {
        for (size_t first = 0; first < 3; ++first)
            for (size_t second = first + 1; second < 3; ++second) {
                const expr* other = f.args[3 - first - second];
                if (f.args[first] == f.args[second])
                    return f.args[first];
                const bool complementary = find_negated(f.args[first]) == f.args[second]
                                           || find_negated(f.args[second]) == f.args[first];
                if (complementary)
                    return other;
            }
        return nullptr;
    }
    if (f.args.size() != 2)
        return nullptr;
    if (f.id == symbols_.exclusion) {
        const expr* chain = simplify_chain(make(f.id, f.args));
        if (chain != nullptr)
            return chain;
    }
    const expr* simpler = simplify_binary(f.id, f.args[0], f.args[1]);
    const bool symmetric = f.id != symbols_.implication;
    if (simpler == nullptr && symmetric)
        simpler = simplify_binary(f.id, f.args[1], f.args[0]);
    return simpler;
}

// A connective with a constant operand, or every operand constant.
LOGIC_SIMPLIFIER_TEMPLATE const expr* LOGIC_SIMPLIFIER::simplify_constant(const expr::functor& f) {
    const expr* one = make_literal_.make_literal(integer(1));
    const expr* zero = make_literal_.make_literal(integer(0));
    if (f.id == symbols_.negation && f.args.size() == 1) {
        const std::optional<bool> truth = find_truth(f.args[0]);
        if (!truth.has_value())
            return nullptr;
        return truth.value() ? zero : one;
    }
    if (f.id == symbols_.majority && f.args.size() == 3) {
        for (size_t position = 0; position < 3; ++position) {
            const std::optional<bool> truth = find_truth(f.args[position]);
            if (!truth.has_value())
                continue;
            const expr* first = f.args[position == 0 ? 1 : 0];
            const expr* second = f.args[position == 2 ? 1 : 2];
            return make(truth.value() ? symbols_.disjunction : symbols_.conjunction, {first, second});
        }
        return nullptr;
    }
    if (f.args.size() != 2)
        return nullptr;
    for (size_t position = 0; position < 2; ++position) {
        const std::optional<bool> truth = find_truth(f.args[position]);
        if (!truth.has_value())
            continue;
        const expr* other = f.args[1 - position];
        const bool on = truth.value();
        if (f.id == symbols_.conjunction)
            return on ? other : zero;
        if (f.id == symbols_.disjunction)
            return on ? one : other;
        if (f.id == symbols_.exclusion)
            return on ? negate(other) : other;
        if (f.id == symbols_.equivalence)
            return on ? other : negate(other);
        if (f.id == symbols_.alternative_denial)
            return on ? negate(other) : one;
        if (f.id == symbols_.joint_denial)
            return on ? zero : negate(other);
        if (f.id != symbols_.implication)
            return nullptr;
        // imp(premise, conclusion)
        if (position == 0)
            return on ? other : one;
        return on ? one : negate(other);
    }
    return nullptr;
}

// The laws whose left operand is `left`, for a connective of two operands.
LOGIC_SIMPLIFIER_TEMPLATE const expr* LOGIC_SIMPLIFIER::simplify_binary(uint32_t id, const expr* left,
                                                                        const expr* right) {
    const expr* one = make_literal_.make_literal(integer(1));
    const expr* zero = make_literal_.make_literal(integer(0));
    const expr* left_negated = find_negated(left);
    const expr* right_negated = find_negated(right);
    const bool complementary = left_negated == right || right_negated == left;
    if (id == symbols_.conjunction) {
        if (left == right)
            return left;
        if (complementary)
            return zero;
        if (left_negated != nullptr && right_negated != nullptr)
            return make(symbols_.joint_denial, {left_negated, right_negated});
        const bool absorbs = is_functor(right, symbols_.disjunction, 2)
                             && (std::get<expr::functor>(right->content).args[0] == left
                                 || std::get<expr::functor>(right->content).args[1] == left);
        if (absorbs)
            return left;
        return nullptr;
    }
    if (id == symbols_.disjunction) {
        if (left == right)
            return left;
        if (complementary)
            return one;
        if (left_negated != nullptr && right_negated != nullptr)
            return make(symbols_.alternative_denial, {left_negated, right_negated});
        if (left_negated != nullptr)
            return make(symbols_.implication, {left_negated, right});
        const bool absorbs = is_functor(right, symbols_.conjunction, 2)
                             && (std::get<expr::functor>(right->content).args[0] == left
                                 || std::get<expr::functor>(right->content).args[1] == left);
        if (absorbs)
            return left;
        // expansions of xor, xnor and maj
        if (!is_functor(left, symbols_.conjunction, 2) || !is_functor(right, symbols_.conjunction, 2))
            return nullptr;
        const expr::functor& p = std::get<expr::functor>(left->content);
        const expr::functor& q = std::get<expr::functor>(right->content);
        for (size_t swap = 0; swap < 2; ++swap) {
            const expr* a = p.args[swap];
            const expr* not_b = p.args[1 - swap];
            const expr* b = find_negated(not_b);
            for (size_t order = 0; order < 2; ++order) {
                const expr* first = q.args[order];
                const expr* second = q.args[1 - order];
                if (b != nullptr && find_negated(first) == a && second == b)
                    return make(symbols_.exclusion, {a, b});
                const expr* first_negated = find_negated(first);
                const expr* second_negated = find_negated(second);
                if (first_negated == a && second_negated == not_b)
                    return make(symbols_.equivalence, {a, not_b});
                // or(and(s, x), and(not(s), y))
                if (first_negated == a)
                    return make(symbols_.multiplexer, {a, not_b, second});
                // or(and(a, or(b, c)), and(b, c))
                const bool spans = is_functor(not_b, symbols_.disjunction, 2);
                if (!spans)
                    continue;
                const expr::functor& r = std::get<expr::functor>(not_b->content);
                const bool same = (r.args[0] == first && r.args[1] == second)
                                  || (r.args[0] == second && r.args[1] == first);
                if (same)
                    return make(symbols_.majority, {a, first, second});
            }
        }
        return nullptr;
    }
    if (id == symbols_.exclusion || id == symbols_.equivalence) {
        const bool exclusion = id == symbols_.exclusion;
        if (left == right)
            return exclusion ? zero : one;
        if (complementary)
            return exclusion ? one : zero;
        if (left_negated != nullptr && right_negated != nullptr)
            return make(id, {left_negated, right_negated});
        if (left_negated != nullptr)
            return make(exclusion ? symbols_.equivalence : symbols_.exclusion, {left_negated, right});
        return nullptr;
    }
    if (id == symbols_.implication) {
        if (left == right)
            return one;
        if (left_negated == right)
            return right;
        return nullptr;
    }
    if (id == symbols_.alternative_denial || id == symbols_.joint_denial) {
        if (left == right)
            return negate(left);
        if (complementary)
            return id == symbols_.alternative_denial ? one : zero;
        return nullptr;
    }
    return nullptr;
}

// A nest of xors with two equal operands, those two cancelled; nothing when no two are equal.
LOGIC_SIMPLIFIER_TEMPLATE const expr* LOGIC_SIMPLIFIER::simplify_chain(const expr* e) {
    std::vector<const expr*> operands;
    collect_chain(e, operands);
    std::map<const expr*, size_t> counts;
    for (const expr* operand : operands)
        ++counts[operand];
    bool cancels = false;
    for (const auto& [operand, count] : counts)
        cancels = cancels || count >= 2;
    if (!cancels)
        return nullptr;
    const expr* chain = nullptr;
    for (const expr* operand : operands) {
        const size_t count = counts[operand];
        if (count % 2 == 0)
            continue;
        counts[operand] = 0;
        chain = chain == nullptr ? operand : make(symbols_.exclusion, {chain, operand});
    }
    return chain == nullptr ? make_literal_.make_literal(integer(0)) : chain;
}

LOGIC_SIMPLIFIER_TEMPLATE void LOGIC_SIMPLIFIER::collect_chain(const expr* e,
                                                               std::vector<const expr*>& operands) const {
    if (!is_functor(e, symbols_.exclusion, 2)) {
        operands.push_back(e);
        return;
    }
    const expr::functor& f = std::get<expr::functor>(e->content);
    collect_chain(f.args[0], operands);
    collect_chain(f.args[1], operands);
}

LOGIC_SIMPLIFIER_TEMPLATE std::optional<bool> LOGIC_SIMPLIFIER::find_truth(const expr* e) const {
    const std::optional<integer> value = find_literal_value_.find_value(e);
    if (!value.has_value() || (value.value() != 0 && value.value() != 1))
        return std::nullopt;
    return value.value() == 1;
}

// a, for e = not(a); else nullptr.
LOGIC_SIMPLIFIER_TEMPLATE const expr* LOGIC_SIMPLIFIER::find_negated(const expr* e) const {
    if (!is_functor(e, symbols_.negation, 1))
        return nullptr;
    return std::get<expr::functor>(e->content).args[0];
}

// A formula no larger than e meaning not(e): a for e = not(a), a binary connective's opposite (and and nand, or and
// nor, xor and xnor), the other constant; else nullptr.
LOGIC_SIMPLIFIER_TEMPLATE const expr* LOGIC_SIMPLIFIER::find_complement(const expr* e) {
    const expr* inner = find_negated(e);
    if (inner != nullptr)
        return inner;
    const std::optional<bool> truth = find_truth(e);
    if (truth.has_value())
        return make_literal_.make_literal(integer(truth.value() ? 0 : 1));
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.size() != 2)
        return nullptr;
    const std::map<uint32_t, uint32_t> opposite{{symbols_.conjunction, symbols_.alternative_denial},
                                                {symbols_.alternative_denial, symbols_.conjunction},
                                                {symbols_.disjunction, symbols_.joint_denial},
                                                {symbols_.joint_denial, symbols_.disjunction},
                                                {symbols_.exclusion, symbols_.equivalence},
                                                {symbols_.equivalence, symbols_.exclusion}};
    const auto flipped = opposite.find(f->id);
    if (flipped == opposite.end())
        return nullptr;
    return make(flipped->second, f->args);
}

LOGIC_SIMPLIFIER_TEMPLATE bool LOGIC_SIMPLIFIER::is_functor(const expr* e, uint32_t id, size_t arity) const {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    return f != nullptr && f->id == id && f->args.size() == arity;
}

LOGIC_SIMPLIFIER_TEMPLATE const expr* LOGIC_SIMPLIFIER::make(uint32_t id, std::vector<const expr*> args) {
    return make_functor_.make_functor(id, args);
}

// not(e), or a for e = not(a).
LOGIC_SIMPLIFIER_TEMPLATE const expr* LOGIC_SIMPLIFIER::negate(const expr* e) {
    const expr* inner = find_negated(e);
    return inner != nullptr ? inner : make(symbols_.negation, {e});
}

#undef LOGIC_SIMPLIFIER
#undef LOGIC_SIMPLIFIER_TEMPLATE

#endif
