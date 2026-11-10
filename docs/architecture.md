# Architecture: modules, rules and solvers

Reduction is a hard problem, so rules are many, specific and individually complex. The repository stays manageable by
keeping everything *around* the rules rigid: three layers, one shape per kind of file, and a fixed place to add each
new thing. A rule may be as complicated inside as it needs to be. Its outside is always the same.

```
core/               domain-free: expressions, the pool, size, printing, rule plumbing, the searches
modules/<domain>/   one domain each: its vocabulary, rules, hypothesis sources and what they use
solvers/<solver>/   composition only: manifests and runtimes that wire modules into solvers
bench/              the corpora and the regression gate over the solvers
```

Each layer depends only on the layers above it. Core never includes a module, and a module never includes a solver.
Modules may include other modules' headers, but rarely need to: collaborators are template parameters, so a list
detector hands element fitting to "whatever proposes hypotheses" without naming polynomials. Domains meet only in
`solvers/`.

## Modules

A module is one domain, and every file in it carries the domain's prefix. The prefix keeps file names unique, because
every module's `hpp/` is on the include path.

| module | prefix | domain |
|---|---|---|
| `piecewise` | `pw_` | the extension algebra over data sets, generic over the domain |
| `abstraction` | `abs_` | binders (`lam`, `lams`, `app`, `let`), de Bruijn indices, beta-reduction |
| `arith` | `arith_` | integer arithmetic: literals, evaluator, simplifier, the hole solver |
| `poly` | `poly_` | polynomials over Z: normal form, rules, fitting detectors |
| `list` | `list_` | lists and list programs (map, filter, fold) |
| `seq` | `seq_` | integer sequences: recurrences, powers, infinite lists |
| `number` | `num_` | number theory over another domain: primes |
| `logic` | `logic_` | Boolean functions |
| `rational` | `rat_` | quotients of polynomials (`docs/rational.md`) |

Inside a module, a file's directory says what it is:

```
modules/<domain>/
  hpp/value_objects/    the vocabulary (<prefix>_symbols: the domain's functors) and data the domain passes around
  hpp/rules/            rules: one per file
  hpp/hypotheses/       hypothesis sources: one per file
  hpp/infrastructure/   what rules and hypothesis sources use: evaluators, solvers, readers, renderers, parsers
  cpp/<same dirs>/      the .cpp of non-templated types
  test/unit/            one real component, everything else mocked
  test/integration/     several real components of this module (a real pool, real literals)
```

`rules/` and `hypotheses/` are flat lists that only grow. To see what a domain can do, list those two directories.

## Rules

A rule rewrites an expression. It has one capability:

```cpp
void detect(const expr* e, std::vector<rewrite>& out);
```

`detect` looks at `e`. For each rewrite it is certain of, it appends a thunk that builds the result. Appending
nothing means the rule does not apply. A rule is either *local* (it reads only the node it is handed, and the solver
lifts it to every position) or a *root* rule (it reads the whole expression, for example because a binding of a hole
must substitute everywhere). The rule itself does not say which: the solver puts it in its local or its root list.

Every rule follows `docs/rule-design.md`: certain detection, cheap refutation first, filters that only reject, limits
as defaulted parameters.

## Hypothesis sources

A hypothesis source proposes what a data piecewise's catch-all could be: models for the data. It has one capability:

```cpp
void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
```

The piecewise rules (`pw_extension_binder`, `pw_branch_absorber`) bind the catch-all to a proposal and drop the
branches it reproduces. Most of a domain's intelligence lives here: a list fold detector, a parity solver, polynomial
interpolation. A source may hand the hard part of its problem to another domain's sources, which it receives as a
template parameter (`docs/decomposing-detection.md`).

## Solvers

A solver directory holds composition and nothing else: types that choose components and own them. A solver for
`<name>` has:

```
solvers/<name>/
  hpp/composition/   <name>_rules_manifest    the rule set, in the fixed shape below
                     <name>_manifest          kernel + rule set + best-first search
                     <name>_mcts_manifest     kernel + rule set + MCTS (same rules)
                     <name>_runtime           the non-templated facade: next(), best(), derivation()
  cpp/composition/   their constructors
  test/integration/  tests that need the real wiring: detectors on real data, runtimes, the soundness fuzz test
```

| solver | what it solves | kernel |
|---|---|---|
| `arith_fit` | integer samples, arithmetic expressions (the smallest example of the shape) | `arith_kernel_manifest` |
| `poly_reduce` | polynomial expressions to their smallest form | `poly_kernel_manifest` |
| `poly_fit` | polynomial data over any number of inputs | `poly_kernel_manifest` |
| `list_fit` | list programs from list data | `poly_kernel_manifest` |
| `seq_fit` | integer sequences, and open-ended lists | `poly_kernel_manifest` |
| `logic_fit` | Boolean functions from truth tables | `poly_kernel_manifest` |

A *kernel* manifest holds what is neither a rule nor a search: the pool, symbols, literals, sizes and printing (and,
for polynomials, the normal form oracle). Some solvers also provide reusable pieces that other solvers borrow:

- `pw_fit_rules_manifest` (`solvers/pw_fit`): the piecewise rules every fitting solver uses, over one domain's
  hypotheses and evaluator: the binder, the absorber, and the builder that turns data points into the starting
  piecewise.
