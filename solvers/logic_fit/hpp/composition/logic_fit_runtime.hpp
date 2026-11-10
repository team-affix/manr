#ifndef LOGIC_FIT_RUNTIME_HPP
#define LOGIC_FIT_RUNTIME_HPP

#include <cstddef>
#include <string>
#include <vector>
#include "composition/logic_fit_manifest.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/poly_limits.hpp"
#include "value_objects/poly_sample.hpp"

// Pull loop: call `next()` until it returns false (or until satisfied), reading `best()` along the way. Results
// print in prefix form.
struct logic_fit_runtime {
    logic_fit_runtime(size_t arity,
                      const std::vector<poly_sample>& samples,
                      const poly_limits& limits,
                      size_t frontier_capacity);
    bool next();
    std::string initial() const;
    std::string best() const;
    size_t best_cost() const;
    size_t rewrites() const;
    std::vector<std::string> derivation() const;
private:
    std::string print(const expr* e) const;
    logic_fit_manifest manifest_;
};

#endif
