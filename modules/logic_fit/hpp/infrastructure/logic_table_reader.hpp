#ifndef LOGIC_TABLE_READER_HPP
#define LOGIC_TABLE_READER_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/logic_truth_table.hpp"
#include "value_objects/pw_data_point.hpp"

// Reads data points as a truth table: every input must be `point(v1, ..., vk)` of 0s and 1s, with k the table's
// `width`, and every output 0 or 1; otherwise there is no table. The first output seen for an input is kept.
template<typename IFindLiteralValue>
struct logic_table_reader {
    logic_table_reader(IFindLiteralValue& find_literal_value, size_t width);
    std::optional<logic_truth_table> find_table(const std::vector<pw_data_point>& points);
private:
    std::optional<bool> find_bit(const expr* e);

    IFindLiteralValue& find_literal_value_;
    size_t width_;
};

#define LOGIC_TABLE_READER_TEMPLATE template<typename IFindLiteralValue>
#define LOGIC_TABLE_READER          logic_table_reader<IFindLiteralValue>

LOGIC_TABLE_READER_TEMPLATE LOGIC_TABLE_READER::logic_table_reader(IFindLiteralValue& find_literal_value, size_t width)
    : find_literal_value_(find_literal_value)
    , width_(width) {}

LOGIC_TABLE_READER_TEMPLATE std::optional<logic_truth_table> LOGIC_TABLE_READER::find_table(
    const std::vector<pw_data_point>& points) {
    logic_truth_table table{.width = width_, .rows = {}};
    for (const pw_data_point& point : points) {
        const expr::functor* input = std::get_if<expr::functor>(&point.input->content);
        if (input == nullptr || input->args.size() != width_)
            return std::nullopt;
        uint64_t row = 0;
        for (size_t position = 0; position < width_; ++position) {
            const std::optional<bool> bit = find_bit(input->args[position]);
            if (!bit.has_value())
                return std::nullopt;
            row |= bit.value() ? uint64_t(1) << position : 0;
        }
        const std::optional<bool> output = find_bit(point.output);
        if (!output.has_value())
            return std::nullopt;
        table.rows.emplace(row, output.value());
    }
    return table;
}

LOGIC_TABLE_READER_TEMPLATE std::optional<bool> LOGIC_TABLE_READER::find_bit(const expr* e) {
    const std::optional<integer> value = find_literal_value_.find_value(e);
    if (!value.has_value() || (value.value() != 0 && value.value() != 1))
        return std::nullopt;
    return value.value() == 1;
}

#undef LOGIC_TABLE_READER
#undef LOGIC_TABLE_READER_TEMPLATE

#endif
