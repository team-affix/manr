#include "infrastructure/pw_constant_hypotheses.hpp"
#include <unordered_set>

void pw_constant_hypotheses::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    std::unordered_set<const expr*> proposed;
    for (const pw_data_point& point : points) {
        const bool fresh = proposed.insert(point.output).second;
        if (!fresh)
            continue;
        out.push_back(point.output);
    }
}
