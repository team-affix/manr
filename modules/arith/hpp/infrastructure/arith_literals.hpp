#ifndef ARITH_LITERALS_HPP
#define ARITH_LITERALS_HPP

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <variant>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"

// Interns integer literals, of any size, as nullary functors named by their decimal text, and reads them back.
template<typename IInternSymbol, typename IMakeFunctor>
struct arith_literals {
    arith_literals(IInternSymbol& intern_symbol, IMakeFunctor& make_functor);
    const expr* make_literal(const integer& value);
    std::optional<integer> find_value(const expr* e) const;
private:
    IInternSymbol& intern_symbol_;
    IMakeFunctor& make_functor_;
    std::unordered_map<uint32_t, integer> values_;
};

#define ARITH_LITERALS_TEMPLATE template<typename IInternSymbol, typename IMakeFunctor>
#define ARITH_LITERALS          arith_literals<IInternSymbol, IMakeFunctor>

ARITH_LITERALS_TEMPLATE ARITH_LITERALS::arith_literals(IInternSymbol& intern_symbol, IMakeFunctor& make_functor)
    : intern_symbol_(intern_symbol)
    , make_functor_(make_functor)
    , values_() {}

ARITH_LITERALS_TEMPLATE const expr* ARITH_LITERALS::make_literal(const integer& value) {
    const uint32_t id = intern_symbol_.intern_symbol(value.str());
    values_.emplace(id, value);
    return make_functor_.make_functor(id, {});
}

ARITH_LITERALS_TEMPLATE std::optional<integer> ARITH_LITERALS::find_value(const expr* e) const {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || !f->args.empty())
        return std::nullopt;
    const auto found = values_.find(f->id);
    if (found == values_.end())
        return std::nullopt;
    return found->second;
}

#undef ARITH_LITERALS
#undef ARITH_LITERALS_TEMPLATE

#endif
