#ifndef LIST_FIT_FIXTURE_HPP
#define LIST_FIT_FIXTURE_HPP

#include <cstddef>
#include <functional>
#include <random>
#include <set>
#include <utility>
#include <vector>
#include "value_objects/integer.hpp"
#include "value_objects/list_sample.hpp"

// Every list of length at most `max_length` with elements in [low, high], shortest first, so every list's tail
// comes before it.
inline std::vector<std::vector<integer>> all_lists(int low, int high, size_t max_length) {
    std::vector<std::vector<integer>> lists{{}};
    size_t begin = 0;
    for (size_t length = 1; length <= max_length; ++length) {
        const size_t end = lists.size();
        for (size_t index = begin; index < end; ++index)
            for (int head = low; head <= high; ++head) {
                std::vector<integer> list{head};
                list.insert(list.end(), lists[index].begin(), lists[index].end());
                lists.push_back(list);
            }
        begin = end;
    }
    return lists;
}

// `count` lists with lengths in [min_length, max_length] and elements in [low, high], drawn from a seeded generator.
inline std::vector<std::vector<integer>> random_lists(size_t count, size_t min_length, size_t max_length, int low,
                                                      int high, unsigned seed) {
    std::mt19937 rng{seed};
    std::vector<std::vector<integer>> lists;
    for (size_t index = 0; index < count; ++index) {
        const size_t length = min_length + rng() % (max_length - min_length + 1);
        std::vector<integer> list;
        for (size_t position = 0; position < length; ++position)
            list.push_back(low + static_cast<int>(rng() % static_cast<unsigned>(high - low + 1)));
        lists.push_back(list);
    }
    return lists;
}

// The lists and every suffix of each (down to the empty list), each once, shortest first.
inline std::vector<std::vector<integer>> with_tails(const std::vector<std::vector<integer>>& lists) {
    std::set<std::pair<size_t, std::vector<integer>>> ordered;
    for (const std::vector<integer>& list : lists)
        for (size_t start = 0; start <= list.size(); ++start)
            ordered.emplace(list.size() - start, std::vector<integer>(list.begin() + start, list.end()));
    std::vector<std::vector<integer>> suffixes;
    for (const auto& [length, suffix] : ordered)
        suffixes.push_back(suffix);
    return suffixes;
}

// Samples of a list-to-integer function at every list.
inline std::vector<list_sample> sample_scalar(const std::vector<std::vector<integer>>& lists,
                                              const std::function<integer(const std::vector<integer>&)>& f) {
    std::vector<list_sample> samples;
    for (const std::vector<integer>& list : lists)
        samples.push_back(list_sample{.input = list, .output = {f(list)}, .output_is_list = false});
    return samples;
}

// Samples of a list-to-list function at every list.
inline std::vector<list_sample> sample_listed(
    const std::vector<std::vector<integer>>& lists,
    const std::function<std::vector<integer>(const std::vector<integer>&)>& f) {
    std::vector<list_sample> samples;
    for (const std::vector<integer>& list : lists)
        samples.push_back(list_sample{.input = list, .output = f(list), .output_is_list = true});
    return samples;
}

#endif
