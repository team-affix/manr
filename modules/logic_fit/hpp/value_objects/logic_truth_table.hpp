#ifndef LOGIC_TRUTH_TABLE_HPP
#define LOGIC_TRUTH_TABLE_HPP

#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>

// The rows of a truth table that the data gives: each input as a bit set (bit i is the i-th variable) and its output.
// Inputs absent from `rows` are unknown.
struct logic_truth_table {
    size_t width;
    std::map<uint64_t, bool> rows;
    auto operator<=>(const logic_truth_table&) const = default;
};

#endif
