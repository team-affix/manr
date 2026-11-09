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

## The two workflows

- **Scout** (`scout.md`) finds problems MANR fails at, checks them, diagnoses the cause, and files them as fit
  requests.
- **Forge** (`forge.md`) pulls the open fit requests, chooses one by dependencies, leverage and size, designs the most
  general fix that stays efficient, builds it with similar problems that prove it general, passes the gate, and
  reports on the issue.

Anyone can file a fit request by hand too; forge treats it the same way.

When the data does not determine the model (too few distinct values, a smaller sound model exists), forge says so on
the issue. A smaller sound result is still a success by MDL; the benchmark keeps the issue's target so the gate
records it as solved.

## Keeping requests useful

- One model per issue. Several related models are several issues.
- Data as the domain's samples would hold it: for lists, each input list and its output. Include the empty input
  if the model defines one. Wrong labels on purpose are fine; say how many.
- Prefer enough data that the model is the smallest sound explanation. When unsure, post more points.
- An issue that needs a new domain is welcome; it becomes that domain's first benchmark problem.
