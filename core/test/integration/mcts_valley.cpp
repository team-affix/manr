#include <cstdint>
#include <random>
#include <unordered_map>
#include <variant>
#include <vector>
#include <gtest/gtest.h>
#include "infrastructure/best_first_search.hpp"
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/mcts_path_walker.hpp"
#include "infrastructure/mcts_search.hpp"
#include "infrastructure/uct_value_manifest.hpp"
#include "infrastructure/value_table.hpp"
#include "infrastructure/visits_table.hpp"

// A reduction that sits behind expansions. Expressions are `g(g(...g(c_k)))`. The rules are:
//   grow:     wrap the expression in one more `g` (cost + 1)
//   step:     c_k -> c_(k+1 mod plateau) at the leaf (same cost: a large plateau at every depth)
//   collapse: at exactly `collapse_depth` g's, the whole expression -> `w` (cost 1)
// Ordering by cost makes best-first walk every plateau before it climbs; probing simulations see the collapse.
namespace {

constexpr uint32_t k_g = 0;
constexpr uint32_t k_w = 1;
constexpr uint32_t k_first_leaf = 2;

struct valley_rules {
    valley_rules(expr_pool& pool, uint32_t plateau, size_t collapse_depth)
        : pool_(pool), plateau_(plateau), collapse_depth_(collapse_depth) {}

    void propose(const expr* e, std::vector<const expr*>& out) {
        size_t depth = 0;
        const expr* leaf = e;
        while (std::get<expr::functor>(leaf->content).id == k_g) {
            leaf = std::get<expr::functor>(leaf->content).args[0];
            ++depth;
        }
        const uint32_t leaf_id = std::get<expr::functor>(leaf->content).id;
        if (leaf_id == k_w)
            return;
        if (depth == collapse_depth_)
            out.push_back(pool_.make_functor(k_w, {}));
        out.push_back(pool_.make_functor(k_g, {e}));
        const expr* stepped = pool_.make_functor(k_first_leaf + (leaf_id - k_first_leaf + 1) % plateau_, {});
        for (size_t level = 0; level < depth; ++level)
            stepped = pool_.make_functor(k_g, {stepped});
        out.push_back(stepped);
    }

    expr_pool& pool_;
    uint32_t plateau_;
    size_t collapse_depth_;
};

using visits_t = monte_carlo::visits_table<uint64_t, std::unordered_map>;
using values_t = monte_carlo::value_table<uint64_t, double, std::unordered_map>;
using uct_t = monte_carlo::uct_value_manifest<
    uint64_t, const expr*, double, visits_t, visits_t, values_t, values_t, mcts_path_walker,
    std::vector<const expr*>, std::vector<const expr*>, std::mt19937>;
using mcts_t = mcts_search<valley_rules, expr_size, uct_t::chooser_t, uct_t::delta_t, uct_t::terminator_t>;

struct MctsValleyIntegrationTest : ::testing::Test {
    expr_pool pool{};
    expr_size sizes{};
    valley_rules rules{pool, 10000, 4};
    const expr* initial = pool.make_functor(k_g, {pool.make_functor(k_first_leaf, {})});
    visits_t visits{};
    values_t values{};
    std::mt19937 rng{7};
    uct_t uct{visits, visits, values, values, rng, 0.5, 0};
};

}

TEST_F(MctsValleyIntegrationTest, ProbingSimulationsFindTheCollapseBehindTheExpansions) {
    mcts_t search{rules, sizes, uct.chooser, uct.delta, uct.terminator, initial, 20, 200};
    while (search.next()) {}
    EXPECT_EQ(search.best(), pool.make_functor(k_w, {}));
    EXPECT_EQ(search.derivation().front(), initial);
    EXPECT_EQ(search.derivation().back(), search.best());
}

// 200 simulations of at most 20 rewrites ask for at most 4000 proposals; best-first gets as many expansions.
TEST_F(MctsValleyIntegrationTest, BestFirstSpendsTheSameBudgetOnThePlateau) {
    best_first_search<valley_rules, expr_size> search{rules, sizes, initial, 1000000};
    for (size_t expansion = 0; expansion < 200 * 20; ++expansion)
        search.next();
    EXPECT_EQ(search.best(), initial);
}
