#ifndef PROG_SAMPLE_HPP
#define PROG_SAMPLE_HPP

#include <string>

// One sample of a program, as text in `prog_value_parser`'s form: its input (a tuple `point(a, ..., z)` for several)
// and its output.
struct prog_sample {
    std::string input;
    std::string output;
    auto operator<=>(const prog_sample&) const = default;
};

#endif
