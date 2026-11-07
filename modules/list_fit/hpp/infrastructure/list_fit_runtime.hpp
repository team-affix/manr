#ifndef LIST_FIT_RUNTIME_HPP
#define LIST_FIT_RUNTIME_HPP

#include <cstddef>
#include <string>
#include <vector>
#include "infrastructure/list_fit_manifest.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/list_sample.hpp"
#include "value_objects/poly_fit_limits.hpp"
#include "value_objects/poly_limits.hpp"

// Pull loop: call `next()` until it returns false (or until satisfied), reading `best()` along the way. Programs and
// lists print in prefix form, with their bodies as infix polynomials.
struct list_fit_runtime {
    list_fit_runtime(const std::vector<list_sample>& samples,
                     const poly_limits& limits,
                     const poly_fit_limits& fit_limits,
                     size_t frontier_capacity);
    bool next();
    std::string initial() const;
    std::string best() const;
    size_t best_cost() const;
    size_t rewrites() const;
    std::vector<std::string> derivation() const;
private:
    std::string print(const expr* e) const;
    list_fit_manifest manifest_;
};

#endif
