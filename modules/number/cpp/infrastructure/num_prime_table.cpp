#include "infrastructure/num_prime_table.hpp"
#include <algorithm>

num_prime_table::num_prime_table(uint32_t max_value)
    : max_value_(max_value)
    , sieved_(1)
    , composite_(2, true)
    , primes_() {}

std::optional<integer> num_prime_table::find_nth_prime(const integer& index) {
    if (index < 0)
        return std::nullopt;
    while (primes_.size() <= index && sieved_ < max_value_)
        grow(std::min<uint64_t>(static_cast<uint64_t>(sieved_) * 2 + 64, max_value_));
    if (primes_.size() <= index)
        return std::nullopt;
    return integer(primes_[index.convert_to<size_t>()]);
}

std::optional<bool> num_prime_table::find_primality(const integer& value) {
    if (value < 2)
        return false;
    if (value > max_value_)
        return std::nullopt;
    const uint32_t number = value.convert_to<uint32_t>();
    if (number > sieved_)
        grow(std::min<uint64_t>(std::max<uint64_t>(number, static_cast<uint64_t>(sieved_) * 2), max_value_));
    return !composite_[number];
}

// Sieves every number up to `bound` again, and lists the primes in order.
void num_prime_table::grow(uint32_t bound) {
    composite_.assign(static_cast<size_t>(bound) + 1, false);
    composite_[0] = true;
    composite_[1] = true;
    for (uint64_t factor = 2; factor * factor <= bound; ++factor) {
        if (composite_[factor])
            continue;
        for (uint64_t multiple = factor * factor; multiple <= bound; multiple += factor)
            composite_[multiple] = true;
    }
    primes_.clear();
    for (uint32_t number = 2; number <= bound; ++number)
        if (!composite_[number])
            primes_.push_back(number);
    sieved_ = bound;
}
