// List-program benchmark: samples of programs over integer lists (folds, maps, filters, some with a wrong label),
// under the best-first and MCTS list solvers. A problem is solved when the result is no larger than a known compact
// form (its cost is given, since programs do not parse). Every result is checked by evaluating it at every sample.
//
//   list_bench [--fast] [--serial] [--write FILE] [--check FILE]
//
// --fast skips MCTS on the large data sets; --write saves the results, --check gates them against a baseline
// (bench/bench_gate.hpp).
// Problems are solved in parallel, one per thread; --serial solves them one at a time, for true timings.
#include <chrono>
#include <cstdio>
#include <functional>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "infrastructure/list_fit_manifest.hpp"
#include "infrastructure/list_fit_mcts_manifest.hpp"
#include "infrastructure/num_prime_table.hpp"
#include "value_objects/list_sample.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"
#include "bench_gate.hpp"

namespace {

using list_t = std::vector<integer>;

struct list_problem {
    std::string name;
    std::vector<std::vector<integer>> inputs;
    bool output_is_list;
    std::function<list_t(const list_t&)> program;
    std::vector<std::pair<size_t, int>> wrong_labels;  // sample index, offset added to its first output
    size_t target;
    std::string target_text;
};

struct outcome {
    size_t cost;
    std::string best;
    double seconds;
    bool sound;
};

constexpr size_t k_rewrites = 3000;
constexpr size_t k_frontier = 20000;
constexpr size_t k_probing_depth = 12;
constexpr size_t k_simulations = 150;
constexpr double k_exploration = 0.4;
constexpr size_t k_large = 160;  // samples; --fast skips MCTS above this

// Every list of length at most `max_length` with elements in [low, high], shortest first.
std::vector<list_t> all_lists(int low, int high, size_t max_length) {
    std::vector<list_t> lists{{}};
    size_t begin = 0;
    for (size_t length = 1; length <= max_length; ++length) {
        const size_t end = lists.size();
        for (size_t index = begin; index < end; ++index)
            for (int head = low; head <= high; ++head) {
                list_t list{head};
                list.insert(list.end(), lists[index].begin(), lists[index].end());
                lists.push_back(list);
            }
        begin = end;
    }
    return lists;
}

// Every list of exactly `length` elements in [low, high]: no empty list and no tails.
std::vector<list_t> exact_lists(int low, int high, size_t length) {
    std::vector<list_t> lists;
    for (const list_t& list : all_lists(low, high, length))
        if (list.size() == length)
            lists.push_back(list);
    return lists;
}

// `count` lists with lengths in [min_length, max_length] and elements in [low, high], from a seeded generator.
std::vector<list_t> random_lists(size_t count, size_t min_length, size_t max_length, int low, int high,
                                 unsigned seed) {
    std::mt19937 rng{seed};
    std::vector<list_t> lists;
    for (size_t index = 0; index < count; ++index) {
        const size_t length = min_length + rng() % (max_length - min_length + 1);
        list_t list;
        for (size_t position = 0; position < length; ++position)
            list.push_back(low + static_cast<int>(rng() % static_cast<unsigned>(high - low + 1)));
        lists.push_back(list);
    }
    return lists;
}

// The lists and every suffix of each (down to the empty list), each once, shortest first.
std::vector<list_t> with_tails(const std::vector<list_t>& lists) {
    std::set<std::pair<size_t, list_t>> ordered;
    for (const list_t& list : lists)
        for (size_t start = 0; start <= list.size(); ++start)
            ordered.emplace(list.size() - start, list_t(list.begin() + start, list.end()));
    std::vector<list_t> suffixes;
    for (const auto& [length, suffix] : ordered)
        suffixes.push_back(suffix);
    return suffixes;
}

std::vector<list_sample> sample(const list_problem& p) {
    std::vector<list_sample> samples;
    for (const list_t& list : p.inputs)
        samples.push_back(list_sample{.input = list, .output = p.program(list), .output_is_list = p.output_is_list});
    for (const auto& [index, offset] : p.wrong_labels)
        samples[index].output[0] += offset;
    return samples;
}

template<typename Manifest>
outcome finish(Manifest& m, const std::vector<list_sample>& samples, std::chrono::steady_clock::time_point start) {
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const expr* best = m.search_.best();
    bool sound = true;
    for (const pw_data_point& point : m.rule_set_.converter_.convert(samples)) {
        const std::optional<const expr*> value = m.rule_set_.pw_evaluator_.evaluate_at(best, point.input);
        sound = sound && value.has_value() && value.value() == point.output;
    }
    std::ostringstream os;
    m.kernel_.printer_.print(os, best);
    return outcome{.cost = m.search_.best_cost(), .best = os.str(), .seconds = seconds, .sound = sound};
}

outcome run_best_first(const std::vector<list_sample>& samples) {
    const auto start = std::chrono::steady_clock::now();
    list_fit_manifest m{samples, k_poly_default_limits, k_poly_fit_default_limits, k_frontier};
    while (m.search_.rewrites() < k_rewrites && m.search_.next()) {}
    return finish(m, samples, start);
}

outcome run_mcts(const std::vector<list_sample>& samples) {
    const auto start = std::chrono::steady_clock::now();
    list_fit_mcts_manifest m{samples, k_poly_default_limits, k_poly_fit_default_limits, k_probing_depth,
                             k_simulations, k_exploration, 1};
    while (m.search_.next()) {}
    return finish(m, samples, start);
}

std::string cell(const outcome& o, size_t target) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%5zu%s %6.2fs", o.cost, !o.sound ? "!" : o.cost <= target ? "*" : " ",
                  o.seconds);
    return buffer;
}

