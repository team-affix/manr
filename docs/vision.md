# Vision: minimal models of data, in any domain

MANR exists to do one thing in every domain it can: take what is observed, written as symbolic expressions of that
domain, and reduce it to the smallest model that explains it. Arithmetic and polynomial algebra are the first
domains, not the scope.

## The one recipe

Every problem has the same shape, whatever the domain:

```
pw(case(input1, label1), case(input2, label2), ..., ?0)
```

- **Inputs and labels are expressions of the domain**, not only numbers. An input can be a number, a tuple, a list, a
  graph or a function. A label can be a different kind of object than its input: a list mapped to a number, a graph
  mapped to a polynomial, a function mapped to its derivative.
- **Free variables (extension terms) stand for what is not known.** The catch-all `?0` is the claim that a general
  rule exists for every unseen input. Free variables can also sit inside the data or inside a partial model, where
  part of the answer is known and the rest is left open.
- **Reduction looks for the minimal model.** The rules of the domain rewrite the piecewise toward the smallest
  expression with the same meaning on the data. The ideal outcome is that the piecewise disappears into one compact
  expression. The next best is a few regions with one compact expression each, plus the points no rule explains.
- **Smallest means minimum description length** for the domain's fixed vocabulary. A model is better when it is
  shorter than the data it replaces, so a model earns its size by explaining more.

## Domains we want

| Domain | Inputs | Labels | A minimal model would be |
|---|---|---|---|
| Arithmetic (done) | integers | integers | a formula, e.g. `2x + 1` |
| Polynomial algebra (done) | integer tuples | integers | a polynomial, or polynomials on regions, compressed (factored, Horner) |
| Algebra | tuples of elements of an unknown structure | elements | the operation and its laws: a group or ring presentation, an identity |
| Calculus | functions as expressions | their derivatives, integrals, limits or series | the operator as rules, e.g. the product and chain rules, or a closed form |
| Sequences | indices | terms | a closed form or a recurrence |
| Lists and programs | lists | lists, numbers or booleans | a program: map, filter, fold, sort, built from a small vocabulary |
| Graph theory | graphs (edge lists) | numbers, polynomials or graphs | an invariant as a formula or algorithm, e.g. counts in terms of vertices and edges |
| Logic | assignments | truth values | the smallest formula |

The same machinery serves all of them: data as a piecewise, free variables for generality, detectors that propose a
model in one step, local rules that simplify, and a search that keeps the smallest result.

## What a domain must supply

The piecewise module is already domain-free. A domain plugs in through capabilities:

- **A vocabulary**: its functors and literals, and its size measure.
- **An evaluator** (`IEvaluateAt`): the value of an expression at an input. This is what lets a model be checked
  against the data.
- **Detectors** (`IProposeHypotheses`): read the data and propose a model in one step, the way interpolation proposes
  a polynomial. Detectors are what make a domain practical. Search alone will not stumble onto a large model.
- **Local rules**: rewrites that make the domain's expressions smaller.
- **A soundness oracle**: an independent way to check that a rewrite kept the meaning, for the fuzz tests.
- **A benchmark corpus**: the problems that define what the domain can do, held by the regression gate
  (`docs/extending.md`).

## What the core still needs

- **Structured inputs and labels.** Lists, graphs and functions as data values, with evaluation over them.
- **Models with structure.** Recursion, folds and higher-order functions, so a model can be a program and not only a
  formula.
- **Free variables anywhere.** Holes inside a partial model, not only the catch-all, with detectors that fill them.
- **Noise, kept exact by associative models.** Real data is rarely exact. Instead of learning `y = f(x)` and
  allowing error, learn the relation between input and output as a predicate. The whole observation becomes the
  input and the label is a boolean: `case(point(x, y), true)` for what was observed, `case(point(x, y), false)` for
  points that were not. The model is still fit exactly, so nothing about reduction or soundness changes. Noise shows
  up as structure in the model, e.g. `lt(abs(y - (2*x + 1)), 3)`: a band around a line, built from comparators and
  ranges. The same form also covers relations that are not functions, such as a circle.
  - Where the false points come from decides how tight the model is. The false points nearest the true ones set the
    width of the band, so they have to be sampled deliberately: close enough to the data to rule out a loose model,
    but not on top of observations that noise could have produced.
  - Unseen points are still left to the catch-all, so labeling a point false is a claim, not an assumption that
    everything unobserved is impossible.
  - Related ideas in the literature: learning a relation's characteristic function (inductive logic programming,
    constraint acquisition), and sampling negatives to learn a distribution's support (noise-contrastive
    estimation, one-class classification).
- **Composition across domains.** A model in one domain can use another: a graph invariant that is a polynomial, a
  list program whose step is arithmetic.
- **A front end.** Reading data and expressions from text, so that a domain can be used without writing C++.

## How we get there

One domain at a time, by the method in `docs/extending.md`: write the problems first, add detectors and rules, and
pass the gate, so that each new domain or capability never makes an existing one worse.
