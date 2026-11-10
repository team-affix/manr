#ifndef PW_GUARDED_HYPOTHESES_HPP
#define PW_GUARDED_HYPOTHESES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/pw_data_point.hpp"
#include "value_objects/pw_symbols.hpp"

constexpr size_t k_pw_default_max_guards = 2;
constexpr size_t k_pw_default_max_models = 8;
constexpr size_t k_pw_default_min_guarded = 3;
constexpr size_t k_pw_default_max_guarded_proposals = 4;
constexpr size_t k_pw_default_max_peeled_atoms = 24;
constexpr size_t k_pw_default_max_atom_depth = 2;

// A detector that reads data as a guarded value, `pw(when(c, v), d)`: a default output d, and where the output is
// something else, a condition c and a value v, split outer-inner (`docs/decomposing-detection.md`). Assuming d, every
// point says whether c holds (its output is not d), which is condition data (`input -> 1` or `-> 0`) for the
// condition domain, and the points where it holds are data for the value domain. So the inner function of a sum that
// adds only the primes reads as `pw(when(is_prime(#0), #0), 0)`, and a count over any condition as
// `pw(when(c, 1), 0)`. Each of the `max_guards` most common outputs, when at least `min_guarded` points have it, is
// tried as d.
//
// The default may also be a model: each of the first `max_models` models the value domain proposes for all the
// points is evaluated at every point, and when it reproduces at least half of them but not all, it is d and the points
// it misses are the guarded ones. So data that follows `#0^2` except where a condition holds reads as
// `pw(when(c, v), #0^2)`: a piecewise whose branches are guarded by any condition the condition domain reads. Each
// condition with each value is checked against every point, and only exact readings are kept. A condition is read only from at least
// `min_guarded` guarded points and as many others: fewer exceptions are left as cases, since a condition read from one
// or two points does not generalize. Nothing when every output is the same, an input has two outputs, or one of the
// models already reproduces every point and is not itself a piecewise. Only the `max_proposals` smallest
// readings are proposed, since each is checked against all the data. The value domain's own proposals for all the
// points (the models tried as defaults) are passed through as well, so this stands in for the value domain where both
// are wanted, and fits the points once. Proposals are memoized per data set, since readings that try several starts
// ask again for the same data.
//
// The mirror reading takes a common output k as the value, where a condition holds, and reads the other points again
// as the default, by this same detector, so a default may itself be guarded and the branches nest:
// `pw(when(c1, k1), when(c2, k2), d)`, the inner piecewise's branches spliced into the outer one. So data that is 1 on
// the evens, 2 on the other multiples of 3 and `#0^2` elsewhere reads as three branches. Each of the `max_guards`
// most common outputs shared by at least `min_guarded` points, with as many others, is tried as k, when at least two
// outputs are that common (with one, the model default reads the same branch) and together they cover at least half
// of the points.
//
// The condition may also come first, so a value may be a model: for each of the first `max_atoms` atoms the condition
// domain offers (`propose_atoms`: primality, residues, ...), the points where it holds are fitted by the value domain's
// single models (`propose_atom_values`, cheaper than all its readings), and when one model reproduces all of them and
// at most half of the others, `when(atom, model)` is peeled and the others are read again by this detector, at most
// `max_atom_depth` peels deep; the first atom that gives a reading ends it. So data that is `#0^2` at the primes, `2 *
// #0` at the multiples of 4 and `#0 + 1` elsewhere reads as those three branches. Only on data of at least three times
// `min_guarded` points that no model, piecewise or not, reproduces.
template<typename IProposeConditions, typename IProposeAtoms, typename IProposeValues,
         typename IProposeAtomValues, typename IEvaluateAt, typename IFindCost, typename IMakeLiteral,
         typename IMakeFunctor>
