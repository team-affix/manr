#ifndef LIST_FIT_FIXTURE_HPP
#define LIST_FIT_FIXTURE_HPP

#include <cstddef>
#include <functional>
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
