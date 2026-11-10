#ifndef POLY_KERNEL_MANIFEST_HPP
#define POLY_KERNEL_MANIFEST_HPP

#include <cstddef>
#include "infrastructure/arith_literals.hpp"
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/poly_arithmetic.hpp"
#include "infrastructure/poly_divisibility_filter.hpp"
#include "infrastructure/poly_expander.hpp"
#include "infrastructure/poly_horner_renderer.hpp"
#include "infrastructure/poly_infix_printer.hpp"
#include "infrastructure/poly_linear_factor_finder.hpp"
#include "infrastructure/poly_parser.hpp"
#include "infrastructure/poly_renderer.hpp"
#include "infrastructure/poly_root_extractor.hpp"
#include "infrastructure/symbol_table.hpp"
#include "value_objects/poly_limits.hpp"
#include "value_objects/poly_symbols.hpp"

// Everything about polynomials that is not a rule or a search: the pool, symbols, literals, exact arithmetic, the
// normal form oracle, renderers, root and linear-factor finding, and infix text in and out. Every polynomial solver owns one.
struct poly_kernel_manifest {
    struct literals_t : arith_literals<symbol_table, expr_pool> {
        using arith_literals<symbol_table, expr_pool>::arith_literals;
    };
    struct expander_t : poly_expander<literals_t, poly_arithmetic, poly_arithmetic, poly_arithmetic, poly_arithmetic> {
        using poly_expander<literals_t, poly_arithmetic, poly_arithmetic, poly_arithmetic, poly_arithmetic>::poly_expander;
    };
    struct renderer_t : poly_renderer<expr_pool, literals_t> {
        using poly_renderer<expr_pool, literals_t>::poly_renderer;
    };
    struct horner_t : poly_horner_renderer<expr_pool, literals_t, renderer_t> {
        using poly_horner_renderer<expr_pool, literals_t, renderer_t>::poly_horner_renderer;
    };
    struct roots_t
        : poly_root_extractor<poly_arithmetic, poly_arithmetic, poly_arithmetic, poly_arithmetic, poly_arithmetic> {
        using poly_root_extractor<poly_arithmetic, poly_arithmetic, poly_arithmetic, poly_arithmetic,
                                  poly_arithmetic>::poly_root_extractor;
    };
    struct linear_factors_t : poly_linear_factor_finder<poly_arithmetic> {
        using poly_linear_factor_finder<poly_arithmetic>::poly_linear_factor_finder;
    };
    struct parser_t : poly_parser<symbol_table, expr_pool, expr_pool, literals_t, literals_t> {
        using poly_parser<symbol_table, expr_pool, expr_pool, literals_t, literals_t>::poly_parser;
    };
    struct printer_t : poly_infix_printer<symbol_table> {
        using poly_infix_printer<symbol_table>::poly_infix_printer;
    };

    poly_kernel_manifest(const poly_limits& limits);

    poly_limits limits_;
    symbol_table symbols_;
    expr_pool exprs_;
    expr_size sizes_;
    poly_symbols poly_symbols_;
    literals_t literals_;
    poly_arithmetic arithmetic_;
    linear_factors_t linear_factors_;
    poly_divisibility_filter divisibility_;
    expander_t expander_;
    renderer_t renderer_;
    horner_t horner_;
    roots_t roots_;
    parser_t parser_;
    printer_t printer_;
};

#endif
