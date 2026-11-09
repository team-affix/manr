#ifndef POLY_PARSER_HPP
#define POLY_PARSER_HPP

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_symbols.hpp"

// Parses infix polynomial text: `+ - * ^`, parentheses, integer literals, identifiers (including de Bruijn indices,
// `#0`, `#1`, ..., which a polynomial treats as variables), unary minus. `+ - *` are
// left-associative; `^` takes a non-negative integer literal. Integer literals may have any number of digits. `-e` is a negative literal when e is a literal, and
// `-1 * e` otherwise. Malformed text throws std::invalid_argument.
template<typename IInternSymbol, typename IMakeFunctor, typename IMakeLiteral, typename IFindLiteralValue>
struct poly_parser {
    poly_parser(IInternSymbol& intern_symbol,
                IMakeFunctor& make_functor,
                IMakeLiteral& make_literal,
                IFindLiteralValue& find_literal_value,
                poly_symbols symbols);
    const expr* parse(const std::string& text);
private:
    const expr* parse_sum();
    const expr* parse_product();
    const expr* parse_unary();
    const expr* parse_power();
    const expr* parse_atom();
    integer parse_integer();
    bool try_take(char expected);

    IInternSymbol& intern_symbol_;
    IMakeFunctor& make_functor_;
    IMakeLiteral& make_literal_;
    IFindLiteralValue& find_literal_value_;
    poly_symbols symbols_;
    std::string text_;
    size_t position_;
};

#define POLY_PARSER_TEMPLATE \
    template<typename IInternSymbol, typename IMakeFunctor, typename IMakeLiteral, typename IFindLiteralValue>
#define POLY_PARSER poly_parser<IInternSymbol, IMakeFunctor, IMakeLiteral, IFindLiteralValue>

POLY_PARSER_TEMPLATE POLY_PARSER::poly_parser(
    IInternSymbol& intern_symbol,
    IMakeFunctor& make_functor,
    IMakeLiteral& make_literal,
    IFindLiteralValue& find_literal_value,
    poly_symbols symbols)
    : intern_symbol_(intern_symbol)
    , make_functor_(make_functor)
    , make_literal_(make_literal)
    , find_literal_value_(find_literal_value)
    , symbols_(symbols)
    , text_()
    , position_(0) {}

POLY_PARSER_TEMPLATE const expr* POLY_PARSER::parse(const std::string& text) {
    text_ = text;
    position_ = 0;
    const expr* parsed = parse_sum();
    if (try_take('\0') || position_ != text_.size())
        throw std::invalid_argument("unexpected text at " + std::to_string(position_) + " in: " + text_);
    return parsed;
}

POLY_PARSER_TEMPLATE const expr* POLY_PARSER::parse_sum() {
    const expr* sum = parse_product();
    while (true) {
        if (try_take('+')) { sum = make_functor_.make_functor(symbols_.add, {sum, parse_product()}); continue; }
        if (try_take('-')) { sum = make_functor_.make_functor(symbols_.sub, {sum, parse_product()}); continue; }
        return sum;
    }
}

POLY_PARSER_TEMPLATE const expr* POLY_PARSER::parse_product() {
    const expr* product = parse_unary();
    while (try_take('*'))
        product = make_functor_.make_functor(symbols_.mul, {product, parse_unary()});
    return product;
}

POLY_PARSER_TEMPLATE const expr* POLY_PARSER::parse_unary() {
    if (!try_take('-'))
        return parse_power();
    const expr* operand = parse_unary();
    const std::optional<integer> value = find_literal_value_.find_value(operand);
    if (value.has_value())
        return make_literal_.make_literal(integer(-value.value()));
    return make_functor_.make_functor(symbols_.mul, {make_literal_.make_literal(-1), operand});
}

POLY_PARSER_TEMPLATE const expr* POLY_PARSER::parse_power() {
    const expr* base = parse_atom();
    if (!try_take('^'))
        return base;
    return make_functor_.make_functor(symbols_.pow, {base, make_literal_.make_literal(parse_integer())});
}

POLY_PARSER_TEMPLATE const expr* POLY_PARSER::parse_atom() {
    if (try_take('(')) {
        const expr* inner = parse_sum();
        if (!try_take(')'))
            throw std::invalid_argument("expected ) at " + std::to_string(position_) + " in: " + text_);
        return inner;
    }
    if (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_])))
        return make_literal_.make_literal(parse_integer());
    const size_t begin = position_;
    const bool index = position_ < text_.size() && text_[position_] == '#';
    position_ += index ? 1 : 0;
    while (position_ < text_.size() && (std::isalnum(static_cast<unsigned char>(text_[position_])) || text_[position_] == '_'))
        ++position_;
    if (position_ == begin + (index ? 1 : 0))
        throw std::invalid_argument("expected a term at " + std::to_string(position_) + " in: " + text_);
    return make_functor_.make_functor(intern_symbol_.intern_symbol(text_.substr(begin, position_ - begin)), {});
}

POLY_PARSER_TEMPLATE integer POLY_PARSER::parse_integer() {
    try_take('\0');
    const size_t begin = position_;
    while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_])))
        ++position_;
    if (position_ == begin)
        throw std::invalid_argument("expected an integer at " + std::to_string(position_) + " in: " + text_);
    return integer(text_.substr(begin, position_ - begin));
}

// Skips whitespace, then consumes `expected` if it is next. '\0' only skips whitespace.
POLY_PARSER_TEMPLATE bool POLY_PARSER::try_take(char expected) {
    while (position_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[position_])))
        ++position_;
    if (expected == '\0' || position_ == text_.size() || text_[position_] != expected)
        return false;
    ++position_;
    return true;
}

#undef POLY_PARSER
#undef POLY_PARSER_TEMPLATE

#endif
