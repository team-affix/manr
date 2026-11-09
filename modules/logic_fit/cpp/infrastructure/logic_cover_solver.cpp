#include "infrastructure/logic_cover_solver.hpp"
#include <bit>
#include <iterator>
#include <utility>

logic_cover_solver::logic_cover_solver(size_t max_width)
    : max_width_(max_width) {}

std::vector<logic_implicant> logic_cover_solver::find_cover(const logic_truth_table& table, bool output) const {
    std::vector<logic_implicant> cover;
    if (table.width > max_width_)
        return cover;
    std::set<uint64_t> uncovered;
    for (const auto& [input, value] : table.rows)
        if (value == output)
            uncovered.insert(input);
    if (uncovered.empty())
        return cover;
    const std::set<logic_implicant> primes = find_primes(table, output);
    // essential primes: the only prime covering some row
    for (const uint64_t row : std::set<uint64_t>(uncovered)) {
        const logic_implicant* only = nullptr;
        size_t covering = 0;
        for (const logic_implicant& prime : primes) {
            if ((row & prime.mask) != prime.bits)
                continue;
            ++covering;
            only = &prime;
        }
        const bool essential = covering == 1;
        if (!essential || !uncovered.contains(row))
            continue;
        cover.push_back(*only);
        for (auto other = uncovered.begin(); other != uncovered.end();)
            other = (*other & only->mask) == only->bits ? uncovered.erase(other) : std::next(other);
    }
    while (!uncovered.empty()) {
        const logic_implicant* best = nullptr;
        size_t best_count = 0;
        for (const logic_implicant& prime : primes) {
            size_t count = 0;
            for (const uint64_t row : uncovered)
                count += (row & prime.mask) == prime.bits ? 1 : 0;
            const bool better = count > best_count
                                || (count == best_count && count > 0
                                    && std::popcount(prime.mask) < std::popcount(best->mask));
            if (!better)
                continue;
            best = &prime;
            best_count = count;
        }
        cover.push_back(*best);
        for (auto row = uncovered.begin(); row != uncovered.end();)
            row = (*row & best->mask) == best->bits ? uncovered.erase(row) : std::next(row);
    }
    return cover;
}

// Merges implicants that differ in exactly one cared variable until nothing merges; the ones never merged are prime.
std::set<logic_implicant> logic_cover_solver::find_primes(const logic_truth_table& table, bool output) const {
    const uint64_t full = (uint64_t(1) << table.width) - 1;
    std::set<logic_implicant> current;
    for (uint64_t input = 0; input <= full; ++input) {
        const auto row = table.rows.find(input);
        const bool allowed = row == table.rows.end() || row->second == output;
        if (allowed)
            current.insert(logic_implicant{.mask = full, .bits = input});
    }
    std::set<logic_implicant> primes;
    while (!current.empty()) {
        std::set<logic_implicant> merged;
        std::set<logic_implicant> used;
        for (const logic_implicant& implicant : current)
            for (size_t position = 0; position < table.width; ++position) {
                const uint64_t bit = uint64_t(1) << position;
                if ((implicant.mask & bit) == 0)
                    continue;
                const logic_implicant partner{.mask = implicant.mask, .bits = implicant.bits ^ bit};
                if (!current.contains(partner))
                    continue;
                used.insert(implicant);
                merged.insert(logic_implicant{.mask = implicant.mask & ~bit, .bits = implicant.bits & ~bit});
            }
        for (const logic_implicant& implicant : current)
            if (!used.contains(implicant))
                primes.insert(implicant);
        current = std::move(merged);
    }
    return primes;
}
