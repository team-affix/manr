#ifndef ABS_FUNCTION_MAKER_HPP
#define ABS_FUNCTION_MAKER_HPP

#include <cstddef>
#include "value_objects/abs_symbols.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"

// Closes a body over `count` arguments with one explicit binder node: `lam(b)` for one, `lams(count, b)` for more.
// With k arguments the first is `#(k-1)` in the body and the last `#0`.
template<typename IMakeLiteral, typename IMakeFunctor>
struct abs_function_maker {
    abs_function_maker(IMakeLiteral& make_literal, IMakeFunctor& make_functor, abs_symbols symbols);
    const expr* make_function(size_t count, const expr* body);
private:
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    abs_symbols symbols_;
};

#define ABS_FUNCTION_MAKER_TEMPLATE template<typename IMakeLiteral, typename IMakeFunctor>
#define ABS_FUNCTION_MAKER          abs_function_maker<IMakeLiteral, IMakeFunctor>

ABS_FUNCTION_MAKER_TEMPLATE ABS_FUNCTION_MAKER::abs_function_maker(
    IMakeLiteral& make_literal, IMakeFunctor& make_functor, abs_symbols symbols)
    : make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols) {}

ABS_FUNCTION_MAKER_TEMPLATE const expr* ABS_FUNCTION_MAKER::make_function(size_t count, const expr* body) {
    if (count == 1)
        return make_functor_.make_functor(symbols_.lambda, {body});
    return make_functor_.make_functor(symbols_.lambdas, {make_literal_.make_literal(integer(count)), body});
}

#undef ABS_FUNCTION_MAKER
#undef ABS_FUNCTION_MAKER_TEMPLATE

#endif