struct pw_guarded_hypotheses {
    pw_guarded_hypotheses(IProposeConditions& propose_conditions,
                          IProposeAtoms& propose_atoms,
                          IProposeValues& propose_values,
                          IProposeAtomValues& propose_atom_values,
                          IEvaluateAt& evaluate_at,
                          IFindCost& find_cost,
                          IMakeLiteral& make_literal,
                          IMakeFunctor& make_functor,
                          pw_symbols symbols,
                          size_t max_guards = k_pw_default_max_guards,
                          size_t max_models = k_pw_default_max_models,
                          size_t min_guarded = k_pw_default_min_guarded,
                          size_t max_proposals = k_pw_default_max_guarded_proposals,
                          size_t max_atoms = k_pw_default_max_peeled_atoms,
                          size_t max_atom_depth = k_pw_default_max_atom_depth);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    void propose_fresh(const std::vector<pw_data_point>& all, const std::vector<const expr*>& models,
                       std::vector<const expr*>& out);
    void propose_guarded(const std::vector<pw_data_point>& points, const expr* fallback,
                         const std::vector<bool>& guarded, std::vector<const expr*>& out);
    void propose_peeled(const std::vector<pw_data_point>& points, const expr* value, std::vector<const expr*>& out);
    void propose_atom_peeled(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
    const expr* splice(const expr* region, const expr* fallback);
    bool reproduces(const expr* reading, const std::vector<pw_data_point>& points);

    IProposeConditions& propose_conditions_;
    IProposeAtoms& propose_atoms_;
    IProposeValues& propose_values_;
    IProposeAtomValues& propose_atom_values_;
    IEvaluateAt& evaluate_at_;
    IFindCost& find_cost_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    pw_symbols symbols_;
    size_t max_guards_;
    size_t max_models_;
    size_t min_guarded_;
    size_t max_proposals_;
    size_t max_atoms_;
    size_t max_atom_depth_;
    size_t depth_;
    std::map<std::vector<pw_data_point>, std::vector<const expr*>> proposed_;
};

#define PW_GUARDED_HYPOTHESES_TEMPLATE                                                                      \
    template<typename IProposeConditions, typename IProposeAtoms, typename IProposeValues,                    \
             typename IProposeAtomValues, typename IEvaluateAt, typename IFindCost, typename IMakeLiteral,     \
             typename IMakeFunctor>
#define PW_GUARDED_HYPOTHESES                                                                                   \
    pw_guarded_hypotheses<IProposeConditions, IProposeAtoms, IProposeValues, IProposeAtomValues, IEvaluateAt,  \
                          IFindCost, IMakeLiteral, IMakeFunctor>

PW_GUARDED_HYPOTHESES_TEMPLATE PW_GUARDED_HYPOTHESES::pw_guarded_hypotheses(IProposeConditions& propose_conditions,
                                                                            IProposeAtoms& propose_atoms,
                                                                            IProposeValues& propose_values,
                                                                            IProposeAtomValues& propose_atom_values,
                                                                            IEvaluateAt& evaluate_at,
                                                                            IFindCost& find_cost,
                                                                            IMakeLiteral& make_literal,
                                                                            IMakeFunctor& make_functor,
                                                                            pw_symbols symbols,
                                                                            size_t max_guards,
                                                                            size_t max_models,
                                                                            size_t min_guarded,
                                                                            size_t max_proposals,
                                                                            size_t max_atoms,
                                                                            size_t max_atom_depth)
    : propose_conditions_(propose_conditions)
    , propose_atoms_(propose_atoms)
    , propose_values_(propose_values)
    , propose_atom_values_(propose_atom_values)
    , evaluate_at_(evaluate_at)
    , find_cost_(find_cost)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , max_guards_(max_guards)
    , max_models_(max_models)
    , min_guarded_(min_guarded)
    , max_proposals_(max_proposals)
    , max_atoms_(max_atoms)
    , max_atom_depth_(max_atom_depth)
    , depth_(0)
    , proposed_() {}

PW_GUARDED_HYPOTHESES_TEMPLATE void PW_GUARDED_HYPOTHESES::propose_hypotheses(const std::vector<pw_data_point>& points,
                                                                             std::vector<const expr*>& out) {
    auto known = proposed_.find(points);
    if (known == proposed_.end()) {
        std::vector<const expr*> models;
        propose_values_.propose_hypotheses(points, models);
        std::vector<const expr*> fresh;
        propose_fresh(points, models, fresh);
        std::vector<std::pair<size_t, const expr*>> ranked;
        for (const expr* reading : fresh)
            ranked.emplace_back(find_cost_.cost(reading), reading);
        std::sort(ranked.begin(), ranked.end());
        std::vector<const expr*> smallest = models;
        const size_t limit = models.size() + max_proposals_;
        for (const auto& [cost, reading] : ranked) {
            const bool fresh_reading = std::find(smallest.begin(), smallest.end(), reading) == smallest.end();
            if (smallest.size() < limit && fresh_reading)
                smallest.push_back(reading);
        }
        known = proposed_.emplace(points, smallest).first;
    }
    out.insert(out.end(), known->second.begin(), known->second.end());
}

PW_GUARDED_HYPOTHESES_TEMPLATE void PW_GUARDED_HYPOTHESES::propose_fresh(const std::vector<pw_data_point>& all,
                                                                        const std::vector<const expr*>& models,
                                                                        std::vector<const expr*>& out) {
    // data that gives one input two outputs is no function: nothing guards it; an input seen again is no more
    // evidence, so the readings work on the distinct inputs
    std::map<const expr*, const expr*> outputs;
    std::vector<pw_data_point> points;
    for (const pw_data_point& point : all) {
        const auto [known, inserted] = outputs.emplace(point.input, point.output);
        if (!inserted && known->second != point.output)
            return;
        if (inserted)
            points.push_back(point);
    }
    std::map<const expr*, size_t> counts;
    for (const pw_data_point& point : points)
        ++counts[point.output];
    if (counts.size() < 2)
        return;
    // which points each model reproduces; a model that reproduces them all leaves nothing to guard, unless it is a
    // piecewise
    std::vector<std::vector<bool>> misses;
    bool explained = false;
    const size_t kept_models = std::min(models.size(), max_models_);
    for (size_t index = 0; index < kept_models; ++index) {
        std::vector<bool> missed;
        for (const pw_data_point& point : points) {
            const std::optional<const expr*> value = evaluate_at_.evaluate_at(models[index], point.input);
            missed.push_back(!value.has_value() || value.value() != point.output);
        }
        // a piecewise that reproduces them all may still be larger than a guarded reading
        const expr::functor* head = std::get_if<expr::functor>(&models[index]->content);
        const bool piecewise = head != nullptr && head->id == symbols_.piecewise;
        const bool exact = std::find(missed.begin(), missed.end(), true) == missed.end();
        if (exact && !piecewise)
            return;
        explained = explained || exact;
        misses.push_back(missed);
    }
    std::vector<std::pair<size_t, const expr*>> common;
    for (const auto& [output, count] : counts)
        common.emplace_back(count, output);
    std::sort(common.rbegin(), common.rend());
    // the most common outputs as defaults, when they are not singular (a value at most a few points share is no
    // default)
    for (size_t guard = 0; guard < common.size() && guard < max_guards_; ++guard) {
        if (common[guard].first < min_guarded_)
            break;
        std::vector<bool> guarded;
        for (const pw_data_point& point : points)
            guarded.push_back(point.output != common[guard].second);
        propose_guarded(points, common[guard].second, guarded, out);
    }
    // the mirror: common outputs as values, the other points read again as the default; only when another common
    // output remains and a third output besides, since otherwise the readings above find the same branches, and when
    // the common outputs cover at least half of the points (a few repeated values among many are no branches)
    size_t covered = 0;
    for (const auto& [count, output] : common)
        covered += count >= min_guarded_ ? count : 0;
    const bool several = common.size() >= 3 && common[1].first >= min_guarded_ && 2 * covered >= points.size();
    for (size_t guard = 0; several && guard < common.size() && guard < max_guards_; ++guard) {
        if (common[guard].first < min_guarded_)
            break;
        if (points.size() - common[guard].first < min_guarded_)
            continue;
        propose_peeled(points, common[guard].second, out);
    }
    // models as defaults: those that reproduce at least half of the points
    for (size_t index = 0; index < kept_models; ++index) {
        const size_t missed = static_cast<size_t>(std::count(misses[index].begin(), misses[index].end(), true));
        if (2 * missed > points.size())
            continue;
        propose_guarded(points, models[index], misses[index], out);
    }
    // conditions first: a model where an atom holds, the other points read again
    const bool peelable = !explained && depth_ < max_atom_depth_ && points.size() >= 3 * min_guarded_;
    if (peelable)
        propose_atom_peeled(points, out);
}

// pw(when(c, v), fallback) for each condition read from the guarded points and each value fitted to them.
PW_GUARDED_HYPOTHESES_TEMPLATE void PW_GUARDED_HYPOTHESES::propose_guarded(const std::vector<pw_data_point>& points,
                                                                          const expr* fallback,
                                                                          const std::vector<bool>& guarded,
                                                                          std::vector<const expr*>& out) {
    const size_t count = static_cast<size_t>(std::count(guarded.begin(), guarded.end(), true));
    const bool readable = count >= min_guarded_ && points.size() - count >= min_guarded_;
    if (!readable)
        return;
    const expr* holds = make_literal_.make_literal(integer(1));
    const expr* fails = make_literal_.make_literal(integer(0));
    std::vector<pw_data_point> conditions_data;
    std::vector<pw_data_point> values_data;
    for (size_t index = 0; index < points.size(); ++index) {
        const expr* label = guarded[index] ? holds : fails;
        conditions_data.push_back(pw_data_point{.input = points[index].input, .output = label});
        if (guarded[index])
            values_data.push_back(points[index]);
    }
    std::vector<const expr*> conditions;
    propose_conditions_.propose_hypotheses(conditions_data, conditions);
    if (conditions.empty())
        return;
    std::vector<const expr*> values;
    propose_values_.propose_hypotheses(values_data, values);
    // only readings that reproduce every point: the domains may also propose near fits
    for (const expr* condition : conditions)
        for (const expr* value : values) {
            const expr* region = make_functor_.make_functor(symbols_.region, {condition, value});
            const expr* reading = make_functor_.make_functor(symbols_.piecewise, {region, fallback});
            if (reproduces(reading, points))
                out.push_back(reading);
        }
}

// pw(when(c, value), d) for each condition read from where the output is `value` and each reading d of the other
// points, a piecewise d spliced in.
PW_GUARDED_HYPOTHESES_TEMPLATE void PW_GUARDED_HYPOTHESES::propose_peeled(const std::vector<pw_data_point>& points,
                                                                         const expr* value,
                                                                         std::vector<const expr*>& out) {
    const expr* holds = make_literal_.make_literal(integer(1));
    const expr* fails = make_literal_.make_literal(integer(0));
    std::vector<pw_data_point> conditions_data;
    std::vector<pw_data_point> rest;
    for (const pw_data_point& point : points) {
        const bool valued = point.output == value;
        conditions_data.push_back(pw_data_point{.input = point.input, .output = valued ? holds : fails});
        if (!valued)
            rest.push_back(point);
    }
    std::vector<const expr*> conditions;
    propose_conditions_.propose_hypotheses(conditions_data, conditions);
    if (conditions.empty())
        return;
    std::vector<const expr*> defaults;
    propose_hypotheses(rest, defaults);
    for (const expr* condition : conditions)
        for (const expr* fallback : defaults) {
            const expr* reading = splice(make_functor_.make_functor(symbols_.region, {condition, value}), fallback);
            if (reproduces(reading, points))
                out.push_back(reading);
        }
}

// pw(when(atom, model), d) for each atom whose points one model reproduces, and each reading d of the other points
PW_GUARDED_HYPOTHESES_TEMPLATE void PW_GUARDED_HYPOTHESES::propose_atom_peeled(const std::vector<pw_data_point>& points,
                                                                              std::vector<const expr*>& out) {
    std::vector<const expr*> atoms;
    propose_atoms_.propose_atoms(points, atoms);
    const expr* holds = make_literal_.make_literal(integer(1));
    const expr* fails = make_literal_.make_literal(integer(0));
    for (size_t index = 0; index < atoms.size() && index < max_atoms_; ++index) {
        const expr* test = make_functor_.make_functor(
            symbols_.piecewise, {make_functor_.make_functor(symbols_.region, {atoms[index], holds}), fails});
        std::vector<pw_data_point> held;
        std::vector<pw_data_point> rest;
        bool decided = true;
        for (const pw_data_point& point : points) {
            const std::optional<const expr*> truth = evaluate_at_.evaluate_at(test, point.input);
            decided = decided && truth.has_value();
            if (!decided)
                break;
            (truth.value() == holds ? held : rest).push_back(point);
        }
        const bool readable = decided && held.size() >= min_guarded_ && rest.size() >= min_guarded_;
        if (!readable)
            continue;
        std::vector<const expr*> values;
        propose_atom_values_.propose_hypotheses(held, values);
        const expr* model = nullptr;
        for (const expr* value : values) {
            const bool smaller = model == nullptr || find_cost_.cost(value) < find_cost_.cost(model);
            if (smaller && reproduces(value, held))
                model = value;
        }
        if (model == nullptr)
            continue;
        // a model of the atom's points that also explains most others is the default, with exceptions, not a branch
        size_t explained = 0;
        for (const pw_data_point& point : rest) {
            const std::optional<const expr*> value = evaluate_at_.evaluate_at(model, point.input);
            explained += value.has_value() && value.value() == point.output ? 1 : 0;
        }
        if (2 * explained > rest.size())
            continue;
        std::vector<const expr*> defaults;
        ++depth_;
        propose_hypotheses(rest, defaults);
        --depth_;
        const expr* region = make_functor_.make_functor(symbols_.region, {atoms[index], model});
        const size_t before = out.size();
        for (const expr* fallback : defaults) {
            const expr* reading = splice(region, fallback);
            if (reproduces(reading, points))
                out.push_back(reading);
        }
        // the first atom that peels is enough: another order of the same branches reads the same
        if (out.size() > before)
            return;
    }
}

// pw(region, fallback), the branches of a piecewise fallback spliced in
PW_GUARDED_HYPOTHESES_TEMPLATE const expr* PW_GUARDED_HYPOTHESES::splice(const expr* region, const expr* fallback) {
    std::vector<const expr*> branches{region};
    const expr::functor* inner = std::get_if<expr::functor>(&fallback->content);
    const bool nested = inner != nullptr && inner->id == symbols_.piecewise;
    if (nested)
        branches.insert(branches.end(), inner->args.begin(), inner->args.end());
    if (!nested)
        branches.push_back(fallback);
    return make_functor_.make_functor(symbols_.piecewise, branches);
}

PW_GUARDED_HYPOTHESES_TEMPLATE bool PW_GUARDED_HYPOTHESES::reproduces(const expr* reading,
                                                                     const std::vector<pw_data_point>& points) {
    for (const pw_data_point& point : points) {
        const std::optional<const expr*> produced = evaluate_at_.evaluate_at(reading, point.input);
        if (!produced.has_value() || produced.value() != point.output)
            return false;
    }
    return true;
}

#undef PW_GUARDED_HYPOTHESES
#undef PW_GUARDED_HYPOTHESES_TEMPLATE

#endif
