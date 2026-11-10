#ifndef NUM_CONDITION_ATOMS_HPP
#define NUM_CONDITION_ATOMS_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/pw_data_point.hpp"

constexpr size_t k_num_default_max_threshold_atoms = 64;
constexpr int k_num_default_max_modulus = 6;

// The atomic predicates over one integer input `#0` that condition detectors combine (a list element, a polynomial
// input): thresholds `lt(v, #0)` and `lt(#0, v)` at every distinct value v of the data (when there are at most
// `max_thresholds` of them), residues `eq(mod(#0, m), r)` for every modulus m from 2 to `max_modulus`, and primality
// `is_prime(#0)`. The data points are `point(v)`; nothing for data with points of any other shape (several inputs),
// whose conditions these atoms over one input do not describe.
template<typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
struct num_condition_atoms {
    num_condition_atoms(IFindLiteralValue& find_literal_value,
                         IMakeLiteral& make_literal,
                         IMakeFunctor& make_functor,
                         uint32_t element_symbol,
                         uint32_t less_symbol,
                         uint32_t equal_symbol,
                         uint32_t modulo_symbol,
                         uint32_t primality_symbol,
                         size_t max_thresholds = k_num_default_max_threshold_atoms,
                         int max_modulus = k_num_default_max_modulus);
    void propose_atoms(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    uint32_t element_symbol_;
    uint32_t less_symbol_;
    uint32_t equal_symbol_;
    uint32_t modulo_symbol_;
    uint32_t primality_symbol_;
    size_t max_thresholds_;
    int max_modulus_;
};

#define NUM_CONDITION_ATOMS_TEMPLATE template<typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
#define NUM_CONDITION_ATOMS          num_condition_atoms<IFindLiteralValue, IMakeLiteral, IMakeFunctor>

NUM_CONDITION_ATOMS_TEMPLATE NUM_CONDITION_ATOMS::num_condition_atoms(IFindLiteralValue& find_literal_value,
                                                                         IMakeLiteral& make_literal,
                                                                         IMakeFunctor& make_functor,
                                                                         uint32_t element_symbol,
                                                                         uint32_t less_symbol,
                                                                         uint32_t equal_symbol,
                                                                         uint32_t modulo_symbol,
                                                                         uint32_t primality_symbol,
                                                                         size_t max_thresholds,
                                                                         int max_modulus)
    : find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , element_symbol_(element_symbol)
    , less_symbol_(less_symbol)
    , equal_symbol_(equal_symbol)
    , modulo_symbol_(modulo_symbol)
    , primality_symbol_(primality_symbol)
    , max_thresholds_(max_thresholds)
    , max_modulus_(max_modulus) {}

NUM_CONDITION_ATOMS_TEMPLATE void NUM_CONDITION_ATOMS::propose_atoms(const std::vector<pw_data_point>& points,
                                                                       std::vector<const expr*>& out) {
    std::set<integer> values;
    for (const pw_data_point& point : points) {
        const expr::functor* key = std::get_if<expr::functor>(&point.input->content);
        if (key == nullptr || key->args.size() != 1)
            return;
        const std::optional<integer> value = find_literal_value_.find_value(key->args[0]);
        if (!value.has_value())
            continue;
        values.insert(value.value());
    }
    const expr* element = make_functor_.make_functor(element_symbol_, {});
    out.push_back(make_functor_.make_functor(primality_symbol_, {element}));
    for (int modulus = 2; modulus <= max_modulus_; ++modulus) {
        const expr* residue = make_functor_.make_functor(modulo_symbol_, {element, make_literal_.make_literal(modulus)});
        for (int remainder = 0; remainder < modulus; ++remainder)
            out.push_back(make_functor_.make_functor(equal_symbol_, {residue, make_literal_.make_literal(remainder)}));
    }
    if (values.size() > max_thresholds_)
        return;
    for (const integer& value : values) {
        const expr* bound = make_literal_.make_literal(value);
        out.push_back(make_functor_.make_functor(less_symbol_, {bound, element}));
        out.push_back(make_functor_.make_functor(less_symbol_, {element, bound}));
    }
}

#undef NUM_CONDITION_ATOMS
#undef NUM_CONDITION_ATOMS_TEMPLATE

#endif
