#ifndef PROG_FIT_RUNTIME_HPP
#define PROG_FIT_RUNTIME_HPP

#include <cstddef>
#include <string>
#include <vector>
#include "composition/prog_fit_manifest.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/poly_limits.hpp"
#include "value_objects/prog_sample.hpp"

// Pull loop: call `next()` until it returns false (or until satisfied), reading `best()` along the way. Results
// print as infix polynomials, with programs and any piecewise in prefix form.
struct prog_fit_runtime {
    prog_fit_runtime(const std::vector<prog_sample>& samples,
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
    prog_fit_manifest manifest_;
};

#endif
