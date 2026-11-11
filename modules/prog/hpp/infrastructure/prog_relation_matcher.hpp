#ifndef PROG_RELATION_MATCHER_HPP
#define PROG_RELATION_MATCHER_HPP

#include <algorithm>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/prog_spec.hpp"
#include "value_objects/prog_symbols.hpp"

// Whether a value meets a wanted output under a relation (`prog_spec`): equal; a permutation of its elements; the
// same set of elements; nonzero exactly where the wanted is (a predicate); holding the wanted as a subsequence.
// Values are interned, so elements compare by pointer.
template<typename IFindLiteralValue>
struct prog_relation_matcher {
    prog_relation_matcher(IFindLiteralValue& find_literal_value, prog_symbols symbols);
    bool matches(uint8_t relation, const expr* value, const expr* wanted) const;
private:
    std::optional<std::vector<const expr*>> find_elements(const expr* list) const;

    IFindLiteralValue& find_literal_value_;
    prog_symbols symbols_;
};

#define PROG_RELATION_MATCHER_TEMPLATE template<typename IFindLiteralValue>
#define PROG_RELATION_MATCHER          prog_relation_matcher<IFindLiteralValue>

PROG_RELATION_MATCHER_TEMPLATE PROG_RELATION_MATCHER::prog_relation_matcher(IFindLiteralValue& find_literal_value,
                                                                            prog_symbols symbols)
    : find_literal_value_(find_literal_value)
    , symbols_(symbols) {}

PROG_RELATION_MATCHER_TEMPLATE bool PROG_RELATION_MATCHER::matches(uint8_t relation, const expr* value,
                                                                   const expr* wanted) const {
    if (relation == k_prog_relation_equal)
        return value == wanted;
    if (relation == k_prog_relation_truth) {
        const std::optional<integer> got = find_literal_value_.find_value(value);
        const std::optional<integer> truth = find_literal_value_.find_value(wanted);
        return got.has_value() && truth.has_value() && (got.value() != 0) == (truth.value() != 0);
    }
    const std::optional<std::vector<const expr*>> got = find_elements(value);
    const std::optional<std::vector<const expr*>> want = find_elements(wanted);
    if (!got.has_value() || !want.has_value())
        return false;
    std::vector<const expr*> left = got.value();
    std::vector<const expr*> right = want.value();
    if (relation == k_prog_relation_supersequence) {
        size_t matched = 0;
        for (const expr* element : left)
            if (matched < right.size() && right[matched] == element)
                ++matched;
        return matched == right.size();
    }
    std::sort(left.begin(), left.end());
    std::sort(right.begin(), right.end());
    if (relation == k_prog_relation_set) {
        left.erase(std::unique(left.begin(), left.end()), left.end());
        right.erase(std::unique(right.begin(), right.end()), right.end());
    }
    return left == right;
}

// the elements of a `cons` chain ending in `nil`
PROG_RELATION_MATCHER_TEMPLATE std::optional<std::vector<const expr*>> PROG_RELATION_MATCHER::find_elements(
    const expr* list) const {
    std::vector<const expr*> elements;
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

#undef PROG_RELATION_MATCHER
#undef PROG_RELATION_MATCHER_TEMPLATE

#endif
