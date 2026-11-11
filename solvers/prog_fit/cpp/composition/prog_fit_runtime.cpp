#include "composition/prog_fit_runtime.hpp"
#include <sstream>

prog_fit_runtime::prog_fit_runtime(
    const std::vector<prog_sample>& samples,
    const poly_limits& limits,
    size_t frontier_capacity)
    : manifest_(samples, limits, frontier_capacity) {}

bool prog_fit_runtime::next() {
    return manifest_.search_.next();
}

std::string prog_fit_runtime::initial() const {
    return print(manifest_.rule_set_.initial_);
}

std::string prog_fit_runtime::best() const {
    return print(manifest_.search_.best());
}

size_t prog_fit_runtime::best_cost() const {
    return manifest_.search_.best_cost();
}

size_t prog_fit_runtime::rewrites() const {
    return manifest_.search_.rewrites();
}

std::vector<std::string> prog_fit_runtime::derivation() const {
    std::vector<std::string> steps;
    for (const expr* step : manifest_.search_.derivation(manifest_.search_.best()))
        steps.push_back(print(step));
    return steps;
}

std::string prog_fit_runtime::print(const expr* e) const {
    std::ostringstream os;
    manifest_.kernel_.printer_.print(os, e);
    return os.str();
}
