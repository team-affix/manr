#ifndef EXPR_PRINTER_HPP
#define EXPR_PRINTER_HPP

#include <ostream>
#include <variant>
#include "value_objects/expr.hpp"

// Prints `name(arg, ...)` for functors, the bare name for nullary functors, and `?index` for extension terms.
template<typename IGetSymbolName>
struct expr_printer {
    expr_printer(IGetSymbolName& get_symbol_name);
    void print(std::ostream& os, const expr* e) const;
private:
    IGetSymbolName& get_symbol_name_;
};

#define EXPR_PRINTER_TEMPLATE template<typename IGetSymbolName>
#define EXPR_PRINTER          expr_printer<IGetSymbolName>

EXPR_PRINTER_TEMPLATE EXPR_PRINTER::expr_printer(IGetSymbolName& get_symbol_name)
    : get_symbol_name_(get_symbol_name) {}

EXPR_PRINTER_TEMPLATE void EXPR_PRINTER::print(std::ostream& os, const expr* e) const {
    if (const expr::var* v = std::get_if<expr::var>(&e->content)) {
        os << '?' << v->index;
        return;
    }
    const expr::functor& f = std::get<expr::functor>(e->content);
    os << get_symbol_name_.name(f.id);
    if (f.args.empty())
        return;
    os << '(';
    for (size_t i = 0; i < f.args.size(); ++i) {
        if (i > 0)
            os << ", ";
        print(os, f.args[i]);
    }
    os << ')';
}

#undef EXPR_PRINTER
#undef EXPR_PRINTER_TEMPLATE

#endif
