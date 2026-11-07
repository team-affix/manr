#include "infrastructure/poly_fit_runtime.hpp"
#include <sstream>

poly_fit_runtime::poly_fit_runtime(
    const std::vector<std::string>& variables,
    const std::vector<poly_sample>& samples,
    const poly_limits& limits,
    const poly_fit_limits& fit_limits,
    size_t frontier_capacity)
    : manifest_(variables, samples, limits, fit_limits, frontier_capacity) {}

bool poly_fit_runtime::next() {
    return manifest_.search_.next();
}

std::string poly_fit_runtime::initial() const {
    return print(manifest_.rule_set_.initial_);
}

std::string poly_fit_runtime::best() const {
    return print(manifest_.search_.best());
}

size_t poly_fit_runtime::best_cost() const {
    return manifest_.search_.best_cost();
}

size_t poly_fit_runtime::rewrites() const {
    return manifest_.search_.rewrites();
}

std::vector<std::string> poly_fit_runtime::derivation() const {
    std::vector<std::string> steps;
    for (const expr* step : manifest_.search_.derivation(manifest_.search_.best()))
        steps.push_back(print(step));
    return steps;
}

std::string poly_fit_runtime::print(const expr* e) const {
    std::ostringstream os;
    manifest_.kernel_.printer_.print(os, e);
    return os.str();
}
