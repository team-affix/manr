# Fit requests: issues as the roadmap

MANR's capability is uneven by design: many domains, each with its own rules, and data that some domain can fit
well, some only partly, and some not at all. Rather than plan that frontier in the abstract, it grows one concrete
request at a time. A **fit request** is a GitHub issue (template: `.github/ISSUE_TEMPLATE/fit-request.md`) holding:

- **the data**: inputs and outputs, in the domain's terms (numbers, tuples, lists, ...)
- **the model that produced it**: the program or formula the poster knows generated the data

The model is a **guide, not the expected output.** MANR is not asked to print that model's text. Reduction order is
not fixed, and many expressions mean the same thing. A result is acceptable when it is **sound** (reproduces every
sample) and **no larger** than the given model by `expr_size`. A result equivalent to the model, or smaller and still
sound, is a success. The model tells the developer (usually Claude) what really generated the data, so the work is to
make MANR recover it: which detectors and rules are missing, and which domain they belong to.

## Handling a fit request

1. **Read the issue and restate it in MANR's vocabulary.** Write the model as a MANR expression of the domain
   (`fold(h * acc, 1, xs)`, `map(e^2, filter(lt(0, e), xs))`), and compute its size: that is the target cost. If the
   domain lacks a functor the model needs, that is the first piece of work, and the issue says so.
2. **Add the data to the domain's benchmark** (`bench/<domain>_bench.cpp`) as a problem with that target and the
   model as its target text, named after the issue (e.g. `"#42 squares of positives"`). Run the gate: it appears as
   `new`, with what MANR reaches today. If it is already solved, report that on the issue and stop.
3. **Find why it fails.** Look at the best form each solver reaches (the benchmark prints it). Typical causes: no
   detector reads the shape, a detector's assumption is broken by the data (no tails, few distinct values, a wrong
   label), or the search never takes the right proposal (dilution, budget).
4. **Decompose** (`decomposing-detection.md`): split the model's shape into a subproblem that one assumption makes
   easy, and hand the rest to an existing domain's detectors. Prefer the generic version of the reading when it
   costs nothing, and keep the specific detectors that exist.
5. **Prove the technique is general, not fitted to the issue.** Beyond the issue's own problem, write similar fitting
   challenges that the new detectors or rules should also solve, and add them as tests and benchmark problems. Vary
   what the technique claims not to depend on: other values and constants, other inner functions, longer or fewer
   lists, no tails, a wrong label, another domain's elements where it applies. Add at least one near miss it must
   not fit, and one the specific detectors already solved, to show it stays out of their way. For example, a fix for
   `map(e^2, filter(lt(0, e), xs))` should also solve `map(2 * e + 1, filter(lt(e, 3), xs))`, and leave plain maps
   and plain filters to their own detectors.
6. **Build it by the loop in `extending.md`**: tests, `make bench_check`, `make bench_check_full`, then
   `make bench_baseline`, committed with the change. The issue's problem and its similar challenges must reach their
   targets under at least one solver, and nothing else may get worse.
7. **Report on the issue** what MANR now finds (the printed result and its cost), whether it is the same model in
   other words or a different sound one, the similar challenges it also solves, and the commit. If only part is
   solved (one solver, or cost above target), say what remains and leave the issue open.

When the data does not determine the model (too few distinct values, a smaller sound model exists), say so on the
issue. A smaller sound result is still a success by MDL; the benchmark keeps the issue's target so the gate records
it as solved.

## Keeping requests useful

- One model per issue. Several related models are several issues.
- Data as the domain's samples would hold it: for lists, each input list and its output. Include the empty input
  if the model defines one. Wrong labels on purpose are fine; say how many.
- Prefer enough data that the model is the smallest sound explanation. When unsure, post more points.
- An issue that needs a new domain is welcome; it becomes that domain's first benchmark problem.
