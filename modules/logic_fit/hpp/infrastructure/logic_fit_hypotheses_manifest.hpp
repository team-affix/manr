#ifndef LOGIC_FIT_HYPOTHESES_MANIFEST_HPP
#define LOGIC_FIT_HYPOTHESES_MANIFEST_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/logic_bidecomposition_hypotheses.hpp"
#include "infrastructure/logic_cover_hypotheses.hpp"
#include "infrastructure/logic_cover_solver.hpp"
#include "infrastructure/logic_evaluator.hpp"
#include "infrastructure/logic_parity_hypotheses.hpp"
#include "infrastructure/logic_parity_solver.hpp"
#include "infrastructure/logic_primitive_hypotheses.hpp"
#include "infrastructure/logic_table_reader.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "infrastructure/pw_hypotheses_union.hpp"
#include "value_objects/logic_symbols.hpp"

constexpr size_t k_logic_condition_decompositions = 50;

// Boolean function fitting over the given input symbols, as a hypothesis source another domain can own: given truth
// table data (`point(b1, ..., bk) -> b` of 0s and 1s, unseen rows as don't cares), `hypotheses_` proposes parities,
// covers, primitives and compositions of any depth over the inputs. A domain that reduces part of its problem to a
// Boolean function (a condition over its own predicates) owns one, with placeholder inputs it then substitutes.
// Conditions are small, so compositions get a small budget (`k_logic_condition_decompositions`).
struct logic_fit_hypotheses_manifest {
    using kernel_t = poly_kernel_manifest;
    struct evaluator_t : logic_evaluator<kernel_t::literals_t, kernel_t::literals_t> {
        using logic_evaluator<kernel_t::literals_t, kernel_t::literals_t>::logic_evaluator;
    };
    struct reader_t : logic_table_reader<kernel_t::literals_t> {
        using logic_table_reader<kernel_t::literals_t>::logic_table_reader;
    };
    struct parities_t : logic_parity_hypotheses<reader_t, logic_parity_solver, expr_pool> {
        using logic_parity_hypotheses<reader_t, logic_parity_solver, expr_pool>::logic_parity_hypotheses;
    };
    struct covers_t : logic_cover_hypotheses<reader_t, logic_cover_solver, expr_pool> {
        using logic_cover_hypotheses<reader_t, logic_cover_solver, expr_pool>::logic_cover_hypotheses;
    };
    struct primitives_t : logic_primitive_hypotheses<reader_t, expr_pool> {
        using logic_primitive_hypotheses<reader_t, expr_pool>::logic_primitive_hypotheses;
    };
    struct leaves_t : pw_hypotheses_union<parities_t, covers_t, primitives_t> {
        using pw_hypotheses_union<parities_t, covers_t, primitives_t>::pw_hypotheses_union;
    };
    struct compositions_t : logic_bidecomposition_hypotheses<leaves_t, reader_t, evaluator_t, expr_size,
                                                             kernel_t::literals_t, expr_pool> {
        using logic_bidecomposition_hypotheses<leaves_t, reader_t, evaluator_t, expr_size, kernel_t::literals_t,
                                               expr_pool>::logic_bidecomposition_hypotheses;
    };
    struct hypotheses_t : pw_hypotheses_union<leaves_t, compositions_t> {
        using pw_hypotheses_union<leaves_t, compositions_t>::pw_hypotheses_union;
    };

    logic_fit_hypotheses_manifest(kernel_t& kernel,
                                  logic_symbols symbols,
                                  std::vector<uint32_t> variables,
                                  uint32_t point_symbol);

    logic_symbols logic_symbols_;
    std::vector<uint32_t> variables_;
    uint32_t point_symbol_;
    evaluator_t evaluator_;
    reader_t reader_;
    logic_parity_solver parity_solver_;
    parities_t parities_;
    logic_cover_solver cover_solver_;
    covers_t covers_;
    primitives_t primitives_;
    leaves_t leaves_;
    compositions_t compositions_;
    hypotheses_t hypotheses_;
};

#endif
