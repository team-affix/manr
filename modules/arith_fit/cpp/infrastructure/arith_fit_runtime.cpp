#include "infrastructure/arith_fit_runtime.hpp"
#include <sstream>

arith_fit_runtime::arith_fit_runtime(const std::vector<arith_sample>& samples, size_t frontier_capacity)
    : manifest_(samples, frontier_capacity) {}

bool arith_fit_runtime::next() {
    return manifest_.search_.next();
}

std::string arith_fit_runtime::initial() const {
    return print(manifest_.initial_);
}

std::string arith_fit_runtime::best() const {
    return print(manifest_.search_.best());
}

size_t arith_fit_runtime::best_cost() const {
    return manifest_.search_.best_cost();
}

size_t arith_fit_runtime::rewrites() const {
    return manifest_.search_.rewrites();
}

std::vector<std::string> arith_fit_runtime::derivation() const {
    std::vector<std::string> steps;
    for (const expr* step : manifest_.search_.derivation(manifest_.search_.best()))
        steps.push_back(print(step));
    return steps;
}

std::string arith_fit_runtime::print(const expr* e) const {
    std::ostringstream os;
    manifest_.printer_.print(os, e);
    return os.str();
}
