#include "infrastructure/logic_parity_solver.hpp"
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

std::optional<logic_parity> logic_parity_solver::solve(const logic_truth_table& table) const {
    if (table.width > 63)
        return std::nullopt;
    const uint64_t constant = uint64_t(1) << table.width;
    // each reduced row: its coefficients (bit width is the constant term), its right-hand side, its pivot bit
    std::vector<std::pair<uint64_t, bool>> reduced;
    std::vector<uint64_t> pivots;
    for (const auto& [input, output] : table.rows) {
        uint64_t row = input | constant;
        bool side = output;
        for (size_t member = 0; member < reduced.size(); ++member) {
            if ((row & pivots[member]) == 0)
                continue;
            row ^= reduced[member].first;
            side = side != reduced[member].second;
        }
        if (row == 0) {
            if (side)
                return std::nullopt;
            continue;
        }
        const uint64_t pivot = row & (~row + 1);
        for (size_t member = 0; member < reduced.size(); ++member) {
            if ((reduced[member].first & pivot) == 0)
                continue;
            reduced[member].first ^= row;
            reduced[member].second = reduced[member].second != side;
        }
        reduced.emplace_back(row, side);
        pivots.push_back(pivot);
    }
    logic_parity parity{.variables = 0, .inverted = false};
    for (size_t member = 0; member < reduced.size(); ++member) {
        if (!reduced[member].second)
            continue;
        if (pivots[member] == constant)
            parity.inverted = true;
        else
            parity.variables |= pivots[member];
    }
    return parity;
}
