#ifndef ABS_INDICES_HPP
#define ABS_INDICES_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"

constexpr size_t k_abs_default_indices = 32;

// Interns de Bruijn indices as nullary symbols `#0`, `#1`, ... and reads them back. The first `count` are interned up
// front, so an index written elsewhere (by a parser, or as `#0` in a fitting variable) is recognized; larger ones are
// interned when made.
template<typename IInternSymbol, typename IMakeFunctor>
struct abs_indices {
    abs_indices(IInternSymbol& intern_symbol, IMakeFunctor& make_functor, size_t count);
    const expr* make_index(size_t index);
    std::optional<size_t> find_index(const expr* e) const;
private:
    IInternSymbol& intern_symbol_;
    IMakeFunctor& make_functor_;
    std::vector<uint32_t> symbols_;
    std::unordered_map<uint32_t, size_t> indices_;
};

#define ABS_INDICES_TEMPLATE template<typename IInternSymbol, typename IMakeFunctor>
#define ABS_INDICES          abs_indices<IInternSymbol, IMakeFunctor>

ABS_INDICES_TEMPLATE ABS_INDICES::abs_indices(IInternSymbol& intern_symbol, IMakeFunctor& make_functor, size_t count)
    : intern_symbol_(intern_symbol)
    , make_functor_(make_functor)
    , symbols_()
    , indices_() {
    for (size_t index = 0; index < count; ++index)
        make_index(index);
}

ABS_INDICES_TEMPLATE const expr* ABS_INDICES::make_index(size_t index) {
    while (symbols_.size() <= index) {
        const uint32_t symbol = intern_symbol_.intern_symbol("#" + std::to_string(symbols_.size()));
        indices_.emplace(symbol, symbols_.size());
        symbols_.push_back(symbol);
    }
    return make_functor_.make_functor(symbols_[index], {});
}

ABS_INDICES_TEMPLATE std::optional<size_t> ABS_INDICES::find_index(const expr* e) const {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || !f->args.empty())
        return std::nullopt;
    const auto found = indices_.find(f->id);
    if (found == indices_.end())
        return std::nullopt;
    return found->second;
}

#undef ABS_INDICES
#undef ABS_INDICES_TEMPLATE

#endif
