#ifndef SEQ_FIT_RUNTIME_HPP
#define SEQ_FIT_RUNTIME_HPP

#include <cstddef>
#include <string>
#include <vector>
#include "infrastructure/seq_fit_manifest.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/poly_fit_limits.hpp"
#include "value_objects/poly_limits.hpp"
#include "value_objects/poly_sample.hpp"

// Pull loop: call `next()` until it returns false (or until satisfied), reading `best()` along the way. Results
// print as infix polynomials, with recurrences, powers and any piecewise in prefix form.
struct seq_fit_runtime {
    seq_fit_runtime(const std::string& variable,
                     const std::vector<poly_sample>& samples,
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
    seq_fit_manifest manifest_;
};

#endif