- `poly_rules_manifest::local_rules_t` (`poly_reduce`): every polynomial rule, so a solver that produces polynomial
  bodies compresses them.
- `poly_fit_hypotheses_manifest` (`poly_fit`) and `logic_fit_hypotheses_manifest` (`logic_fit`): polynomial and
  Boolean fitting as hypothesis sources, for domains that hand a subproblem to them (list bodies, sequence steps,
  filter conditions).

### The shape of a rules manifest

Every `<name>_rules_manifest` declares its types in the same four sections, in this order. Its members follow the same
order, and so does its constructor's init list.

```cpp
struct seq_fit_rules_manifest {
    using kernel_t = poly_kernel_manifest;

    // ---- support: what the hypotheses and rules use ----
    struct evaluator_t : seq_evaluator<...> { using ...; };
    ...
    // ---- hypotheses: add one's type here, then to `hypotheses_t` ----
    struct recurrences_t : seq_recurrence_hypotheses<...> { using ...; };
    struct primes_t : num_prime_hypotheses<...> { using ...; };
    using hypotheses_base_t = pw_hypotheses_union<
        fit_t::hypotheses_t,
        recurrences_t,
        primes_t>;
    struct hypotheses_t : hypotheses_base_t { using hypotheses_base_t::hypotheses_base_t; };

    // ---- rules: add a rule's type here, then to `local_rules_t` (every position) or `root_rules_t` (the root) ----
    struct piecewise_t : pw_fit_rules_manifest<hypotheses_t, pw_evaluator_t, function_maker_t, ...> { using ...; };
    struct closer_t : seq_power_closer<...> { using ...; };
    struct open_lists_t : seq_open_list_reader<...> { using ...; };

    // ---- the rule lists ----
    using local_rules_base_t = proposer_union<
        piecewise_t::binder_t,
        piecewise_t::absorber_t,
        closer_t,
        poly_rules_manifest::local_rules_t>;
    struct local_rules_t : local_rules_base_t { ... };
    using root_rules_base_t = proposer_union<
        open_lists_t>;
    struct root_rules_t : root_rules_base_t { ... };
    struct subterms_t : subterm_proposer<local_rules_t, expr_pool> { ... };   // the local rules at every position
    struct rules_t : proposer_union<subterms_t, root_rules_t> { ... };      // what a search injects

    seq_fit_rules_manifest(kernel_t& kernel, ...);

    // support and hypotheses members, in declaration order
    hypotheses_t hypotheses_;
    // rules
    piecewise_t piecewise_;
    closer_t closer_;
    open_lists_t open_lists_;
    // the rule lists
    local_rules_t local_rules_;
    root_rules_t root_rules_;
    subterms_t subterms_;
    rules_t rules_;
    const expr* initial_;   // where a search starts: the data as a piecewise with an open catch-all
};
```

The four lists are the only places that decide what a solver does: `hypotheses_t`, `local_rules_t`, `root_rules_t`
(and, in `poly_reduce`, the polynomial rules every other solver borrows). A solver with no root rules keeps the
empty `proposer_union<>`, so every rule set has the same shape.

## Adding things

### A rule

1. Write `modules/<domain>/hpp/rules/<prefix>_<what>.hpp`: a template over the capabilities it calls, with
   `detect(e, out)`, following `docs/rule-design.md`. Its limits are defaulted parameters named
   `k_<prefix>_default_<limit>`.
2. Test it: a unit test in `modules/<domain>/test/unit/` (mocks), and an integration test in
   `solvers/<solver>/test/integration/` if it needs real interning or the real wiring.
3. Wire it into each solver that should use it, in `<solver>_rules_manifest`:
   - its nested type, at the end of the rules section;
   - its type, appended to `local_rules_base_t` or `root_rules_base_t`;
   - its member, after the other rule members;
   - in the `.cpp`, its construction at the same place, and its member appended to `local_rules_(...)` or
     `root_rules_(...)`.
4. Run the gate (`docs/extending.md`): `make run_tests`, `make bench_check`; record the baseline with the change.

### A hypothesis source

The same four steps. Write it in `modules/<domain>/hpp/hypotheses/` with `propose_hypotheses(points, out)`. Put its
type at the end of the hypotheses section, append it to `hypotheses_base_t`, and append its member to the
`hypotheses_(...)` init.

### A domain

1. `modules/<domain>/` with a new prefix: `value_objects/<prefix>_symbols.hpp` (its functors), an evaluator in
   `infrastructure/` (the `IEvaluateAt` capability: the value of an expression at an input), then its hypothesis
   sources and rules.
2. `solvers/<domain>_fit/`: a rules manifest in the shape above (on `poly_kernel_manifest` unless the domain needs
   its own kernel), the two search manifests, a runtime, and a soundness fuzz test with the domain's own oracle.
3. A corpus in `bench/` with its baseline, and a `docs/<domain>.md`.

### A solver variant

A new manifest in the solver's directory that composes a different rule list or search (as `poly_baseline_manifest`
does with only the low-level rules). Composition decides what runs, not the rule (`docs/extending.md`, design rule 5).

## Order is behavior

The lists are ordered. Detection order is the order of rewrites, which decides ties in both searches, so the gate's
exact costs depend on it. Appending to a list is the safe change. Reordering is a behavior change and goes through
the gate like one. Members are constructed in declaration order, and some constructors intern symbols, so add new
members after the existing ones in their section and don't reorder them.
