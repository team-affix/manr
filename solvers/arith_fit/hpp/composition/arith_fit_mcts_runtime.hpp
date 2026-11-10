#ifndef ARITH_FIT_MCTS_RUNTIME_HPP
#define ARITH_FIT_MCTS_RUNTIME_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "composition/arith_fit_mcts_manifest.hpp"
#include "value_objects/arith_sample.hpp"

// Pull loop: each `next()` runs one simulation, until `max_simulations` are spent.
struct arith_fit_mcts_runtime {
    arith_fit_mcts_runtime(const std::vector<arith_sample>& samples,
                           size_t probing_depth,
                           size_t max_simulations,
                           double exploration_constant,
                           uint32_t seed);
    bool next();
    std::string initial() const;
    std::string best() const;
    size_t best_cost() const;
    size_t simulations() const;
    std::vector<std::string> derivation() const;
private:
    std::string print(const expr* e) const;

    arith_fit_mcts_manifest manifest_;
};

#endif