list_t fold_right(const list_t& list, integer base, const std::function<integer(integer, integer)>& step) {
    integer accumulated = base;
    for (auto element = list.rbegin(); element != list.rend(); ++element)
        accumulated = step(*element, accumulated);
    return {accumulated};
}

list_t map_each(const list_t& list, const std::function<integer(integer)>& f) {
    list_t result;
    for (const integer& value : list)
        result.push_back(f(value));
    return result;
}

list_t keep(const list_t& list, const std::function<bool(integer)>& condition) {
    list_t result;
    for (const integer& value : list)
        if (condition(value))
            result.push_back(value);
    return result;
}

list_t sum(const list_t& list) {
    return fold_right(list, 0, [](integer h, integer acc) { return h + acc; });
}

// one problem under both solvers
struct solved {
    size_t points;
    bool skip_mcts;
    outcome best_first;
    outcome mcts;
};

solved solve(const list_problem& p, bool fast) {
    const std::vector<list_sample> samples = sample(p);
    const bool skip_mcts = fast && samples.size() > k_large;
    const outcome skipped{.cost = 0, .best = "(skipped)", .seconds = 0, .sound = true};
    return solved{.points = samples.size(), .skip_mcts = skip_mcts, .best_first = run_best_first(samples),
                  .mcts = skip_mcts ? skipped : run_mcts(samples)};
}

// The total of an invoice: each item's price plus a fee of 2.
list_t invoice_total(const list_t& list) {
    return fold_right(list, 0, [](integer h, integer acc) { return h + 2 + acc; });
}

// The primes of a list, in order.
list_t keep_primes(const list_t& list) {
    num_prime_table table{k_num_default_max_value};
    list_t kept;
    for (const integer& value : list)
        if (table.find_primality(value).value())
            kept.push_back(value);
    return kept;
}

}

