# Search

Reduction is search, not a fixed sequence of reductions. Rules branch, and the goal is the smallest final
expression. The hard case is a reduction that only becomes available after several rewrites that each make the
expression larger, or no smaller. A search that scores each rewrite by the expression it produces next cannot see
the collapse coming. What matters is the size of the expression many rewrites later. That is future reward, and it
is why MANR has Monte Carlo tree search.

Both searches take the same capabilities (`IDetect`, `IGetCost`, an initial expression) and expose the same pull
loop (`next()`, `best()`, `best_cost()`, a derivation). A manifest picks one. Rules only detect; performing a rewrite is
the search's choice. MCTS performs only the rewrite it takes. Best-first performs every rewrite of the expression it
expands, so each result is costed before anything is ordered by it.

## `mcts_search`: probing simulations

Each `next()` is one simulation from the initial expression:

1. The current expression's rewrites are detected. Inside the MCTS tree, UCT (`team-affix/mcts`,
   `uct_value_manifest`) picks one by index; past the tree's frontier, one is picked uniformly at random. Only the
   picked rewrite is performed. One that returns nullptr leaves the expression in place and uses up the step.
2. The simulation continues for up to `probing_depth` rewrites, and stops early at an expression with no rewrites.
3. The reward is the smallest cost the simulation reached at any step,
   `(initial cost - smallest cost reached) / initial cost`, and it is backed up the tree path.

Expansions are therefore credited with whatever collapse lies within `probing_depth` rewrites behind them.

- **Smallest cost reached, not the cost at the end.** Any prefix of a derivation is itself a derivation. A
  simulation that passes through a small expression and then wanders off has still found that expression, and
  `best()` keeps it. Scoring the end of the probe instead would be a one-line change in `mcts_search::next()`.
- **Tree MCTS.** Nodes are named by their path of choices (`mcts_path_walker`, a 64-bit hash), so two routes to
  the same expression keep separate statistics. Sharing statistics across transpositions is possible because
  expressions are interned, but a node's reward then mixes in costs from different prefixes.
- **Memory** is one visits entry and one value entry per tree node, and a simulation adds at most one node. A
  probe adds nothing beyond its own path.
- **Every step proposes on the whole expression,** including detectors. Deep probes (around 1000 rewrites) cost
  `probing_depth` calls to `detect` per simulation, so detection cost matters here.
- `core/test/integration/mcts_valley.cpp` shows the motivating case. The collapse sits behind three cost-increasing
  rewrites, and every depth has a 10 000-wide plateau of equal-cost rewrites. MCTS finds it in 200 simulations of
  depth 20. Best-first, given the same 4000 expansions, is still clearing the first plateaus.

Not built yet:

- Root advancement: committing to the best first move and re-rooting, so a long horizon can be covered by many
  shorter probes.
- Rollout policies smarter than uniform, for example ones that prefer detector proposals.

## `best_first_search`: cost-ordered frontier

The frontier holds reached expressions not yet expanded, keyed by their own cost. Each `next()` expands the cheapest:
it detects that expression's rewrites and performs all of them, queuing each new result. Expressions are interned,
so one reached twice is queued and detected once. Detection, the expensive part, runs only on expanded expressions.

An earlier version queued the rewrites themselves, keyed by their source's cost, and performed one per step. That
starved siblings: once one rewrite produced something cheaper than its source, that result's own rewrites outranked
every untried sibling, so a far better sibling (a data fit binding the whole piecewise to `x^3 - x`) was never
performed.

- Rewrites are scored only by the expression they produce, so a climb toward a collapse competes with every
  cheaper or equal-cost alternative. It survives only while it fits in the frontier and nothing cheaper is left.
- The frontier is capped (`frontier_capacity`), and the most expensive entry is evicted.
- The set of expressions already reached (`parents_`, also used for derivations) is **not** bounded. In long runs
  it is the main memory cost.

It suits rule sets where detectors already jump the valleys and leave a shallow, wide tree. It is also a
deterministic baseline.
