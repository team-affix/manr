// The regression gate shared by the benchmarks. A run's results are written as one tab-separated line per
// (problem, solver): cost, seconds, sound. `check_results` compares a run against a committed baseline:
//
//   hard (exit 1):  any unsound result; any cost larger than the baseline's for the same problem and solver
//   reported:       costs that improved, problems solved for the first time, and results slower than the baseline
//                   by more than k_slower_ratio (timings vary by machine and load, so they warn, never fail)
//
// Costs are deterministic (best-first, seeded MCTS), so any cost change is a real change.
#ifndef BENCH_GATE_HPP
#define BENCH_GATE_HPP

#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

struct bench_result {
    std::string problem;
    std::string solver;
    size_t cost;
    size_t target;
    double seconds;
    bool sound;
};

constexpr double k_slower_ratio = 1.5;
constexpr double k_slower_floor = 0.25;  // seconds: differences below this are noise

struct bench_options {
    bool fast;
    std::string write_path;
    std::string check_path;
};

inline bench_options parse_bench_options(int argc, char** argv) {
    bench_options options{.fast = false, .write_path = "", .check_path = ""};
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--fast")
            options.fast = true;
        else if (arg == "--write" && index + 1 < argc)
            options.write_path = argv[++index];
        else if (arg == "--check" && index + 1 < argc)
            options.check_path = argv[++index];
        else
            std::fprintf(stderr, "unknown argument: %s\n", arg.c_str());
    }
    return options;
}

inline void write_results(const std::string& path, const std::vector<bench_result>& results) {
    std::ofstream out(path);
    out << "# problem\tsolver\tcost\ttarget\tseconds\tsound\n";
    for (const bench_result& result : results)
        out << result.problem << '\t' << result.solver << '\t' << result.cost << '\t' << result.target << '\t'
            << result.seconds << '\t' << (result.sound ? 1 : 0) << '\n';
}

inline std::map<std::pair<std::string, std::string>, bench_result> read_results(const std::string& path) {
    std::map<std::pair<std::string, std::string>, bench_result> results;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream fields(line);
        bench_result result{};
        std::string cost, target, seconds, sound;
        std::getline(fields, result.problem, '\t');
        std::getline(fields, result.solver, '\t');
        std::getline(fields, cost, '\t');
        std::getline(fields, target, '\t');
        std::getline(fields, seconds, '\t');
        std::getline(fields, sound, '\t');
        result.cost = std::stoul(cost);
        result.target = std::stoul(target);
        result.seconds = std::stod(seconds);
        result.sound = sound == "1";
        results.emplace(std::make_pair(result.problem, result.solver), result);
    }
    return results;
}

// Prints the comparison and returns the number of hard failures. Results missing from the baseline are new
// problems or solvers; baseline entries missing from the run (e.g. under --fast) are skipped.
inline size_t check_results(const std::string& path, const std::vector<bench_result>& results) {
    const auto baseline = read_results(path);
    if (baseline.empty()) {
        std::printf("gate: no baseline at %s\n", path.c_str());
        return 1;
    }
    size_t failures = 0;
    size_t compared = 0;
    std::printf("\ngate against %s\n", path.c_str());
    for (const bench_result& result : results) {
        const std::string name = result.problem + " / " + result.solver;
        if (!result.sound) {
            std::printf("  FAIL unsound   %s\n", name.c_str());
            ++failures;
        }
        const auto found = baseline.find(std::make_pair(result.problem, result.solver));
        if (found == baseline.end()) {
            std::printf("  new            %s: cost %zu (target %zu)\n", name.c_str(), result.cost, result.target);
            continue;
        }
        ++compared;
        const bench_result& before = found->second;
        if (result.cost > before.cost) {
            std::printf("  FAIL worse     %s: cost %zu -> %zu (target %zu)\n", name.c_str(), before.cost, result.cost,
                        result.target);
            ++failures;
        }
        if (result.cost < before.cost)
            std::printf("  better         %s: cost %zu -> %zu%s\n", name.c_str(), before.cost, result.cost,
                        before.cost > result.target && result.cost <= result.target ? "  (newly solved)" : "");
        const bool slower =
            result.seconds > before.seconds * k_slower_ratio && result.seconds - before.seconds > k_slower_floor;
        if (slower)
            std::printf("  warn slower    %s: %.2fs -> %.2fs\n", name.c_str(), before.seconds, result.seconds);
    }
    std::printf("gate: %zu compared, %zu failure%s\n", compared, failures, failures == 1 ? "" : "s");
    return failures;
}

// Writes and checks as the options ask; the process exit code.
inline int finish_bench(const bench_options& options, const std::vector<bench_result>& results) {
    if (!options.write_path.empty()) {
        write_results(options.write_path, results);
        std::printf("wrote %s\n", options.write_path.c_str());
    }
    if (options.check_path.empty())
        return 0;
    return check_results(options.check_path, results) == 0 ? 0 : 1;
}

#endif
