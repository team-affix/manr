# The forge workflow: pick an issue, and build what solves it

**"Perform the forge workflow"** (or "forge") means: pull the open fit-request issues, choose one (or a group that
shares a cause), design the most general fix that stays efficient, build it, prove it on the gate, and report on the
issue. The issues come from the scout workflow (`scout.md`) or from anyone (`fit-requests.md`).

A model in an issue is a guide, not expected output: success is a result that reproduces every sample and is no
larger than the model (`fit-requests.md`). Committing and pushing follow the usual rule: ask first unless told to.
Comments on the issue go out after the push, and an issue is closed only when its problem reaches its target.

## 1. Pull the issues

`gh issue list --label fit-request --state open`, then read each candidate (`gh issue view <n>`). For the chosen
ones, rebuild the data exactly (inline, or from the generator and seed).

## 2. Choose what to work on

Rank by, in order:

1. **Dependencies.** A missing domain or vocabulary comes before issues that need it; an issue blocked by another
   waits.
2. **Leverage.** Prefer the fix that solves the most issues (issues sharing a cause are one piece of work) or the
   most common kind of real data.
3. **Size.** Prefer the smallest piece of work that makes real progress. A large issue (a new domain) is split: this
   run builds its first piece, and the issue stays open with what remains.

Say which issue was chosen and why, and which were passed over.

## 3. Reproduce

Restate the model in MANR's vocabulary and compute its size: the target. Add the data to the domain's benchmark as a
problem named after the issue (`"#42 squares of positives"`), with that target and the model as target text. Run
the gate: it appears as `new`. If it is already solved, comment so on the issue, close it, and choose another.

## 4. Design the most general fix that stays efficient

Fit the issue's example first, then generalize as far as efficiency allows:

- **Decompose** (`decomposing-detection.md`): split the model's shape into a subproblem that one assumption makes
  easy, and hand the rest to an existing domain's detectors. Prefer the generic reading (an outer combiner over an
  unknown inner function) when it costs nothing; keep the specific detectors beside it.
- **Refute first** (`rule-design.md`): find the cheap facts that rule the reading out, and check them before any
  exact work.
- **Pick the scale of the change**, by what the issue needs:
  - a rule or detector in an existing module
  - a family of similar rules (one per combiner, per condition kind, per domain)
  - new vocabulary in a domain (functors, evaluation, rules)
  - a new module, for a new domain or a domain-free capability (`vision.md`, "What a domain must supply"; for
    abstraction, `plans.md`)
- Consider free variables (`rule-design.md`, section 5), and record what is out of scope in `plans.md`.

## 5. Build and prove it general

By the loop in `extending.md`:

- Tests: a unit test per new component, integration tests where real interning matters, and the domain's soundness
  fuzz covering the new rules.
- **Generality**: besides the issue's problem, add similar problems the fix should also solve (other constants, other
  inner functions, other sizes, no tails, a wrong label), a near miss it must not fit, and one the existing detectors
  already solve, as tests and benchmark problems.
- `make run_tests`, `make bench_check`, then `make bench_check_full`: no cost may get worse, and the issue's problems
  must reach their targets under at least one solver. A slowdown warning is investigated, not ignored.
- `make bench_baseline`, and update the module docs, `plans.md` and `CLAUDE.md` (if the layout changed).

## 6. Report

To the user: the issue chosen and why, the design and its generalization, what now solves (cost and time per solver,
before and after), what remains, and anything added to `plans.md`. After the commit is pushed, comment on the issue
with the same, plus the commit. Close it when its problem reaches its target; otherwise leave it open with what
remains. Issues solved along the way (sharing the cause) get the same comment and are closed too.
