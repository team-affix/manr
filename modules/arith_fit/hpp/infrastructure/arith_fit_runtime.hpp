#ifndef ARITH_FIT_RUNTIME_HPP
#define ARITH_FIT_RUNTIME_HPP

#include <cstddef>
#include <string>
#include <vector>
#include "infrastructure/arith_fit_manifest.hpp"
#include "value_objects/arith_sample.hpp"

// Pull loop: call `next()` until it returns false (or until satisfied), reading `best()` along the way.
struct arith_fit_runtime {
    arith_fit_runtime(const std::vector<arith_sample>& samples, size_t frontier_capacity);
    bool next();
    std::string initial() const;
    std::string best() const;
    size_t best_cost() const;
    size_t expansions() const;
    std::vector<std::string> derivation() const;
private:
    std::string print(const expr* e) const;

    arith_fit_manifest manifest_;
};

#endif
