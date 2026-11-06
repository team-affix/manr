---
description: Dependency injection, template collaborators, and strategy interfaces
---

# Dependency Injection & Behavior Swapping

- Swap behavior with templates, not inheritance. The ONLY allowed use of
  inheritance is the manifest naming trick (`struct x_t : y<...> { using y<...>::y; };`).
- Inject collaborators through TEMPLATE parameters, never as concrete types. A
  collaborator's type is always a template parameter (named for its capability),
  even when only one production implementation exists. Naming a concrete
  collaborator type in a constructor/member is wrong — it defeats swapping and
  mock-based testing.
- Inject collaborators by reference. A struct must not OWN large collaborators
  that have behavior; it holds a `T&` supplied at construction. Value objects
  and small state may be owned.
  - Exception: manifests own their collaborators by value (they are the
    composition root, e.g. `dbuct_ridge_manifest`).
- Take a separate template type parameter per collaborator capability. Do not
  bundle multiple roles into one parameter. One concrete object may be passed
  for several slots when it provides all those capabilities.

```cpp
template<typename IFrameDepthTracker, typename IGoalExprs, typename IMhu>
struct dbuct_frame_hub {
    dbuct_frame_hub(IFrameDepthTracker& depth_tracker, IGoalExprs& goal_exprs, IMhu& mhu)
        : depth_tracker_(depth_tracker), goal_exprs_(goal_exprs), mhu_(mhu) {}
private:
    IFrameDepthTracker& depth_tracker_;   // injected by reference, not owned
    IGoalExprs& goal_exprs_;
    IMhu& mhu_;
};
```

## Always template the collaborator, never name its concrete type

```cpp
// BAD: constructor names the concrete collaborator type
struct dbuct_goal_candidate_rules {
    dbuct_goal_candidate_rules(ra_rule_id_set_factory& factory);
private:
    ra_rule_id_set_factory& factory_;
};

// GOOD: the collaborator is a template parameter named for the invoked capability
template<typename IMakeRuleIdSet>
struct dbuct_goal_candidate_rules {
    dbuct_goal_candidate_rules(IMakeRuleIdSet& make_rule_id_set);
private:
    IMakeRuleIdSet& make_rule_id_set_;   // we only ever call make_rule_id_set_.make()
};
```

The only method invoked on the factory is `.make()`, so the capability is
`IMakeRuleIdSet` — not `IRaRuleIdSetFactory`, which would re-introduce the
concrete concept (see "Depend on capabilities, not concepts" below).

## Depend on capabilities, not concepts (no verbal coupling)

A template parameter should name ONE function-level capability, not a broader
"thing" concept. Even naming a collaborator after a concept (`IFrameHub`) is
coupling: it says the caller conceptually knows *what* it is talking to. All a
type should know is *which functions* it invokes and their immediate context.

If a struct calls N distinct methods on one injected collaborator, that is N
pieces of coupling. Split it into N template parameters — one per method —
each injected by reference. This also frees those behaviors to be implemented
in separate locations later, and dissolves the artificial umbrella concept.

```cpp
// BAD: one "IFrameHub" concept, three methods called on it → verbal coupling
template<typename IFrameHub, typename IGetUltimateDecisionDepth, typename IDbuct>
struct dbuct_sim {
    dbuct_sim(IFrameHub& hub, IGetUltimateDecisionDepth&, IDbuct&);
private:
    IFrameHub& hub_;   // we call hub_.depth(), hub_.push_frame(), hub_.pop_frame()
};

// GOOD: one capability per parameter; the "IFrameHub" concept disappears
template<typename IGetFrameDepth, typename IPushFrame, typename IPopFrame,
         typename IGetUltimateDecisionDepth, typename IDbuct>
struct dbuct_sim {
    dbuct_sim(IGetFrameDepth& get_frame_depth,
              IPushFrame& push_frame,
              IPopFrame& pop_frame,
              IGetUltimateDecisionDepth&, IDbuct&);
private:
    IGetFrameDepth& get_frame_depth_;   // get_frame_depth_.depth()
    IPushFrame&     push_frame_;        // push_frame_.push_frame()
    IPopFrame&      pop_frame_;         // pop_frame_.pop_frame()
};
```

One concrete object (e.g. a frame hub) can still be passed for all three slots,
but the caller no longer *depends* on that fact.

## Multiple collaborators sharing one capability

When two (or more) collaborators implement the SAME function and the parent must
invoke that function on each of them, do NOT name the parameters after what the
concrete things are — that re-introduces verbal coupling.

```cpp
// BAD: caller "knows" it is talking to a dog and a cat
template<typename IDog, typename ICat>
struct feeder {
private:
    IDog& dog_;
    ICat& cat_;
    // dog_.eat(); cat_.eat();
};
```

Instead, name them after the shared capability plus an index, keeping it as loose
as possible while still preserving caller-level intuition:

```cpp
// GOOD: both are just eaters; nothing else is assumed
template<typename IEater0, typename IEater1>
struct feeder {
private:
    IEater0& e0_;
    IEater1& e1_;
    // e0_.eat(); e1_.eat();
};
```

### Example: orchestrators / synchronizers

A synchronizer/orchestrator whose whole job is to fan a lifecycle event out to many
components is just a larger instance of the `IEater` case: many collaborators share
the same capability and the parent invokes it on each. It holds one slot per
component it drives, and the same naming discipline applies.

Do NOT name a parameter after the component as a solid concept
(`IFrameDepthTracker`, `IGoalExprs`, `IMhu`). That couples the orchestrator to the
idea that there is a singular "depth tracker" concept and that its push/pop live in
one place. Instead, look at how each is actually invoked and name the parameter
after that per-method capability, keeping it open whether it is implemented in one
place or several.

```cpp
// BAD: orchestrator couples to each collaborator as a solid concept
template<typename IFrameDepthTracker, typename IGoalExprs, /* … */ typename IMhu>
struct dbuct_frame_hub {
private:
    IFrameDepthTracker& depth_tracker_;   // depth_tracker_.push(); .pop();
    IGoalExprs&         goal_exprs_;       // goal_exprs_.push_frame(); .pop_frame();
    IMhu&               mhu_;              // mhu_.push_frame(); .pop_frame();
};

// GOOD: one capability per invoked method; concepts dissolve
template<typename IPushFrameDepthFrame, typename IPopFrameDepthFrame,
         typename IPushGoalExprFrame,   typename IPopGoalExprFrame,
         /* … */
         typename IPushBindingFrame,    typename IPopBindingFrame>
struct dbuct_frame_hub { /* push_frame() calls each IPush*, pop_frame() each IPop* */ };
```

Note how `IMhu` becomes `IPushBindingFrame` / `IPopBindingFrame` — named for what
the call means at the caller, not for the concrete collaborator behind it. A single
object can still be injected into both its push and pop slots, but the orchestrator
never depends on that.

## Strategy-style interfaces

Choose a variant by which strategy is injected, not by branching inside a
top-level type. E.g. Scott vs Church is selected by injecting the corresponding
transpiler; the top-level transpiler holds a reference and never branches on
the variant itself.
