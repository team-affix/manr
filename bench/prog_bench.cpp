// Generic-program benchmark: samples of programs over data of any shape (lists, nested lists, tuples of inputs, trees,
// records, graphs as edge lists), some with holes, under the best-first and MCTS program solvers (`prog_fit`). A
// problem is solved when the result is no larger than a known compact program (its cost is given). Every result is
// checked against every sample: its value there must match the label, under one binding of the labels' holes; at an
// input holding a hole, once the hole is filled with a part of the label or 0 or 1.
//
//   prog_bench [--fast] [--serial] [--full-inputs] [--write FILE] [--check FILE]
//
// --fast skips MCTS on the large data sets; --write saves the results, --check gates them against a baseline
// (bench/bench_gate.hpp). Problems are solved in parallel, one per thread; --serial solves them one at a time.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include <map>
#include <numeric>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include "bench_gate.hpp"
#include "composition/prog_fit_manifest.hpp"
#include "composition/prog_fit_mcts_manifest.hpp"
#include "value_objects/poly_default_limits.hpp"

namespace {

using list_t = std::vector<long>;

struct prog_problem {
    std::string name;
    std::vector<prog_sample> samples;
    size_t target;
    std::string target_text;
};

struct outcome {
    std::string input;
    size_t cost;
    std::string best;
    double seconds;
    bool sound;
};

constexpr size_t k_rewrites = 2000;
constexpr size_t k_frontier = 20000;
constexpr size_t k_probing_depth = 12;
constexpr size_t k_simulations = 150;
constexpr double k_exploration = 0.4;
constexpr size_t k_large = 40;  // samples; --fast skips MCTS above this

std::string show(const list_t& list) {
    std::string text = "[";
    for (size_t i = 0; i < list.size(); ++i)
        text += (i ? ", " : "") + std::to_string(list[i]);
    return text + "]";
}

std::vector<list_t> random_lists(size_t count, size_t shortest, size_t longest, int low, int high, unsigned seed) {
    std::mt19937 rng(seed);
    std::vector<list_t> lists;
    for (size_t i = 0; i < count; ++i) {
        list_t list(std::uniform_int_distribution<size_t>(shortest, longest)(rng));
        for (long& value : list)
            value = std::uniform_int_distribution<int>(low, high)(rng);
        lists.push_back(list);
    }
    return lists;
}

prog_problem from_lists(const std::string& name, const std::vector<list_t>& inputs,
                        const std::function<std::string(const list_t&)>& program, size_t target,
                        const std::string& target_text, const std::vector<prog_sample>& extra) {
    prog_problem p{.name = name, .samples = {}, .target = target, .target_text = target_text};
    for (const list_t& list : inputs)
        p.samples.push_back(prog_sample{.input = show(list), .output = program(list)});
    p.samples.insert(p.samples.end(), extra.begin(), extra.end());
    return p;
}

std::string sorted(const list_t& list) {
    list_t copy = list;
    std::sort(copy.begin(), copy.end());
    return show(copy);
}

std::string prefix_sums(const list_t& list) {
    list_t sums;
    long running = 0;
    for (const long value : list)
        sums.push_back(running += value);
    return show(sums);
}

// trees: leaf | node(left, value, right), as index-linked nodes (-1 for a leaf)
struct tree_node {
    int value;
    int left;
    int right;
};

int grow_tree(std::mt19937& rng, std::vector<tree_node>& tree, int budget) {
    if (budget <= 0)
        return -1;
    const int left_budget = std::uniform_int_distribution<int>(0, budget - 1)(rng);
    const int id = static_cast<int>(tree.size());
    tree.push_back(tree_node{.value = std::uniform_int_distribution<int>(-5, 9)(rng), .left = -1, .right = -1});
    const int left = grow_tree(rng, tree, left_budget);
    const int right = grow_tree(rng, tree, budget - 1 - left_budget);
    tree[id].left = left;
    tree[id].right = right;
    return id;
}

std::string tree_text(const std::vector<tree_node>& tree, int at, bool mirrored) {
    if (at < 0)
        return "leaf";
    const int first = mirrored ? tree[at].right : tree[at].left;
    const int second = mirrored ? tree[at].left : tree[at].right;
    return "node(" + tree_text(tree, first, mirrored) + ", " + std::to_string(tree[at].value) + ", "
           + tree_text(tree, second, mirrored) + ")";
}

long tree_sum(const std::vector<tree_node>& tree, int at) {
    return at < 0 ? 0 : tree[at].value + tree_sum(tree, tree[at].left) + tree_sum(tree, tree[at].right);
}

long tree_depth(const std::vector<tree_node>& tree, int at) {
    return at < 0 ? 0 : 1 + std::max(tree_depth(tree, tree[at].left), tree_depth(tree, tree[at].right));
}

long tree_size(const std::vector<tree_node>& tree, int at) {
    return at < 0 ? 0 : 1 + tree_size(tree, tree[at].left) + tree_size(tree, tree[at].right);
}

void traverse(const std::vector<tree_node>& tree, int at, bool pre, list_t& out) {
    if (at < 0)
        return;
    if (pre)
        out.push_back(tree[at].value);
    traverse(tree, tree[at].left, pre, out);
    if (!pre)
        out.push_back(tree[at].value);
    traverse(tree, tree[at].right, pre, out);
}

// the tree problems, all over the same 30 random trees
std::vector<prog_problem> tree_problems() {
    std::mt19937 rng(11);
    prog_problem sum{"#67 tree sum", {}, 11, "lam(tfold(lams(3, #2 + #1 + #0), 0, #0))"};
    prog_problem depth{"tree depth", {}, 11, "lam(tfold(lams(3, max(#2, #0) + 1), 0, #0))"};
    prog_problem size{"tree size", {}, 11, "lam(tfold(lams(3, #2 + #0 + 1), 0, #0))"};
    prog_problem mirror{"#68 tree mirror", {}, 10, "lam(tfold(lams(3, node(#0, #1, #2)), leaf, #0))"};
    prog_problem in_order{"#69 tree in-order traversal", {}, 11, "lam(tfold(lams(3, append(#2, cons(#1, #0))), nil, #0))"};
    prog_problem pre_order{"tree pre-order traversal", {}, 11, "lam(tfold(lams(3, cons(#1, append(#2, #0))), nil, #0))"};
    prog_problem holed{"tree sum, a value ?1", {}, 11, "lam(tfold(lams(3, #2 + #1 + #0), 0, #0)), ?1 = 4"};
    for (int i = 0; i < 30; ++i) {
        std::vector<tree_node> tree;
        const int root = grow_tree(rng, tree, std::uniform_int_distribution<int>(0, 6)(rng));
        const std::string text = tree_text(tree, root, false);
        list_t in;
        list_t pre;
        traverse(tree, root, false, in);
        traverse(tree, root, true, pre);
        sum.samples.push_back({text, std::to_string(tree_sum(tree, root))});
        depth.samples.push_back({text, std::to_string(tree_depth(tree, root))});
        size.samples.push_back({text, std::to_string(tree_size(tree, root))});
        mirror.samples.push_back({text, tree_text(tree, root, true)});
        in_order.samples.push_back({text, show(in)});
        pre_order.samples.push_back({text, show(pre)});
        holed.samples.push_back({text, std::to_string(tree_sum(tree, root))});
    }
    holed.samples.push_back({"node(node(leaf, 1, leaf), ?1, leaf)", "5"});
    return {sum, depth, size, mirror, in_order, pre_order, holed};
}

// graphs as lists of edges over the nodes 0 to 4
std::vector<prog_problem> graph_problems() {
    std::mt19937 rng(12);
    prog_problem out{"#70 out-degree of node 0", {}, 10, "lam(length(filter(lam(eq(field(0, #0), 0)), #0)))"};
    prog_problem in{"in-degree of node 0", {}, 10, "lam(length(filter(lam(eq(field(1, #0), 0)), #0)))"};
    prog_problem reach{"#71 is node 4 reachable from 0", {}, 6, "lam(elem(4, reachable(#0, 0)))"};
    for (int i = 0; i < 30; ++i) {
        const int count = std::uniform_int_distribution<int>(0, 6)(rng);
        std::vector<std::pair<int, int>> edges;
        std::string text = "[";
        for (int e = 0; e < count; ++e) {
            const int from = std::uniform_int_distribution<int>(0, 4)(rng);
            const int to = std::uniform_int_distribution<int>(0, 4)(rng);
            edges.emplace_back(from, to);
            text += (e ? ", " : "") + std::string("edge(") + std::to_string(from) + ", " + std::to_string(to) + ")";
        }
        text += "]";
        long outgoing = 0;
        long incoming = 0;
        for (const auto& [from, to] : edges) {
            outgoing += from == 0;
            incoming += to == 0;
        }
        std::set<int> seen{0};
        for (bool grew = true; grew;) {
            grew = false;
            for (const auto& [from, to] : edges) {
                if (!seen.contains(from) || seen.contains(to))
                    continue;
                seen.insert(to);
                grew = true;
            }
        }
        out.samples.push_back({text, std::to_string(outgoing)});
        in.samples.push_back({text, std::to_string(incoming)});
        reach.samples.push_back({text, seen.contains(4) ? "1" : "0"});
    }
    return {out, in, reach};
}

// lists of lists, and pairs of inputs
std::vector<prog_problem> nested_problems() {
    std::mt19937 rng(10);
    prog_problem flatten{"#65 flatten a list of lists", {}, 9, "lam(fold(lam(lam(append(#0, #1))), nil, #0))"};
    prog_problem sums{"#66 sum of each sublist", {}, 12, "lam(map(lam(fold(lam(lam(#0 + #1)), 0, #0)), #0))"};
    prog_problem lengths{"length of each sublist", {}, 6, "lam(map(lam(length(#0)), #0))"};
    prog_problem holed{"flatten, an inner list ?1", {}, 9, "lam(fold(lam(lam(append(#0, #1))), nil, #0)), ?1 = [4]"};
    for (int i = 0; i < 30; ++i) {
        const std::vector<list_t> lists =
            random_lists(std::uniform_int_distribution<size_t>(0, 3)(rng), 0, 3, -5, 9, 100 + i);
        std::string text = "[";
        list_t flat;
        list_t totals;
        list_t counts;
        for (size_t j = 0; j < lists.size(); ++j) {
            text += (j ? ", " : "") + show(lists[j]);
            flat.insert(flat.end(), lists[j].begin(), lists[j].end());
            totals.push_back(std::accumulate(lists[j].begin(), lists[j].end(), 0L));
            counts.push_back(static_cast<long>(lists[j].size()));
        }
        text += "]";
        flatten.samples.push_back({text, show(flat)});
        sums.samples.push_back({text, show(totals)});
        lengths.samples.push_back({text, show(counts)});
        holed.samples.push_back({text, show(flat)});
    }
    holed.samples.push_back({"[[1, 2], ?1, [5]]", "[1, 2, 4, 5]"});
    prog_problem joined{"#64 append two lists", {}, 5, "lams(2, append(#1, #0))"};
    const std::vector<list_t> firsts = random_lists(30, 0, 4, -5, 9, 5);
    const std::vector<list_t> seconds = random_lists(30, 0, 4, -5, 9, 6);
    for (size_t i = 0; i < firsts.size(); ++i) {
        list_t both = firsts[i];
        both.insert(both.end(), seconds[i].begin(), seconds[i].end());
        joined.samples.push_back({"point(" + show(firsts[i]) + ", " + show(seconds[i]) + ")", show(both)});
    }
    prog_problem dot{"dot product of two lists", {}, 17,
                     "lams(2, fold(lam(lam(#0 + #1)), 0, zipwith(lam(lam(#0 * #1)), #1, #0)))"};
    for (const list_t& left : random_lists(30, 0, 4, -5, 9, 7)) {
        const list_t right = random_lists(1, left.size(), left.size(), -5, 9, 13 * left.size() + 1)[0];
        long product = 0;
        for (size_t i = 0; i < left.size(); ++i)
            product += left[i] * right[i];
        dot.samples.push_back({"point(" + show(left) + ", " + show(right) + ")", std::to_string(product)});
    }
    prog_problem member{"membership of a value in a list", {}, 5, "lams(2, elem(#1, #0))"};
    std::mt19937 values(9);
    for (const list_t& list : random_lists(40, 0, 5, 0, 6, 8)) {
        const int value = std::uniform_int_distribution<int>(0, 6)(values);
        const bool found = std::find(list.begin(), list.end(), value) != list.end();
        member.samples.push_back({"point(" + std::to_string(value) + ", " + show(list) + ")", found ? "1" : "0"});
    }
    return {flatten, sums, lengths, holed, joined, dot, member};
}

std::vector<prog_problem> list_problems() {
    const std::vector<list_t> lists = random_lists(40, 0, 5, -5, 9, 1);
    const std::vector<list_t> nonempty = random_lists(40, 1, 5, -5, 9, 3);
    std::vector<prog_problem> problems;
    problems.push_back(from_lists("sum (control)", lists, [](const list_t& l) {
        return std::to_string(std::accumulate(l.begin(), l.end(), 0L)); }, 9, "lam(fold(lam(lam(#0 + #1)), 0, #0))", {}));
    problems.push_back(from_lists("#56 sort", lists, sorted, 3, "lam(sort(#0))", {}));
    problems.push_back(from_lists("sort, an input element ?1", lists, sorted, 3, "lam(sort(#0)), ?1 = 2",
                                  {{"[3, ?1, 1]", "[1, 2, 3]"}}));
    problems.push_back(from_lists("sort descending", lists, [](const list_t& l) {
        list_t copy = l; std::sort(copy.rbegin(), copy.rend()); return show(copy); }, 4, "lam(reverse(sort(#0)))", {}));
    problems.push_back(from_lists("#57 prefix sums", lists, prefix_sums, 9, "lam(scan(lam(lam(#0 + #1)), 0, #0))", {}));
    problems.push_back(from_lists("prefix sums, an output ?1", lists, prefix_sums, 9,
                                  "lam(scan(lam(lam(#0 + #1)), 0, #0)), ?1 = 3", {{"[1, 2, 3]", "[1, ?1, 6]"}}));
    problems.push_back(from_lists("running maximum", nonempty, [](const list_t& l) {
        list_t out; long best = l[0]; for (long v : l) out.push_back(best = std::max(best, v)); return show(out); },
        10, "lam(scan(lam(lam(max(#0, #1))), head(#0), #0))", {}));
    problems.push_back(from_lists("#58 remove duplicates", random_lists(40, 0, 6, 0, 4, 2), [](const list_t& l) {
        list_t out; for (long v : l) if (std::find(out.begin(), out.end(), v) == out.end()) out.push_back(v);
        return show(out); }, 3, "lam(unique(#0)), or the fold lam(fold(lam(lam(cons(#0, filter(lam(not(eq(#0, #1))), #1)))), nil, #0)) (15)", {}));
    problems.push_back(from_lists("#59 last element", nonempty, [](const list_t& l) { return std::to_string(l.back()); },
                                  4, "lam(head(reverse(#0)))", {}));
    problems.push_back(from_lists("#60 pairwise differences", random_lists(40, 1, 6, -5, 9, 4), [](const list_t& l) {
        list_t out; for (size_t i = 1; i < l.size(); ++i) out.push_back(l[i] - l[i - 1]); return show(out); },
        10, "lam(zipwith(lam(lam(#0 - #1)), #0, tail(#0)))", {}));
    problems.push_back(from_lists("#61 sum times length", lists, [](const list_t& l) {
        return std::to_string(std::accumulate(l.begin(), l.end(), 0L) * static_cast<long>(l.size())); },
        12, "lam(length(#0) * fold(lam(lam(#0 + #1)), 0, #0))", {}));
    problems.push_back(from_lists("maximum minus minimum", nonempty, [](const list_t& l) {
        return std::to_string(*std::max_element(l.begin(), l.end()) - *std::min_element(l.begin(), l.end())); },
        10, "lam(head(reverse(sort(#0))) - head(sort(#0)))", {}));
    problems.push_back(from_lists("#62 reverse of squares", lists, [](const list_t& l) {
        list_t out; for (auto it = l.rbegin(); it != l.rend(); ++it) out.push_back(*it * *it); return show(out); },
        8, "lam(reverse(map(lam(#0 * #0), #0)))", {}));
    problems.push_back(from_lists("#63 is sorted", lists, [](const list_t& l) {
        return std::string(std::is_sorted(l.begin(), l.end()) ? "1" : "0"); }, 5, "lam(eq(sort(#0), #0))", {}));
    problems.push_back(from_lists("random labels stay (control)", lists, [](const list_t& l) {
        return std::to_string((l.size() * 7919 + (l.empty() ? 3 : l[0] * 31)) % 23); }, 10000,
        "no small program", {}));
    return problems;
}

// A result is sound when every sample matches its value under one binding of the labels' holes; at an input holding
// a hole, once the hole is filled with one of the label's parts, 0 or 1.
template<typename Manifest>
bool reproduces(Manifest& m, const expr* best) {
    std::map<uint32_t, const expr*> bindings;
    for (const pw_data_point& point : m.rule_set_.points_) {
        std::vector<const expr*> fillers{nullptr, m.kernel_.literals_.make_literal(0), m.kernel_.literals_.make_literal(1)};
        std::vector<const expr*> stack{point.output};
        while (!stack.empty()) {
            const expr* at = stack.back();
            stack.pop_back();
            fillers.push_back(at);
            if (const expr::functor* f = std::get_if<expr::functor>(&at->content))
                stack.insert(stack.end(), f->args.begin(), f->args.end());
        }
        bool matched = false;
        for (const expr* filler : fillers) {
            // first the input as it is (a kept case), then its holes filled
            std::map<uint32_t, const expr*> filled;
            for (uint32_t hole = 0; hole < 10 && filler != nullptr; ++hole)
                filled.emplace(hole, filler);
            const expr* input = m.rule_set_.piecewise_.matcher_.substitute_holes(point.input, filled);
            const std::optional<const expr*> value = m.rule_set_.pw_evaluator_.evaluate_at(best, input);
            std::map<uint32_t, const expr*> tried = bindings;
            if (!value.has_value() || !m.rule_set_.piecewise_.matcher_.match(point.output, value.value(), tried))
                continue;
            bindings = tried;
            matched = true;
            break;
        }
        if (!matched)
            return false;
    }
    return true;
}

template<typename Manifest, typename Search>
outcome finish(Manifest& m, Search& search, std::chrono::steady_clock::time_point start) {
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::ostringstream input;
    std::ostringstream os;
    m.kernel_.printer_.print(input, m.rule_set_.initial_);
    m.kernel_.printer_.print(os, search.best());
    return outcome{.input = input.str(), .cost = search.best_cost(), .best = os.str(), .seconds = seconds,
                   .sound = reproduces(m, search.best())};
}

outcome run_best_first(const prog_problem& p) {
    const auto start = std::chrono::steady_clock::now();
    prog_fit_manifest m{p.samples, k_poly_default_limits, k_frontier};
    while (m.search_.rewrites() < k_rewrites && m.search_.next()) {}
    return finish(m, m.search_, start);
}

outcome run_mcts(const prog_problem& p) {
    const auto start = std::chrono::steady_clock::now();
    prog_fit_mcts_manifest m{p.samples, k_poly_default_limits, k_probing_depth, k_simulations, k_exploration, 1};
    while (m.search_.next()) {}
    return finish(m, m.search_, start);
}

std::string cell(const outcome& o, size_t target) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%5zu%s %6.2fs", o.cost, !o.sound ? "!" : o.cost <= target ? "*" : " ",
                  o.seconds);
    return buffer;
}

