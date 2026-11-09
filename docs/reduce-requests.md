# Reduce requests: issues as the roadmap

MANR's capability is uneven by design: many domains, each with its own rules, and inputs that some domain reduces
well, some only partly, and some not at all. Rather than plan that frontier in the abstract, it grows one concrete
request at a time. A **reduce request** is a GitHub issue (template: `.github/ISSUE_TEMPLATE/reduce-request.md`,
label `reduce-request`) holding:

- **the input**: what to reduce. Either
  - **a symbolic expression** of some domain, e.g. `(x + 1)^2 - (x - 1)^2`, or
  - **data**: inputs and outputs in the domain's terms (numbers, tuples, lists, ...), which MANR turns mechanically
    into a piecewise `pw(case(in, out), ..., ?0)`. A data fit is one kind of reduce request, not a separate thing.
- **the desired reduction**: the form it should reduce to, or something like it. For data, the model that produced
  it; for an expression, the simpler form, e.g. `4 * x`.

Either may contain **free variables** (`?n`). They are holes at the syntax level: reduction may bind each to any
expression, and a request with them asks for the smallest form reachable by choosing the bindings, e.g.
`or(and(not(x), y), ?0)` to `xor(x, y)` with `?0 = and(x, not(y))`. The desired reduction then gives the bindings
too. Data's own catch-all `?0` is the common case of the same thing, and holes inside data (a label `?1`, a list
holding `?2`) are its harder cases.

The desired form is a **guide, not the expected output.** MANR is not asked to print its text. Reduction order is not
fixed, and many expressions mean the same thing. A result is acceptable when it is **sound** (it means the same as
the input under some binding of the input's free variables: the same normal form for an expression, every sample
reproduced for data) and **no larger** than the
desired form by `expr_size`. A result equivalent to it, or smaller and still sound, is a success. The desired form
tells the developer (usually Claude) what reduction is really possible, so the work is to make MANR find it: which
detectors and rules are missing, and which domain they belong to.

**Any domain is welcome, including ones MANR does not support at all.** A request in an unsupported domain (logic,
sequences, graphs, strings, ...) is how a new domain starts: forge builds the module, or several, that let MANR state
and reduce it, and the request becomes that domain's first benchmark problem.

## The two workflows

- **Scout** (`scout.md`) finds inputs MANR fails to reduce, checks them, diagnoses the cause, and files them as
  reduce requests.
- **Forge** (`forge.md`) pulls the open reduce requests, chooses one by dependencies, leverage and size, designs the
  most general fix that stays efficient (a rule, a family of rules, vocabulary, or new modules), builds it with
  similar problems that prove it general, passes the gate, and reports on the issue.

Anyone can file a reduce request by hand too; forge treats it the same way.

When the input does not determine the desired form (data with too few distinct values, a smaller sound result
exists), forge says so on the issue. A smaller sound result is still a success by MDL; the benchmark keeps the
issue's target so the gate records it as solved.

## Keeping requests useful

- One desired reduction per issue. Several related ones are several issues.
- An expression in any clear notation; MANR's own (`docs/poly.md`, `docs/list.md`) when the domain has one.
- Data as the domain's samples would hold it: for lists, each input list and its output. Include the empty input
  if the model defines one. Wrong labels on purpose are fine; say how many.
- Prefer enough data that the model is the smallest sound explanation. When unsure, post more points.
