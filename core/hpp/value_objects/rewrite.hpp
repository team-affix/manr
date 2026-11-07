#ifndef REWRITE_HPP
#define REWRITE_HPP

#include <functional>
#include "value_objects/expr.hpp"

// A detected rewrite, not yet performed: a thunk that captures where it applies and whatever detection worked out.
//
// Detection decides applicability with certainty: a rule appends a rewrite only when it is sure the rewrite applies
// and meets every condition the rule has (e.g. "the result is smaller"). Invoking the thunk does the remaining work
// that cannot change that answer, such as building the result, and always returns the whole resulting expression,
// never nullptr. Put as much work as possible in the thunk, but no uncertainty. See docs/rule-design.md.
using rewrite = std::function<const expr*()>;

// A rewrite whose result detection already built, for rules whose whole result is cheap to construct.
inline rewrite ready(const expr* result) {
    return [result]() { return result; };
}

#endif