struct solved {
    bool skip_mcts;
    outcome best_first;
    outcome mcts;
};

solved solve(const prog_problem& p, bool fast) {
    const bool skip_mcts = fast && p.samples.size() > k_large;
    const outcome skipped{.input = "", .cost = 0, .best = "(skipped)", .seconds = 0, .sound = true};
    return solved{.skip_mcts = skip_mcts, .best_first = run_best_first(p), .mcts = skip_mcts ? skipped : run_mcts(p)};
}

}

int main(int argc, char** argv) {
    const bench_options options = parse_bench_options(argc, argv);
    std::vector<prog_problem> problems = list_problems();
    for (const std::vector<prog_problem>& group : {nested_problems(), tree_problems(), graph_problems()})
        problems.insert(problems.end(), group.begin(), group.end());

    std::printf("budgets: best-first %zu rewrites; mcts %zu simulations x depth %zu\n", k_rewrites, k_simulations,
                k_probing_depth);
    std::printf("cost  *: no larger than target   !: UNSOUND (misses a sample)\n\n");
    std::printf("%-36s %6s %6s | %-15s | %-15s\n", "problem", "points", "target", "best-1st", "mcts");
    size_t solved_counts[2] = {0, 0};
    std::vector<bench_result> gated;
    std::vector<std::string> notes;
    const std::vector<solved> runs = solve_in_parallel(problems, solve, options);
    for (size_t index = 0; index < problems.size(); ++index) {
        const prog_problem& p = problems[index];
        const solved& run = runs[index];
        solved_counts[0] += run.best_first.cost <= p.target ? 1 : 0;
        solved_counts[1] += !run.skip_mcts && run.mcts.cost <= p.target ? 1 : 0;
        std::printf("%-36s %6zu %6zu | %s | %s\n", p.name.c_str(), p.samples.size(), p.target,
                    cell(run.best_first, p.target).c_str(),
                    run.skip_mcts ? "      (skipped)" : cell(run.mcts, p.target).c_str());
        std::fflush(stdout);
        gated.push_back(bench_result{.problem = p.name, .solver = "best-1st", .cost = run.best_first.cost,
                                     .target = p.target, .seconds = run.best_first.seconds,
                                     .sound = run.best_first.sound});
        if (!run.skip_mcts)
            gated.push_back(bench_result{.problem = p.name, .solver = "mcts", .cost = run.mcts.cost, .target = p.target,
                                         .seconds = run.mcts.seconds, .sound = run.mcts.sound});
        const std::string input = options.full_inputs ? run.best_first.input : abbreviate_piecewise(run.best_first.input);
        notes.push_back(p.name + "\n    input:    " + input + "\n    target:   " + p.target_text
                        + "\n    best-1st: " + abbreviate_piecewise(run.best_first.best)
                        + "\n    mcts:     " + abbreviate_piecewise(run.mcts.best));
    }
    std::printf("\nsolved (of %zu): best-1st %zu, mcts %zu\n\n", problems.size(), solved_counts[0], solved_counts[1]);
    for (const std::string& note : notes)
        std::printf("%s\n", note.c_str());
    return finish_bench(options, gated);
}
