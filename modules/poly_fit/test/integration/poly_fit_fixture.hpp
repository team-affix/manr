#ifndef POLY_FIT_FIXTURE_HPP
#define POLY_FIT_FIXTURE_HPP

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>
#include "value_objects/integer.hpp"
#include "value_objects/poly_sample.hpp"

// Samples of `f` at every integer x in [low, high], with `outliers` (sample index, offset) added to the outputs.
inline std::vector<poly_sample> sample_line(int low, int high, const std::function<integer(integer)>& f,
                                            const std::vector<std::pair<size_t, int>>& outliers) {
    std::vector<poly_sample> samples;
    for (int x = low; x <= high; ++x)
        samples.push_back(poly_sample{.inputs = {x}, .output = f(x)});
    for (const auto& [index, offset] : outliers)
        samples[index].output += offset;
    return samples;
}

// Samples of `f` at every integer (x, y) in [low, high]^2.
inline std::vector<poly_sample> sample_grid(int low, int high, const std::function<integer(integer, integer)>& f) {
    std::vector<poly_sample> samples;
    for (int x = low; x <= high; ++x)
        for (int y = low; y <= high; ++y)
            samples.push_back(poly_sample{.inputs = {x, y}, .output = f(x, y)});
    return samples;
}

#endif
