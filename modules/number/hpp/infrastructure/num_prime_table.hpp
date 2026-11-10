#ifndef NUM_PRIME_TABLE_HPP
#define NUM_PRIME_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
#include "value_objects/integer.hpp"

constexpr uint32_t k_num_default_max_value = 10000000;

// The primes up to `max_value`, by a sieve grown on demand (doubling, so a run of growing queries costs linear time
// overall). No answer past `max_value`: the n-th prime when it is larger, or the primality of a larger value.
struct num_prime_table {
    num_prime_table(uint32_t max_value = k_num_default_max_value);
    std::optional<integer> find_nth_prime(const integer& index);
    std::optional<bool> find_primality(const integer& value);
private:
    void grow(uint32_t bound);

    uint32_t max_value_;
    uint32_t sieved_;
    std::vector<bool> composite_;
    std::vector<uint32_t> primes_;
};

#endif
