#include "composition/logic_fit_hypotheses_manifest.hpp"
#include <utility>

logic_fit_hypotheses_manifest::logic_fit_hypotheses_manifest(kernel_t& kernel,
                                                             logic_symbols symbols,
                                                             std::vector<uint32_t> variables,
                                                             uint32_t point_symbol)
    : logic_symbols_(symbols)
    , variables_(std::move(variables))
    , point_symbol_(point_symbol)
    , evaluator_(kernel.literals_, kernel.literals_, logic_symbols_, variables_)
    , reader_(kernel.literals_, variables_.size())
    , parity_solver_()
    , parities_(reader_, parity_solver_, kernel.exprs_, logic_symbols_, variables_)
    , cover_solver_(k_logic_default_max_cover_width)
    , covers_(reader_, cover_solver_, kernel.exprs_, logic_symbols_, variables_)
    , primitives_(reader_, kernel.exprs_, logic_symbols_, variables_)
    , leaves_(parities_, covers_, primitives_)
    , compositions_(leaves_, reader_, evaluator_, kernel.sizes_, kernel.literals_, kernel.exprs_, logic_symbols_,
                    point_symbol_, variables_, k_logic_condition_decompositions)
    , hypotheses_(leaves_, compositions_) {}
