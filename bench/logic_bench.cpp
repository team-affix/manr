// Logic benchmark: boolean functions given by their truth tables, some with rows missing (unseen inputs are free) or a
// wrong label, under the best-first and MCTS logic solvers. A problem is solved when the result is no larger than a
// known compact form (its cost is given, since formulas do not parse). Every result is checked by evaluating it at
// every sample.
//
//   logic_bench [--serial] [--write FILE] [--check FILE]
//
// --write saves the results, --check gates them against a baseline (bench/bench_gate.hpp). Every problem is small, so
// --fast changes nothing.
// Problems are solved in parallel, one per thread; --serial solves them one at a time, for true timings.
#include <chrono>
#include <cstdio>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "infrastructure/logic_fit_manifest.hpp"
#include "infrastructure/logic_fit_mcts_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "bench_gate.hpp"

namespace {

using truth_t = int (*)(const std::vector<int>&);

// the truth table of `truth` over every input, without the rows in `missing`, with the rows in `flipped` negated
struct logic_problem {
    std::string name;
    std::vector<std::string> variables;
    truth_t truth;
    std::set<size_t> missing;
    std::set<size_t> flipped;
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

int ones(const std::vector<int>& bits) {
    int count = 0;
    for (const int bit : bits)
        count += bit;
    return count;
}

int parity(const std::vector<int>& bits) {
    return ones(bits) % 2;
}

int majority(const std::vector<int>& bits) {
    return 2 * ones(bits) > static_cast<int>(bits.size()) ? 1 : 0;
}

int conjunction(const std::vector<int>& bits) {
    return ones(bits) == static_cast<int>(bits.size()) ? 1 : 0;
}

int exactly_one(const std::vector<int>& bits) {
    return ones(bits) == 1 ? 1 : 0;
}

int implication(const std::vector<int>& bits) {
    return bits[0] == 0 || bits[1] == 1 ? 1 : 0;
}

int equivalence(const std::vector<int>& bits) {
    return bits[0] == bits[1] ? 1 : 0;
}

// s ? a : b, with the inputs (s, a, b)
int multiplexer(const std::vector<int>& bits) {
    return bits[0] == 1 ? bits[1] : bits[2];
}

std::vector<poly_sample> sample(const logic_problem& p) {
    std::vector<poly_sample> samples;
    const size_t rows = size_t(1) << p.variables.size();
    for (size_t row = 0; row < rows; ++row) {
        if (p.missing.contains(row))
            continue;
        std::vector<int> bits;
        for (size_t position = 0; position < p.variables.size(); ++position)
            bits.push_back((row >> position) & 1 ? 1 : 0);
        poly_sample s{.inputs = {}, .output = p.flipped.contains(row) ? 1 - p.truth(bits) : p.truth(bits)};
        for (const int bit : bits)
            s.inputs.push_back(bit);
        samples.push_back(s);
    }
    return samples;
}

template<typename Manifest>
outcome finish(Manifest& m, const std::vector<poly_sample>& samples, std::chrono::steady_clock::time_point start) {
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

outcome run_best_first(const logic_problem& p, const std::vector<poly_sample>& samples) {
    const auto start = std::chrono::steady_clock::now();
    logic_fit_manifest m{p.variables.size(), samples, k_poly_default_limits, k_frontier};
    while (m.search_.rewrites() < k_rewrites && m.search_.next()) {}
    return finish(m, samples, start);
}

outcome run_mcts(const logic_problem& p, const std::vector<poly_sample>& samples) {
    const auto start = std::chrono::steady_clock::now();
    logic_fit_mcts_manifest m{p.variables.size(), samples, k_poly_default_limits, k_probing_depth, k_simulations,
                              k_exploration, 1};
    while (m.search_.next()) {}
    return finish(m, samples, start);
}

std::string cell(const outcome& o, size_t target) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%5zu%s %6.2fs", o.cost, !o.sound ? "!" : o.cost <= target ? "*" : " ",
                  o.seconds);
    return buffer;
}

// one problem under both solvers
struct solved {
    size_t points;
    outcome best_first;
    outcome mcts;
};

// every problem is small, so `fast` changes nothing
solved solve(const logic_problem& p, bool) {
    const std::vector<poly_sample> samples = sample(p);
    return solved{.points = samples.size(), .best_first = run_best_first(p, samples), .mcts = run_mcts(p, samples)};
}

}

