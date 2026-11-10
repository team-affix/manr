#ifndef ARITH_KERNEL_MANIFEST_HPP
#define ARITH_KERNEL_MANIFEST_HPP

#include "infrastructure/arith_literals.hpp"
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_printer.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/symbol_table.hpp"
#include "value_objects/arith_symbols.hpp"

// Everything about integer arithmetic that is not a rule or a search: the pool, symbols, literals and a printer.
// Every arithmetic solver owns one.
struct arith_kernel_manifest {
    struct literals_t : arith_literals<symbol_table, expr_pool> {
        using arith_literals<symbol_table, expr_pool>::arith_literals;
    };
    struct printer_t : expr_printer<symbol_table> {
        using expr_printer<symbol_table>::expr_printer;
    };

    arith_kernel_manifest();

    symbol_table symbols_;
    expr_pool exprs_;
    expr_size sizes_;
    arith_symbols arith_symbols_;
    literals_t literals_;
    printer_t printer_;
};

#endif
