# The scout workflow: find what MANR fails at, and file it

**"Perform the scout workflow"** (or "scout") means: find inputs MANR struggles to reduce or fails to reduce, check
them against MANR, and file each real failure as a GitHub issue in the reduce-request form (`reduce-requests.md`).
Scout builds nothing. Its output is issues. The forge workflow (`forge.md`) picks them up and builds the fixes.

Invoking scout authorizes creating issues and the `reduce-request` label in `team-affix/manr`. Every issue created is
listed in the report.

## 1. Survey what MANR implements

Skip this when the session already knows. Otherwise read:

- `CLAUDE.md` and the module docs (`poly.md`, `list.md`, ...): the domains, their detectors and rules
- `plans.md` and the gaps sections of the module docs: what is known to be missing
- the benchmarks (`bench/*_bench.cpp`) and their baselines (`bench/baseline/*.tsv`): what is solved, what is open,
  and which solver is slow where
- the open issues (`gh issue list --label reduce-request`), so nothing is filed twice

## 2. Invent problems MANR probably cannot do

Start from the known weak points (open benchmark problems, gaps, plans). Then invent problems, each as **an input
plus the reduction it should reach**: an expression and its simpler form, or data and the model that produced it.
Push in several directions:

- **Expressions that do not reduce as they should.** Symbolic expressions of an existing domain whose obvious
  simpler form MANR misses: cancellations, factorings, identities, nested forms (`poly_bench` is the model).
- **Harder data in an existing domain.** Fewer points, no tails, few distinct values, huge values, a wrong label or
  several, data that does not determine the model.
- **Harder models in an existing domain.** Deeper nesting, programs inside programs, a step in pieces, a repeated
  computation that a helper would compress.
- **Outside the existing domains.** Sequences (a recurrence or a closed form), logic (the smallest formula for a
  truth table), algebra, graphs, strings, anything in `vision.md` or beyond it. A problem MANR has no vocabulary
  for is a valid finding: it marks a missing domain.
- **Scale.** The same problem with ten times the data, or ten times the size.
- **Free variables (`?n`), the priority.** We want many of these: they are what turns reduction into
  generalization, and MANR binds almost none of them yet (`plans.md`, free variables). A free variable is a hole at the syntax level: reduction may bind it to any expression
  of the domain, and should bind it to whatever makes the whole expression smallest. That is how reduction
  generalizes: the data piecewise's catch-all `?0` is bound to a model, and the same goes for a `?n` anywhere. So a
  request with free variables asks for the smallest expression reachable by choosing their bindings, and its desired
  reduction names the bindings too. The piecewise is the case to start from: the data piecewise's catch-all is
  already a hole, and holes inside it (a label `?1`, a label `2 * ?1 + 1`, an input `cons(?2, nil)`) are the most
  common requests with free variables. Reducing one leads into free-variable reductions in the domain's own terms
  (`rule-design.md`, section 5). Make some requests carry them, in every domain:
  - data with holes inside: `0 -> 1, 1 -> ?1, 2 -> 5, 3 -> 2 * ?2, 4 -> 17` reduces to `x^2 + 1`, with
    `?1 = 2` and `?2 = 5`;
  - polynomials: `(?0 + 1)^2 - ?0^2` reduces to `1`, with `?0 = 0`;
  - logic: `or(and(not(x), y), ?0)` reduces to `xor(x, y)`, with `?0 = and(x, not(y))`;
  - several holes, holes inside data (an output that is `?1`, a list holding `?0`), and holes whose best binding is
    itself an expression of the input variables, not a constant.

Aim for a handful, a mix of near misses and far reaches, and make free-variable requests a large share of them,
about half, starting with holes inside data. Prefer data that determines the model: enough points that
the model is the smallest sound explanation. Inputs in domains MANR does not support at all are as welcome as any.

## 3. Check each against MANR

Confirm each problem really fails before filing it. Run it under both solvers (a scratch copy of the domain's
benchmark or a test is enough: `poly_bench` for expressions, `fit_bench` or `list_bench` for data; scout commits
nothing). Record:

- the cost reached against the model's size (the target), whether the result is sound, and the time each solver takes
- the best form each solver reaches

Free variables have no text form yet (the parsers do not read `?n`): build those inputs with
`expr_pool::make_var(n)` in the scratch test. A result is sound when it means the same as the input
under some binding of its free variables: check by applying the bindings and comparing normal forms, or evaluating
at sample points. A result that is smaller than the input but unsound under every binding, or a crash, is a failure
as much as a missed binding.

A problem MANR already solves is dropped (or, if it is a good regression check, mentioned in the report as a
candidate benchmark problem). A problem outside every domain needs no run: MANR cannot state it.

## 4. Diagnose each failure

Classify why it fails, cheapest cause first. The cause goes in the issue, because it tells forge what kind of work
the issue is.

| cause | sign | what it needs |
|---|---|---|
| search | a detector proposes the right model, but the solver never takes it, or runs out of time | search or budget work (`search.md`), less dilution or drag (`extending.md`) |
| a broken assumption | a detector for the shape exists, but this data breaks what it assumes (no tails, too few values) | a new subproblem that drops that assumption (`decomposing-detection.md`) |
| no detector | nothing proposes this shape | a detector, decomposed into easy subproblems |
| no vocabulary | the domain cannot express the model (booleans, tuples, helpers) | functors, evaluation and rules for them, or a plan in `plans.md` |
| no binding | nothing proposes the binding of a `?n` that would shrink the expression, or a rule stops at `?n` | a detector that chooses bindings (solving for the hole, as the binder does for data), or a rule made to tolerate `?n` (`rule-design.md`, section 5) |
| no domain | MANR cannot state the problem at all | a new domain module (`vision.md`, "What a domain must supply") |

## 5. File the issues

One issue per desired reduction, in the reduce-request template (`.github/ISSUE_TEMPLATE/reduce-request.md`),
labelled `reduce-request` (create the label if it is missing). Title: `reduce: <a few words>`. Fill in:

- **Domain**; **Input**: the expression, or the data (one sample per line; inline when small, otherwise the
  generator: the model, the input ranges and a seed, so forge can rebuild it exactly); **Desired reduction**: the
  simpler form or the model, in MANR's vocabulary where it has one, with its size (the target). Write free
  variables as `?n`, give the bindings that reach the desired reduction, and say how to build them
  (`make_var(n)`), since nothing parses them yet
- **Notes**: what MANR reaches today under each solver (cost, time, best form), the diagnosed cause, and related
  issues. Problems that share a cause reference each other, so forge can solve them together.

## 6. Report

List the problems tried, how MANR did on each, the cause of each failure, and the issues created (with links).
Mention problems MANR already solves that would make good benchmark problems.
