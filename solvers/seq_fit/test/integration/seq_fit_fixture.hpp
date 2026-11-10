#ifndef SEQ_FIT_FIXTURE_HPP
#define SEQ_FIT_FIXTURE_HPP

#include <cstddef>
#include <vector>
#include "value_objects/integer.hpp"
#include "value_objects/poly_sample.hpp"

// The terms a(0) to a(last) of the recurrence of order bases.size() whose step is `step(i, p1, p2)` (p1 and p2 are
// a(i-1) and a(i-2), 0 before the start), sampled from index `first` on.
template<typename Step>
std::vector<poly_sample> recurrence_samples(const std::vector<integer>& bases, Step step, int first, int last) {
    std::vector<integer> terms;
    for (int index = 0; index <= last; ++index) {
        if (static_cast<size_t>(index) < bases.size()) {
            terms.push_back(bases[index]);
            continue;
        }
        const integer previous = terms[index - 1];
        const integer before = index >= 2 ? terms[index - 2] : integer(0);
        terms.push_back(integer(step(integer(index), previous, before)));
    }
    std::vector<poly_sample> samples;
    for (int index = first; index <= last; ++index)
        samples.push_back(poly_sample{.inputs = {index}, .output = terms[index]});
    return samples;
}

#endif
