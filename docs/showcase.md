# The showcase workflow: show what MANR can do, and how far it generalizes

**"Perform the showcase workflow"** (or "showcase") is the positive counterpart of scout (`scout.md`): invent
realistic problems MANR should solve, confirm it solves them soundly, push each one until it breaks to measure how
far it generalizes, and post a capability report as a GitHub Discussion. Scout says what is missing. Showcase says
what works today and where its edge is, so the state of the project is visible at any point in time.

Showcase changes no rule or detector. Its outputs are a Discussion and new benchmark problems. Invoking it
authorizes posting one Discussion in `team-affix/manr` (category **Show and tell**) and committing the benchmark
additions to master. Failures it runs into are not filed by showcase: they are listed in the report as scout
candidates.

## 1. Survey what MANR implements

As scout does (`scout.md`, section 1): `CLAUDE.md`, the module docs, `plans.md`, the benchmarks and their baselines,
and the last showcase Discussion, so a run builds on the previous one instead of repeating it.

## 2. Invent real use cases

Problems someone would actually bring, not toys made to pass: data from a real process, a program someone would
write, a formula from a real field. Each is **an input plus the model it should reduce to**, as in a reduce request.
MANR is a reduction system and fitting data is one use of it, so mix both kinds: expressions to reduce (identities,
cancellations, factorings, programs, formulas, expressions with free variables) and data to fit. Aim for a dozen of
each, spread over every domain, and favour:

- **Realistic stories.** A shop's prices with a bulk discount (a piecewise polynomial), compound growth (a
  recurrence closing to a power), a sensor log with a few bad readings (a model plus cases), a checksum (a fold), a
  voting rule (a logic formula), the primes in a range of IDs (a filter).
- **Composition.** Models that combine domains or nest programs: a map inside a fold, a recurrence over a polynomial
  step, a filter by a number-theory condition.
- **Free variables.** Data with unknown labels or holes, where binding them is part of the answer
  (`rule-design.md`, section 5).
- **Some complexity.** Several pieces, two or three inputs, long lists, large values, enough that the reduction is
  not obvious by eye.

Prefer data that determines the model: enough points that the model is the smallest sound explanation.

## 3. Confirm each one

Run each problem under both solvers (a scratch harness, or straight in the domain's benchmark). Record the cost
against the model's size, soundness (`scout.md`, section 3: under some binding of the free variables), the time,
and the form reached. A problem counts as solved when the result is sound and no larger than the model. Unsolved ones
are dropped from the showcase and listed as scout candidates.

## 4. Measure how far each one generalizes

For every solved problem, vary it along the axes that matter for its domain, and find where it stops working:

- **Scale**: more points, longer lists, larger values, more inputs.
- **Less data**: fewer points, no tails, data that starts late, until the model is no longer determined.
- **Noise**: one wrong label, then several, until the model stops being the smallest explanation.
- **Holes**: an unknown label, a label holding a hole, a hole in an input.
- **Constants and shape**: other coefficients, another inner function, one more piece.

Record each variant as solved or not, so every problem gets a margin: "solves up to N", "survives K wrong labels",
"breaks on holes in keys". A variant that fails where it should not is a scout candidate.

## 5. Lock it in

Add the solved problems, and their most telling variants, to the domain's benchmark (`<domain>_bench`) with their
targets, named `showcase: <story>`. Run `make bench_check` (the problems appear as `new`), then `make bench_baseline`,
and commit the benchmark and baseline. From then on the gate catches any regression of a showcased capability.

## 6. Report

Post one Discussion in the **Show and tell** category, titled `showcase: <date>`. Report problems as plain input and
output, with no story: readers can see the domain and the use for themselves.

- One short preamble: that data becomes a piecewise whose catch-all `?0` is bound by reduction, the notation (lists
  as `[a, b]`), how many were solved, and that they are gated.
- **Every input in full**, every case of every data set, however long: the report demonstrates what MANR does, so
  nothing of an input is left out. Take them from the benchmarks' `--full-inputs` notes. If the body passes
  GitHub's size limit (65,536 characters), continue in comments, never by shortening.
- One section per kind, expressions first, then data by domain. Each is a table with three columns, one row per
  problem: **demonstrates** (a fragment naming what the row shows, e.g. "recognizes the primes from a late start",
  "binds unknown labels"), **input**, **output**, and **time**: wall-clock for the solver that produced the output,
  from a `--serial` run.
- **Not reduced**: a table of what was tried, the full input, the output reached, the smaller form that exists, and
  the time spent before the search stopped.

Give the user the Discussion's link and a few lines of summary.
