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
    int low;
    int high;
    size_t max_length;
    size_t exact_length;  // when nonzero, only lists of this length (no tails, no empty list)
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

std::vector<list_sample> sample(const list_problem& p) {
    std::vector<list_sample> samples;
    for (const list_t& list : all_lists(p.low, p.high, p.max_length)) {
        if (p.exact_length != 0 && list.size() != p.exact_length)
            continue;
        samples.push_back(list_sample{.input = list, .output = p.program(list), .output_is_list = p.output_is_list});
    }
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
        {"sum", -2, 2, 3, 0, false, sum, {}, 6, "fold(h + acc, 0, xs)"},
        {"length", -2, 2, 3, 0, false,
         [](const list_t& l) { return fold_right(l, 0, [](integer, integer acc) { return acc + 1; }); }, {}, 6,
         "fold(acc + 1, 0, xs)"},
        {"product", -2, 2, 3, 0, false,
         [](const list_t& l) { return fold_right(l, 1, [](integer h, integer acc) { return h * acc; }); }, {}, 6,
         "fold(h * acc, 1, xs)"},
        {"sum of squares", -2, 2, 3, 0, false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return h * h + acc; }); }, {}, 8,
         "fold(h^2 + acc, 0, xs)"},
        {"alternating sum", -2, 2, 3, 0, false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return h - acc; }); }, {}, 6,
         "fold(h - acc, 0, xs)"},
        {"digits", 0, 3, 3, 0, false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return 10 * acc + h; }); }, {}, 8,
         "fold(10 * acc + h, 0, xs)"},
        {"sum with wrong label", -2, 2, 3, 0, false, sum, {{7, 10}}, 14, "pw(case(cons(-1, cons(-2, nil)), 7), fold(h + acc, 0, xs))"},
        {"sum 781", -2, 2, 4, 0, false, sum, {}, 6, "fold(h + acc, 0, xs)"},
        {"map square", -2, 2, 3, 0, true, [](const list_t& l) { return map_each(l, [](integer e) { return e * e; }); },
         {}, 5, "map(e^2, xs)"},
        {"map affine", -2, 2, 3, 0, true,
         [](const list_t& l) { return map_each(l, [](integer e) { return 2 * e + 1; }); }, {}, 7, "map(2 * e + 1, xs)"},
        {"map absolute value", -3, 3, 2, 0, true,
         [](const list_t& l) { return map_each(l, [](integer e) { return e < 0 ? integer(-e) : e; }); }, {}, 11,
         "map(pw(when(lt(e, 1), -1 * e), e), xs)"},
        {"map with wrong label", -2, 2, 3, 0, true,
         [](const list_t& l) { return map_each(l, [](integer e) { return e * e; }); }, {{3, 5}}, 13,
         "pw(case(cons(0, nil), cons(5, nil)), map(e^2, xs))"},
        {"filter positives", -2, 2, 3, 0, true, [](const list_t& l) { return keep(l, [](integer e) { return e > 0; }); },
         {}, 5, "filter(lt(0, e), xs)"},
        {"filter below one", -2, 2, 3, 0, true, [](const list_t& l) { return keep(l, [](integer e) { return e < 1; }); },
         {}, 5, "filter(lt(e, 1), xs)"},
        // open: no solver reaches the target yet; kept so the gate reports when one does
        {"maximum (open)", 0, 3, 3, 0, false,
         [](const list_t& l) { return fold_right(l, 0, [](integer h, integer acc) { return h < acc ? acc : h; }); },
         {}, 10, "fold(pw(when(lt(acc, h), h), acc), 0, xs)"},
        {"sum without tails (open)", -2, 2, 3, 3, false, sum, {}, 6, "fold(h + acc, 0, xs)"},
        {"squares of positives (open)", -2, 2, 3, 0, true,
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
