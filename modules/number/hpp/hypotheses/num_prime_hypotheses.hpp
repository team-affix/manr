#ifndef NUM_PRIME_HYPOTHESES_HPP
#define NUM_PRIME_HYPOTHESES_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/num_symbols.hpp"
#include "value_objects/pw_data_point.hpp"

// A detector that compares integer data of one input (`point(n) -> v`) with the primes, as the reverse detector
// compares lists with their reversal. Data of 0s and 1s with at least one 1 is read as primality, `is_prime(#0)`;
// any other integer data as the n-th prime, `prime(#0)`. It proposes the reading when it explains more than half of
// the points (the rest stay as branches), so a data set unlike the primes is refuted after one pass. Nothing for
// points that are not one integer, or a value past the prime table's range.
template<typename IFindLiteralValue, typename IFindNthPrime, typename IFindPrimality, typename IMakeFunctor>
struct num_prime_hypotheses {
    num_prime_hypotheses(IFindLiteralValue& find_literal_value,
                         IFindNthPrime& find_nth_prime,
                         IFindPrimality& find_primality,
                         IMakeFunctor& make_functor,
                         num_symbols symbols,
                         uint32_t variable);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    IFindLiteralValue& find_literal_value_;
    IFindNthPrime& find_nth_prime_;
    IFindPrimality& find_primality_;
    IMakeFunctor& make_functor_;
    num_symbols symbols_;
    uint32_t variable_;
};

#define NUM_PRIME_HYPOTHESES_TEMPLATE \
    template<typename IFindLiteralValue, typename IFindNthPrime, typename IFindPrimality, typename IMakeFunctor>
#define NUM_PRIME_HYPOTHESES num_prime_hypotheses<IFindLiteralValue, IFindNthPrime, IFindPrimality, IMakeFunctor>

NUM_PRIME_HYPOTHESES_TEMPLATE NUM_PRIME_HYPOTHESES::num_prime_hypotheses(IFindLiteralValue& find_literal_value,
                                                                         IFindNthPrime& find_nth_prime,
                                                                         IFindPrimality& find_primality,
                                                                         IMakeFunctor& make_functor,
                                                                         num_symbols symbols,
                                                                         uint32_t variable)
    : find_literal_value_(find_literal_value)
    , find_nth_prime_(find_nth_prime)
    , find_primality_(find_primality)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , variable_(variable) {}

NUM_PRIME_HYPOTHESES_TEMPLATE void NUM_PRIME_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    std::vector<integer> inputs;
    std::vector<integer> outputs;
    inputs.reserve(points.size());
    outputs.reserve(points.size());
    bool truth_values = true;
    bool any_true = false;
    for (const pw_data_point& point : points) {
        const expr::functor* key = std::get_if<expr::functor>(&point.input->content);
        if (key == nullptr || key->args.size() != 1)
            return;
        const std::optional<integer> input = find_literal_value_.find_value(key->args[0]);
        const std::optional<integer> output = find_literal_value_.find_value(point.output);
        if (!input.has_value() || !output.has_value())
            continue;
        inputs.push_back(input.value());
        outputs.push_back(output.value());
        truth_values = truth_values && (output.value() == 0 || output.value() == 1);
        any_true = any_true || output.value() == 1;
    }
    if (inputs.empty())
        return;
    const bool primality = truth_values && any_true;
    size_t explained = 0;
    for (size_t index = 0; index < inputs.size(); ++index) {
        if (primality) {
            const std::optional<bool> prime = find_primality_.find_primality(inputs[index]);
            if (!prime.has_value())
                return;
            explained += (prime.value() ? 1 : 0) == outputs[index] ? 1 : 0;
            continue;
        }
        const std::optional<integer> prime = find_nth_prime_.find_nth_prime(inputs[index]);
        explained += prime.has_value() && prime.value() == outputs[index] ? 1 : 0;
    }
    const bool most = 2 * explained > inputs.size();
    if (!most)
        return;
    const expr* variable = make_functor_.make_functor(variable_, {});
    out.push_back(make_functor_.make_functor(primality ? symbols_.is_prime : symbols_.prime, {variable}));
}

#undef NUM_PRIME_HYPOTHESES
#undef NUM_PRIME_HYPOTHESES_TEMPLATE

#endif