int main(int argc, char** argv) {
    const bench_options options = parse_bench_options(argc, argv);
    const std::vector<logic_problem> problems{
        {"#9 parity of 4 bits", {"a", "b", "c", "d"}, parity, {}, {}, 9, "lams(4, xor(xor(#3, #2), xor(#1, #0)))"},
        {"#10 majority of 3 bits", {"a", "b", "c"}, majority, {}, {}, 11, "lams(3, or(and(#2, #1), and(#0, or(#2, #1))))"},
        {"parity of 3 bits", {"a", "b", "c"}, parity, {}, {}, 7, "lams(3, xor(xor(#2, #1), #0))"},
        {"parity of 6 bits", {"a", "b", "c", "d", "e", "f"}, parity, {}, {}, 13, "lams(6, xor of six)"},
        {"equivalence", {"a", "b"}, equivalence, {}, {}, 6, "lams(2, not(xor(#1, #0)))"},
        {"implication", {"a", "b"}, implication, {}, {}, 6, "lams(2, or(not(#1), #0))"},
        {"and of 3 bits", {"a", "b", "c"}, conjunction, {}, {}, 7, "lams(3, and(and(#2, #1), #0))"},
        {"multiplexer", {"s", "a", "b"}, multiplexer, {}, {}, 10, "lams(3, or(and(#2, #1), and(not(#2), #0)))"},
        {"majority, two rows unseen", {"a", "b", "c"}, majority, {0, 7}, {}, 11, "lams(3, or(and(#2, #1), and(#0, or(#2, #1))))"},
        {"parity of 4, wrong label", {"a", "b", "c", "d"}, parity, {}, {5}, 17,
         "pw(case(point(1, 0, 1, 0), 1), lams(4, xor(xor(#3, #2), xor(#1, #0))))"},
        {"exactly one of 3", {"a", "b", "c"}, exactly_one, {}, {}, 14,
         "pw(case(point(1, 1, 1), 0), lams(3, xor(xor(#2, #1), #0)))"},
        {"majority of 5 bits", {"a", "b", "c", "d", "e"}, majority, {}, {}, 39,
         "the factored sum of the 10 triples; shares maj(#2, #1, #0) twice, which helpers would compress"},
    };

    std::printf("budgets: best-first %zu rewrites; mcts %zu simulations x depth %zu\n", k_rewrites, k_simulations,
                k_probing_depth);
    std::printf("cost  *: no larger than target   !: UNSOUND (misses a sample)\n\n");
    std::printf("%-30s %6s %6s | %-15s | %-15s\n", "problem", "points", "target", "best-1st", "mcts");
    size_t solved_counts[2] = {0, 0};
    std::vector<bench_result> gated;
    std::vector<std::string> notes;
    const std::vector<solved> runs = solve_in_parallel(problems, solve, options);
    for (size_t index = 0; index < problems.size(); ++index) {
        const logic_problem& p = problems[index];
        const outcome& best_first = runs[index].best_first;
        const outcome& mcts = runs[index].mcts;
        solved_counts[0] += best_first.cost <= p.target ? 1 : 0;
        solved_counts[1] += mcts.cost <= p.target ? 1 : 0;
        std::printf("%-30s %6zu %6zu | %s | %s\n", p.name.c_str(), runs[index].points, p.target,
                    cell(best_first, p.target).c_str(), cell(mcts, p.target).c_str());
        std::fflush(stdout);
        gated.push_back(bench_result{.problem = p.name, .solver = "best-1st", .cost = best_first.cost,
                                     .target = p.target, .seconds = best_first.seconds, .sound = best_first.sound});
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
