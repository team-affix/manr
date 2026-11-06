#ifndef POLY_INFIX_PRINTER_HPP
#define POLY_INFIX_PRINTER_HPP

#include <ostream>
#include <string>
#include <variant>
#include "value_objects/expr.hpp"
#include "value_objects/poly_symbols.hpp"

// Prints in the infix syntax `poly_parser` reads, with only the parentheses needed for the tree to parse back to
// itself: a right operand of the same precedence is parenthesized, so `a - (b - c)` and `a + (b + c)` survive.
// Negative literals are parenthesized everywhere but the start.
template<typename IGetSymbolName>
struct poly_infix_printer {
    poly_infix_printer(IGetSymbolName& get_symbol_name, poly_symbols symbols);
    void print(std::ostream& os, const expr* e) const;
private:
    void print_at(std::ostream& os, const expr* e, int context, bool leading) const;

    IGetSymbolName& get_symbol_name_;
    poly_symbols symbols_;
};

#define POLY_INFIX_PRINTER_TEMPLATE template<typename IGetSymbolName>
#define POLY_INFIX_PRINTER          poly_infix_printer<IGetSymbolName>

POLY_INFIX_PRINTER_TEMPLATE POLY_INFIX_PRINTER::poly_infix_printer(IGetSymbolName& get_symbol_name, poly_symbols symbols)
    : get_symbol_name_(get_symbol_name)
    , symbols_(symbols) {}

POLY_INFIX_PRINTER_TEMPLATE void POLY_INFIX_PRINTER::print(std::ostream& os, const expr* e) const {
    print_at(os, e, 0, true);
}

// `context` is the least precedence that prints without parentheses: 1 sum, 2 product, 3 power base, 4 atom.
POLY_INFIX_PRINTER_TEMPLATE void POLY_INFIX_PRINTER::print_at(
    std::ostream& os, const expr* e, int context, bool leading) const {
    if (const expr::var* v = std::get_if<expr::var>(&e->content)) {
        os << '?' << v->index;
        return;
    }
    const expr::functor& f = std::get<expr::functor>(e->content);
    if (f.args.empty()) {
        const std::string& name = get_symbol_name_.name(f.id);
        const bool negative = name.size() > 1 && name[0] == '-';
        const bool wrap = negative && !(leading && context <= 2);
        os << (wrap ? "(" : "") << name << (wrap ? ")" : "");
        return;
    }
    const bool sum = f.id == symbols_.add || f.id == symbols_.sub;
    const int precedence = sum ? 1 : f.id == symbols_.mul ? 2 : 3;
    const bool wrap = precedence < context;
    const bool inner_leading = wrap || leading;
    if (wrap)
        os << '(';
    const char* op = f.id == symbols_.add ? " + " : f.id == symbols_.sub ? " - " : f.id == symbols_.mul ? " * " : "^";
    print_at(os, f.args[0], precedence == 3 ? 4 : precedence, inner_leading);
    os << op;
    print_at(os, f.args[1], precedence == 3 ? 4 : precedence + 1, false);
    if (wrap)
        os << ')';
}

#undef POLY_INFIX_PRINTER
#undef POLY_INFIX_PRINTER_TEMPLATE

#endif
