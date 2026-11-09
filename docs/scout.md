# The scout workflow: find what MANR fails at, and file it

**"Perform the scout workflow"** (or "scout") means: find problems MANR struggles with or fails at, check them
against MANR, and file each real failure as a GitHub issue in the fit-request form (`fit-requests.md`). Scout builds
nothing. Its output is issues. The forge workflow (`forge.md`) picks them up and builds the fixes.

Invoking scout authorizes creating issues and the `fit-request` label in `team-affix/manr`. Every issue created is
listed in the report.

## 1. Survey what MANR implements

Skip this when the session already knows. Otherwise read:

- `CLAUDE.md` and the module docs (`poly.md`, `list.md`, ...): the domains, their detectors and rules
- `plans.md` and the gaps sections of the module docs: what is known to be missing
- the benchmarks (`bench/*_bench.cpp`) and their baselines (`bench/baseline/*.tsv`): what is solved, what is open,
  and which solver is slow where
- the open issues (`gh issue list --label fit-request`), so nothing is filed twice

## 2. Invent problems MANR probably cannot do

Start from the known weak points (open benchmark problems, gaps, plans). Then invent problems, each as **data plus
the model that produced it**. Push in several directions:

- **Harder data in an existing domain.** Fewer points, no tails, few distinct values, huge values, a wrong label or
  several, data that does not determine the model.
- **Harder models in an existing domain.** Deeper nesting, programs inside programs, a step in pieces, a repeated
  computation that a helper would compress.
- **Outside the existing domains.** Sequences (a recurrence or a closed form), logic (the smallest formula for a
  truth table), algebra, graphs, strings, anything in `vision.md` or beyond it. A problem MANR has no vocabulary
  for is a valid finding: it marks a missing domain.
- **Scale.** The same problem with ten times the data, or ten times the size.

Aim for a handful, a mix of near misses and far reaches. Prefer data that determines the model: enough points that
the model is the smallest sound explanation.

## 3. Check each against MANR

Confirm each problem really fails before filing it. Run it under both solvers (a scratch copy of the domain's
benchmark or a test is enough; scout commits nothing). Record:

- the cost reached against the model's size (the target), whether the result is sound, and the time each solver takes
- the best form each solver reaches

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
| no domain | MANR cannot state the problem at all | a new domain module (`vision.md`, "What a domain must supply") |

## 5. File the issues

One issue per model, in the fit-request template (`.github/ISSUE_TEMPLATE/fit-request.md`), labelled
`fit-request` (create the label if it is missing). Title: `fit: <model in a few words>`. Fill in:

- **Domain**, **Model** (in MANR's vocabulary where it has one, and the target size), **Data** (one sample per line;
  inline when small, otherwise the generator: the model, the input ranges and a seed, so forge can rebuild it exactly)
- **Notes**: what MANR reaches today under each solver (cost, time, best form), the diagnosed cause, and related
  issues. Problems that share a cause reference each other, so forge can solve them together.

## 6. Report

List the problems tried, how MANR did on each, the cause of each failure, and the issues created (with links).
Mention problems MANR already solves that would make good benchmark problems.
