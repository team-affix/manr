#ifndef PROG_VALUE_PARSER_HPP
#define PROG_VALUE_PARSER_HPP

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/prog_symbols.hpp"

// Parses data of any shape, and programs in prefix form: an integer literal (any number of digits, a leading `-`), a
// hole `?k`, a list `[a, b, ...]` (as `cons` and `nil`), and any term `name` or `name(a, b, ...)`: a tree (`leaf`,
// `node(l, v, r)`), a record (`edge(1, 2)`), a tuple of inputs (`point(xs, x)`), a de Bruijn index (`#0`), a program
// (`lam(fold(lam(lam(add(#0, #1))), 0, #0))`). Malformed text throws std::invalid_argument.
template<typename IMakeLiteral, typename IMakeFunctor, typename IInternSymbol, typename IMakeVar>
struct prog_value_parser {
    prog_value_parser(IMakeLiteral& make_literal,
                      IMakeFunctor& make_functor,
                      IInternSymbol& intern_symbol,
                      IMakeVar& make_var,
                      prog_symbols symbols);
    const expr* parse(const std::string& text);
private:
    const expr* parse_value(const std::string& text, size_t& at);
    std::vector<const expr*> parse_items(const std::string& text, size_t& at, char close);
    static void skip_spaces(const std::string& text, size_t& at);

    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    IInternSymbol& intern_symbol_;
    IMakeVar& make_var_;
    prog_symbols symbols_;
};

#define PROG_VALUE_PARSER_TEMPLATE \
    template<typename IMakeLiteral, typename IMakeFunctor, typename IInternSymbol, typename IMakeVar>
#define PROG_VALUE_PARSER prog_value_parser<IMakeLiteral, IMakeFunctor, IInternSymbol, IMakeVar>

PROG_VALUE_PARSER_TEMPLATE PROG_VALUE_PARSER::prog_value_parser(IMakeLiteral& make_literal,
                                                                IMakeFunctor& make_functor,
                                                                IInternSymbol& intern_symbol,
                                                                IMakeVar& make_var,
                                                                prog_symbols symbols)
    : make_literal_(make_literal)
    , make_functor_(make_functor)
    , intern_symbol_(intern_symbol)
    , make_var_(make_var)
    , symbols_(symbols) {}

PROG_VALUE_PARSER_TEMPLATE const expr* PROG_VALUE_PARSER::parse(const std::string& text) {
    size_t at = 0;
    const expr* value = parse_value(text, at);
    skip_spaces(text, at);
    if (at != text.size())
        throw std::invalid_argument("unexpected text at " + std::to_string(at) + " in: " + text);
    return value;
}

PROG_VALUE_PARSER_TEMPLATE const expr* PROG_VALUE_PARSER::parse_value(const std::string& text, size_t& at) {
    skip_spaces(text, at);
    if (at >= text.size())
        throw std::invalid_argument("expected a value in: " + text);
    if (text[at] == '[') {
        ++at;
        const std::vector<const expr*> items = parse_items(text, at, ']');
        const expr* list = make_functor_.make_functor(symbols_.nil, {});
        for (auto item = items.rbegin(); item != items.rend(); ++item)
            list = make_functor_.make_functor(symbols_.cons, {*item, list});
        return list;
    }
    if (text[at] == '?') {
        size_t end = at + 1;
        while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end])))
            ++end;
        if (end == at + 1)
            throw std::invalid_argument("expected a hole number in: " + text);
        const expr* hole = make_var_.make_var(static_cast<uint32_t>(std::stoul(text.substr(at + 1, end - at - 1))));
        at = end;
        return hole;
    }
    const bool negative = text[at] == '-';
    if (negative || std::isdigit(static_cast<unsigned char>(text[at]))) {
        size_t end = at + (negative ? 1 : 0);
        while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end])))
            ++end;
        if (end == at + (negative ? 1 : 0))
            throw std::invalid_argument("expected digits in: " + text);
        const integer value(text.substr(at, end - at));
        at = end;
        return make_literal_.make_literal(value);
    }
    size_t end = at;
    while (end < text.size()
           && (std::isalnum(static_cast<unsigned char>(text[end])) || text[end] == '_' || text[end] == '#'))
        ++end;
    if (end == at)
        throw std::invalid_argument("unexpected text at " + std::to_string(at) + " in: " + text);
    const uint32_t name = intern_symbol_.intern_symbol(text.substr(at, end - at));
    at = end;
    skip_spaces(text, at);
    if (at >= text.size() || text[at] != '(')
        return make_functor_.make_functor(name, {});
    ++at;
    return make_functor_.make_functor(name, parse_items(text, at, ')'));
}

// values separated by commas up to `close`, which is consumed
PROG_VALUE_PARSER_TEMPLATE std::vector<const expr*> PROG_VALUE_PARSER::parse_items(const std::string& text, size_t& at,
                                                                                  char close) {
    std::vector<const expr*> items;
    skip_spaces(text, at);
    if (at < text.size() && text[at] == close) {
        ++at;
        return items;
    }
    while (true) {
        items.push_back(parse_value(text, at));
        skip_spaces(text, at);
        if (at < text.size() && text[at] == ',') {
            ++at;
            continue;
        }
        if (at < text.size() && text[at] == close) {
            ++at;
            return items;
        }
        throw std::invalid_argument("expected ',' or '" + std::string(1, close) + "' in: " + text);
    }
}

PROG_VALUE_PARSER_TEMPLATE void PROG_VALUE_PARSER::skip_spaces(const std::string& text, size_t& at) {
    while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at])))
        ++at;
}

#undef PROG_VALUE_PARSER
#undef PROG_VALUE_PARSER_TEMPLATE

#endif