int main(int argc, char** argv) {
    const bench_options options = parse_bench_options(argc, argv);
    const std::vector<list_problem> problems{
        {"sum", all_lists(-2, 2, 3), false, sum, {}, 9, "lam(fold(lam(lam(#0 + #1)), 0, #0))"},
        {"length", all_lists(-2, 2, 3), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer, integer acc) { return acc + 1; }); }, {}, 9,
         "lam(fold(lam(lam(#1 + 1)), 0, #0))"},
        {"product", all_lists(-2, 2, 3), false,
         [](const list_t& l) { return fold_right(l, 1, [](integer h, integer acc) { return h * acc; }); }, {}, 9,
         "lam(fold(lam(lam(#0 * #1)), 1, #0))"},
        {"sum of squares", all_lists(-2, 2, 3), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return h * h + acc; }); }, {}, 11,
         "lam(fold(lam(lam(#0^2 + #1)), 0, #0))"},
        {"alternating sum", all_lists(-2, 2, 3), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return h - acc; }); }, {}, 9,
         "lam(fold(lam(lam(#0 - #1)), 0, #0))"},
        {"digits", all_lists(0, 3, 3), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return 10 * acc + h; }); }, {}, 11,
         "lam(fold(lam(lam(10 * #1 + #0)), 0, #0))"},
        {"sum with wrong label", all_lists(-2, 2, 3), false, sum, {{7, 10}}, 17, "pw(case(cons(-1, cons(-2, nil)), 7), lam(fold(lam(lam(#0 + #1)), 0, #0)))"},
        {"sum 781", all_lists(-2, 2, 4), false, sum, {}, 9, "lam(fold(lam(lam(#0 + #1)), 0, #0))"},
        {"map square", all_lists(-2, 2, 3), true, [](const list_t& l) { return map_each(l, [](integer e) { return e * e; }); },
         {}, 7, "lam(map(lam(#0^2), #0))"},
        {"map affine", all_lists(-2, 2, 3), true,
         [](const list_t& l) { return map_each(l, [](integer e) { return 2 * e + 1; }); }, {}, 9, "lam(map(lam(2 * #0 + 1), #0))"},
        {"map absolute value", all_lists(-3, 3, 2), true,
         [](const list_t& l) { return map_each(l, [](integer e) { return e < 0 ? integer(-e) : e; }); }, {}, 13,
         "lam(map(lam(pw(when(lt(#0, 1), -1 * #0), #0)), #0))"},
        {"map with wrong label", all_lists(-2, 2, 3), true,
         [](const list_t& l) { return map_each(l, [](integer e) { return e * e; }); }, {{3, 5}}, 15,
         "pw(case(cons(0, nil), cons(5, nil)), lam(map(lam(#0^2), #0)))"},
        {"filter positives", all_lists(-2, 2, 3), true, [](const list_t& l) { return keep(l, [](integer e) { return e > 0; }); },
         {}, 7, "lam(filter(lam(lt(0, #0)), #0))"},
        {"filter below one", all_lists(-2, 2, 3), true, [](const list_t& l) { return keep(l, [](integer e) { return e < 1; }); },
         {}, 7, "lam(filter(lam(lt(#0, 1)), #0))"},
        // reduce request #14, and similar filters by primality: the kept and removed values go to the condition domain
        {"#14 primes of a list", random_lists(30, 2, 8, 0, 40, 61), true, keep_primes, {}, 7,
         "lam(filter(lam(is_prime(#0)), #0))"},
        {"primes of longer lists", random_lists(20, 10, 20, -20, 100, 7), true, keep_primes, {}, 6,
         "lam(filter(lam(is_prime(#0)), #0))"},
        {"primes of a list, wrong label", random_lists(30, 2, 8, 0, 40, 61), true, keep_primes, {{4, 1}}, 24,
         "one case + lam(filter(lam(is_prime(#0)), #0))"},
        // long lists: 20 to 40 elements, values up to 50 in size; folds get every list's tails, maps and filters none
        {"long sum", with_tails(random_lists(12, 20, 40, -50, 50, 1)), false, sum, {}, 9, "lam(fold(lam(lam(#0 + #1)), 0, #0))"},
        {"long length", with_tails(random_lists(12, 20, 40, -50, 50, 6)), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer, integer acc) { return acc + 1; }); }, {}, 9,
         "lam(fold(lam(lam(#1 + 1)), 0, #0))"},
        {"long sum of squares", with_tails(random_lists(12, 20, 40, -50, 50, 5)), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return h * h + acc; }); }, {}, 11,
         "lam(fold(lam(lam(#0^2 + #1)), 0, #0))"},
        {"long product", with_tails(random_lists(12, 20, 40, -9, 9, 2)), false,
         [](const list_t& l) { return fold_right(l, 1, [](integer h, integer acc) { return h * acc; }); }, {}, 9,
         "lam(fold(lam(lam(#0 * #1)), 1, #0))"},
        {"long sum with wrong label", with_tails(random_lists(12, 20, 40, -50, 50, 1)), false, sum, {{3, 100}}, 15,
         "pw(case(cons(v, nil), v + 100), lam(fold(lam(lam(#0 + #1)), 0, #0)))"},
        {"long map", random_lists(30, 20, 40, -50, 50, 3), true,
         [](const list_t& l) { return map_each(l, [](integer e) { return 2 * e * e - 3; }); }, {}, 11,
         "lam(map(lam(2 * #0^2 - 3), #0))"},
        {"long map absolute value", random_lists(30, 20, 40, -50, 50, 7), true,
         [](const list_t& l) { return map_each(l, [](integer e) { return e < 0 ? integer(-e) : e; }); }, {}, 13,
         "lam(map(lam(pw(when(lt(#0, 1), -1 * #0), #0)), #0))"},
        {"long filter", random_lists(30, 20, 40, -50, 50, 4), true,
         [](const list_t& l) { return keep(l, [](integer e) { return e < 17; }); }, {}, 7, "lam(filter(lam(lt(#0, 17)), #0))"},
        {"maximum", all_lists(0, 3, 3), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return h < acc ? acc : h; }); },
         {}, 13, "lam(fold(lam(lam(pw(when(lt(#1, #0), #0), #1))), 0, #0))"},
        {"sum without tails", exact_lists(-2, 2, 3), false, sum, {}, 9, "lam(fold(lam(lam(#0 + #1)), 0, #0))"},
        {"long sum without tails", random_lists(40, 20, 40, -50, 50, 8), false, sum, {}, 9,
         "lam(fold(lam(lam(#0 + #1)), 0, #0))"},
        {"long minimum without tails", random_lists(30, 20, 40, -50, 50, 14), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return h < acc ? h : acc; }); },
         {}, 13, "lam(fold(lam(lam(pw(when(lt(#0, #1), #0), #1))), 0, #0))"},
        {"long maximum with wrong label", random_lists(30, 20, 40, -50, 50, 15), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return h < acc ? acc : h; }); },
         {{4, 100}}, 63, "pw(case(cons(v, ...), w), lam(fold(lam(lam(pw(when(lt(#1, #0), #0), #1))), 0, #0)))"},
        {"product without tails", random_lists(30, 5, 10, -5, 5, 16), false,
         [](const list_t& l) { return fold_right(l, 1, [](integer h, integer acc) { return h * acc; }); }, {}, 9,
         "lam(fold(lam(lam(#0 * #1)), 1, #0))"},
        {"long product with wrong label", with_tails(random_lists(12, 20, 40, -9, 9, 2)), false,
         [](const list_t& l) { return fold_right(l, 1, [](integer h, integer acc) { return h * acc; }); }, {{5, 1}}, 15,
         "pw(case(cons(0, nil), 1), lam(fold(lam(lam(#0 * #1)), 1, #0)))"},
        {"long count without tails", random_lists(30, 20, 40, -50, 50, 17), false,
         [](const list_t& l) {
             return fold_right(l, 0, [](integer h, integer acc) { return integer(10) < h ? acc + 1 : acc; });
         },
         {}, 15, "lam(fold(lam(lam(pw(when(lt(10, #0), #1 + 1), #1))), 0, #0))"},
        {"squares of positives", random_lists(30, 2, 6, -9, 9, 28), true,
         [](const list_t& l) { return map_each(keep(l, [](integer e) { return e > 0; }), [](integer e) { return e * e; }); },
         {}, 12, "lam(map(lam(#0^2), filter(lam(lt(0, #0)), #0)))"},
        // reduce request #4, and similar sums of an inner function (counts on any condition) for the sum fold reading
        {"#4 count of evens", random_lists(30, 2, 8, -9, 9, 34), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return h % 2 == 0 ? acc + 1 : acc; }); },
         {}, 18, "lam(fold(lam(lam(pw(when(eq(mod(#0, 2), 0), #1 + 1), #1))), 0, #0))"},
        {"count of multiples of 3", random_lists(30, 2, 8, -9, 9, 47), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return h % 3 == 0 ? acc + 1 : acc; }); },
         {}, 17, "lam(fold(lam(lam(pw(when(eq(mod(#0, 3), 0), 1 + #1), #1))), 0, #0))"},
        {"sum of the evens", random_lists(30, 2, 8, -9, 9, 49), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return h % 2 == 0 ? acc + h : acc; }); },
         {}, 17, "lam(fold(lam(lam(pw(when(eq(mod(#0, 2), 0), #0 + #1), #1))), 0, #0))"},
        {"sum of absolute values (open)", random_lists(30, 2, 8, -9, 9, 50), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return integer(acc + abs(h)); }); },
         {}, 17, "lam(fold(lam(lam(pw(when(lt(#0, 1), #1 - #0), #0 + #1))), 0, #0))"},
        {"count of evens, long (open)", random_lists(30, 20, 40, -50, 50, 48), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return h % 2 == 0 ? acc + 1 : acc; }); },
         {}, 18, "lam(fold(lam(lam(pw(when(eq(mod(#0, 2), 0), #1 + 1), #1))), 0, #0))"},
        // reduce request #5, and similar folds that build lists, for the listed fold detector
        {"#5 reverse", random_lists(30, 0, 6, -9, 9, 35), true,
         [](const list_t& l) { return list_t(l.rbegin(), l.rend()); },
         {}, 3, "lam(reverse(#0))"},
        {"duplicate each element", random_lists(30, 0, 6, -9, 9, 51), true,
         [](const list_t& l) {
             list_t result;
             for (const integer& value : l) {
                 result.push_back(value);
                 result.push_back(value);
             }
             return result;
         },
         {}, 11, "lam(fold(lam(lam(cons(#0, cons(#0, #1)))), nil, #0))"},
        {"reverse with each element twice", random_lists(30, 0, 6, -9, 9, 52), true,
         [](const list_t& l) {
             list_t result;
             for (auto value = l.rbegin(); value != l.rend(); ++value) {
                 result.push_back(*value);
                 result.push_back(*value);
             }
             return result;
         },
         {}, 13, "lam(fold(lam(lam(append(#1, cons(#0, cons(#0, nil))))), nil, #0))"},
        {"reverse, wrong label", random_lists(30, 0, 6, -9, 9, 53), true,
         [](const list_t& l) { return list_t(l.rbegin(), l.rend()); },
         {{3, 1}}, 19, "pw(case(cons(1, cons(-4, cons(9, nil))), cons(10, cons(-4, cons(1, nil)))), lam(reverse(#0)))"},
        // reduce request #1, and similar long compositions for the signature reading of a map over a filter
        {"#1 long map over filter", random_lists(30, 20, 40, -50, 50, 31), true,
         [](const list_t& l) { return map_each(keep(l, [](integer e) { return e < 17; }), [](integer e) { return 2 * e + 1; }); },
         {}, 14, "lam(map(lam(2 * #0 + 1), filter(lam(lt(#0, 17)), #0)))"},
        {"long squares of positives", random_lists(30, 20, 40, -50, 50, 45), true,
         [](const list_t& l) { return map_each(keep(l, [](integer e) { return e > 0; }), [](integer e) { return e * e; }); },
         {}, 12, "lam(map(lam(#0^2), filter(lam(lt(0, #0)), #0)))"},
        {"long map over filter above", random_lists(30, 20, 40, -50, 50, 46), true,
         [](const list_t& l) { return map_each(keep(l, [](integer e) { return e > -10; }), [](integer e) { return 3 * e - 2; }); },
         {}, 14, "lam(map(lam(3 * #0 - 2), filter(lam(lt(-10, #0)), #0)))"},
        // reduce requests #2 and #3, and similar problems for the generic selection and factor folds
        {"#2 largest square", random_lists(30, 2, 6, -9, 9, 32), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return acc < h * h ? h * h : acc; }); },
         {}, 17, "lam(fold(lam(lam(pw(when(lt(#1, #0^2), #0^2), #1))), 0, #0))"},
        {"#3 product of squares", random_lists(30, 2, 6, -4, 4, 33), false,
         [](const list_t& l) { return fold_right(l, 1, [](integer h, integer acc) { return integer(h * h * acc); }); }, {}, 11,
         "lam(fold(lam(lam(#0^2 * #1)), 1, #0))"},
        {"largest absolute value (open)", random_lists(30, 2, 6, -9, 9, 42), false,
         [](const list_t& l) {
             return fold_right(l, 0, [](integer h, integer acc) { return acc < abs(h) ? integer(abs(h)) : acc; });
         },
         {}, 29, "lam(fold(lam(lam(pw(when(lt(#1, |#0|), |#0|), #1))), 0, #0), |#0| as pw(when(lt(#0, 1), -1 * #0), #0))"},
        {"smallest of 2h + 1", random_lists(30, 2, 6, -9, 9, 37), false,
         [](const list_t& l) {
             return fold_right(l, 100, [](integer h, integer acc) { return 2 * h + 1 < acc ? integer(2 * h + 1) : acc; });
         },
         {}, 21, "lam(fold(lam(lam(pw(when(lt(2 * #0 + 1, #1), 2 * #0 + 1), #1))), 7, #0))"},
        {"product of h^2 + 1", random_lists(30, 1, 5, -3, 3, 43), false,
         [](const list_t& l) { return fold_right(l, 3, [](integer h, integer acc) { return integer((h * h + 1) * acc); }); }, {},
         13, "lam(fold(lam(lam((#0^2 + 1) * #1)), 3, #0))"},
        {"long largest square (open)", random_lists(30, 20, 40, -50, 50, 44), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return acc < h * h ? h * h : acc; }); },
         {}, 17, "lam(fold(lam(lam(pw(when(lt(#1, #0^2), #0^2), #1))), 0, #0))"},
        // open: no solver reaches the target yet; kept so the gate reports when one does
        {"squares of positives (open)", all_lists(-2, 2, 3), true,
         [](const list_t& l) { return map_each(keep(l, [](integer e) { return e > 0; }), [](integer e) { return e * e; }); },
         {}, 12, "lam(map(lam(#0^2), filter(lam(lt(0, #0)), #0)))"},
        // showcase (docs/showcase.md): realistic list programs, and variants measuring how far each generalizes
        {"showcase: invoice total, fee 2 per item", with_tails(random_lists(10, 3, 8, 1, 50, 71)), false, invoice_total,
         {}, 11, "lam(fold(lam(lam(#0 + 2 + #1)), 0, #0))"},
        {"showcase: invoice total, no tails", random_lists(30, 3, 8, 1, 50, 72), false, invoice_total, {}, 11,
         "lam(fold(lam(lam(#0 + 2 + #1)), 0, #0))"},
        {"showcase: prices in cents", random_lists(30, 1, 8, 1, 99, 73), true,
         [](const list_t& l) { return map_each(l, [](integer e) { return 100 * e; }); }, {}, 7,
         "lam(map(lam(100 * #0), #0))"},
        {"showcase: prime order IDs", random_lists(25, 10, 30, 0, 200, 74), true, keep_primes, {}, 6,
         "lam(filter(lam(is_prime(#0)), #0))"},
        {"showcase: prime order IDs, a typo", random_lists(25, 10, 30, 0, 200, 74), true, keep_primes, {{3, 1}}, 78,
         "one case + the filter"},
        {"showcase: highest score", random_lists(30, 3, 10, 0, 100, 75), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return h < acc ? acc : h; }); },
         {}, 13, "lam(fold(lam(lam(pw(when(lt(#1, #0), #0), #1))), 0, #0))"},
        {"showcase: passing grades counted", random_lists(30, 3, 10, 0, 100, 76), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return integer(49) < h ? acc + 1 : acc; }); },
         {}, 15, "lam(fold(lam(lam(pw(when(lt(49, #0), #1 + 1), #1))), 0, #0))"},
        {"showcase: playlist reversed", random_lists(30, 0, 12, 1, 500, 77), true,
         [](const list_t& l) { return list_t(l.rbegin(), l.rend()); }, {}, 3, "lam(reverse(#0))"},
        {"showcase: squared positive readings", random_lists(30, 3, 10, -20, 20, 78), true,
         [](const list_t& l) { return map_each(keep(l, [](integer e) { return e > 0; }), [](integer e) { return e * e; }); },
         {}, 12, "lam(map(lam(#0^2), filter(lam(lt(0, #0)), #0)))"},
        {"showcase: invoice total with 5 shipping", random_lists(30, 3, 8, 1, 50, 79), false,
         [](const list_t& l) { return fold_right(l, 5, [](integer h, integer acc) { return h + 2 + acc; }); }, {}, 11,
         "lam(fold(lam(lam(#0 + 2 + #1)), 5, #0))"},
        {"showcase: discounted positive prices", random_lists(30, 3, 10, -20, 60, 81), true,
         [](const list_t& l) { return map_each(keep(l, [](integer e) { return e > 0; }), [](integer e) { return 3 * e - 1; }); },
         {}, 14, "lam(map(lam(3 * #0 - 1), filter(lam(lt(0, #0)), #0)))"},
    };

    std::printf("budgets: best-first %zu rewrites; mcts %zu simulations x depth %zu\n", k_rewrites, k_simulations,
                k_probing_depth);
    std::printf("cost  *: no larger than target   !: UNSOUND (misses a sample)\n\n");
    std::printf("%-28s %6s %6s | %-15s | %-15s\n", "problem", "lists", "target", "best-1st", "mcts");
    size_t solved_counts[2] = {0, 0};
    std::vector<bench_result> gated;
    std::vector<std::string> notes;
    const std::vector<solved> runs = solve_in_parallel(problems, solve, options);
    for (size_t index = 0; index < problems.size(); ++index) {
        const list_problem& p = problems[index];
        const bool skip_mcts = runs[index].skip_mcts;
        const outcome& best_first = runs[index].best_first;
        const outcome& mcts = runs[index].mcts;
        solved_counts[0] += best_first.cost <= p.target ? 1 : 0;
        solved_counts[1] += !skip_mcts && mcts.cost <= p.target ? 1 : 0;
        std::printf("%-28s %6zu %6zu | %s | %s\n", p.name.c_str(), runs[index].points, p.target,
                    cell(best_first, p.target).c_str(), skip_mcts ? "      (skipped)" : cell(mcts, p.target).c_str());
        std::fflush(stdout);
        gated.push_back(bench_result{.problem = p.name, .solver = "best-1st", .cost = best_first.cost,
                                     .target = p.target, .seconds = best_first.seconds, .sound = best_first.sound});
        if (!skip_mcts)
            gated.push_back(bench_result{.problem = p.name, .solver = "mcts", .cost = mcts.cost, .target = p.target,
                                         .seconds = mcts.seconds, .sound = mcts.sound});
        notes.push_back(p.name + "\n    target:   " + p.target_text + "\n    best-1st: " + best_first.best.substr(0, 160)
                        + "\n    mcts:     " + mcts.best.substr(0, 160));
    }
    std::printf("\nsolved (of %zu): best-1st %zu, mcts %zu\n\n", problems.size(), solved_counts[0], solved_counts[1]);
    for (const std::string& note : notes)
        std::printf("%s\n", note.c_str());
    return finish_bench(options, gated);
}
