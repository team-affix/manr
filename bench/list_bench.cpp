// List-program benchmark: samples of programs over integer lists (folds, maps, filters, some with a wrong label),
// under the best-first and MCTS list solvers. A problem is solved when the result is no larger than a known compact
// form (its cost is given, since programs do not parse). Every result is checked by evaluating it at every sample.
//
//   list_bench [--fast] [--write FILE] [--check FILE]
//
// --fast skips MCTS on the large data sets; --write saves the results, --check gates them against a baseline
// (bench/bench_gate.hpp).
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

}

int main(int argc, char** argv) {
    const bench_options options = parse_bench_options(argc, argv);
    const std::vector<list_problem> problems{
        {"sum", all_lists(-2, 2, 3), false, sum, {}, 6, "fold(h + acc, 0, xs)"},
        {"length", all_lists(-2, 2, 3), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer, integer acc) { return acc + 1; }); }, {}, 6,
         "fold(acc + 1, 0, xs)"},
        {"product", all_lists(-2, 2, 3), false,
         [](const list_t& l) { return fold_right(l, 1, [](integer h, integer acc) { return h * acc; }); }, {}, 6,
         "fold(h * acc, 1, xs)"},
        {"sum of squares", all_lists(-2, 2, 3), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return h * h + acc; }); }, {}, 8,
         "fold(h^2 + acc, 0, xs)"},
        {"alternating sum", all_lists(-2, 2, 3), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return h - acc; }); }, {}, 6,
         "fold(h - acc, 0, xs)"},
        {"digits", all_lists(0, 3, 3), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return 10 * acc + h; }); }, {}, 8,
         "fold(10 * acc + h, 0, xs)"},
        {"sum with wrong label", all_lists(-2, 2, 3), false, sum, {{7, 10}}, 14, "pw(case(cons(-1, cons(-2, nil)), 7), fold(h + acc, 0, xs))"},
        {"sum 781", all_lists(-2, 2, 4), false, sum, {}, 6, "fold(h + acc, 0, xs)"},
        {"map square", all_lists(-2, 2, 3), true, [](const list_t& l) { return map_each(l, [](integer e) { return e * e; }); },
         {}, 5, "map(e^2, xs)"},
        {"map affine", all_lists(-2, 2, 3), true,
         [](const list_t& l) { return map_each(l, [](integer e) { return 2 * e + 1; }); }, {}, 7, "map(2 * e + 1, xs)"},
        {"map absolute value", all_lists(-3, 3, 2), true,
         [](const list_t& l) { return map_each(l, [](integer e) { return e < 0 ? integer(-e) : e; }); }, {}, 11,
         "map(pw(when(lt(e, 1), -1 * e), e), xs)"},
        {"map with wrong label", all_lists(-2, 2, 3), true,
         [](const list_t& l) { return map_each(l, [](integer e) { return e * e; }); }, {{3, 5}}, 13,
         "pw(case(cons(0, nil), cons(5, nil)), map(e^2, xs))"},
        {"filter positives", all_lists(-2, 2, 3), true, [](const list_t& l) { return keep(l, [](integer e) { return e > 0; }); },
         {}, 5, "filter(lt(0, e), xs)"},
        {"filter below one", all_lists(-2, 2, 3), true, [](const list_t& l) { return keep(l, [](integer e) { return e < 1; }); },
         {}, 5, "filter(lt(e, 1), xs)"},
        // long lists: 20 to 40 elements, values up to 50 in size; folds get every list's tails, maps and filters none
        {"long sum", with_tails(random_lists(12, 20, 40, -50, 50, 1)), false, sum, {}, 6, "fold(h + acc, 0, xs)"},
        {"long length", with_tails(random_lists(12, 20, 40, -50, 50, 6)), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer, integer acc) { return acc + 1; }); }, {}, 6,
         "fold(acc + 1, 0, xs)"},
        {"long sum of squares", with_tails(random_lists(12, 20, 40, -50, 50, 5)), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return h * h + acc; }); }, {}, 8,
         "fold(h^2 + acc, 0, xs)"},
        {"long product", with_tails(random_lists(12, 20, 40, -9, 9, 2)), false,
         [](const list_t& l) { return fold_right(l, 1, [](integer h, integer acc) { return h * acc; }); }, {}, 6,
         "fold(h * acc, 1, xs)"},
        {"long sum with wrong label", with_tails(random_lists(12, 20, 40, -50, 50, 1)), false, sum, {{3, 100}}, 12,
         "pw(case(cons(v, nil), v + 100), fold(h + acc, 0, xs))"},
        {"long map", random_lists(30, 20, 40, -50, 50, 3), true,
         [](const list_t& l) { return map_each(l, [](integer e) { return 2 * e * e - 3; }); }, {}, 9,
         "map(2 * e^2 - 3, xs)"},
        {"long map absolute value", random_lists(30, 20, 40, -50, 50, 7), true,
         [](const list_t& l) { return map_each(l, [](integer e) { return e < 0 ? integer(-e) : e; }); }, {}, 11,
         "map(pw(when(lt(e, 1), -1 * e), e), xs)"},
        {"long filter", random_lists(30, 20, 40, -50, 50, 4), true,
         [](const list_t& l) { return keep(l, [](integer e) { return e < 17; }); }, {}, 5, "filter(lt(e, 17), xs)"},
        // open: no solver reaches the target yet; kept so the gate reports when one does
        {"maximum (open)", all_lists(0, 3, 3), false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return h < acc ? acc : h; }); },
         {}, 10, "fold(pw(when(lt(acc, h), h), acc), 0, xs)"},
        {"sum without tails", exact_lists(-2, 2, 3), false, sum, {}, 6, "fold(h + acc, 0, xs)"},
        {"long sum without tails", random_lists(40, 20, 40, -50, 50, 8), false, sum, {}, 6,
         "fold(h + acc, 0, xs)"},
        {"squares of positives (open)", all_lists(-2, 2, 3), true,
         [](const list_t& l) { return map_each(keep(l, [](integer e) { return e > 0; }), [](integer e) { return e * e; }); },
         {}, 9, "map(e^2, filter(lt(0, e), xs))"},
    };

    std::printf("budgets: best-first %zu rewrites; mcts %zu simulations x depth %zu\n", k_rewrites, k_simulations,
                k_probing_depth);
    std::printf("cost  *: no larger than target   !: UNSOUND (misses a sample)\n\n");
    std::printf("%-28s %6s %6s | %-15s | %-15s\n", "problem", "lists", "target", "best-1st", "mcts");
    size_t solved[2] = {0, 0};
    std::vector<bench_result> gated;
    std::vector<std::string> notes;
    for (const list_problem& p : problems) {
        const std::vector<list_sample> samples = sample(p);
        const bool skip_mcts = options.fast && samples.size() > k_large;
        const outcome best_first = run_best_first(samples);
        const outcome mcts = skip_mcts ? outcome{.cost = 0, .best = "(skipped)", .seconds = 0, .sound = true}
                                       : run_mcts(samples);
        solved[0] += best_first.cost <= p.target ? 1 : 0;
        solved[1] += !skip_mcts && mcts.cost <= p.target ? 1 : 0;
        std::printf("%-28s %6zu %6zu | %s | %s\n", p.name.c_str(), samples.size(), p.target,
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
    std::printf("\nsolved (of %zu): best-1st %zu, mcts %zu\n\n", problems.size(), solved[0], solved[1]);
    for (const std::string& note : notes)
        std::printf("%s\n", note.c_str());
    return finish_bench(options, gated);
}
