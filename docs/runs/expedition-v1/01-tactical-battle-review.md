# Ticket #2 — independent review axes

Baseline: `bca18be64b76ab7c0cac1cae71915ddccad489b2`.
Candidate: `e8b21f5` (`git diff bca18be64b76ab7c0cac1cae71915ddccad489b2...e8b21f5`).
Exactly two independent, read-only, same-model gpt-6.1-sol reviewers ran, one per axis. Neither operated the editor or modified files. Later V1 slices were explicitly excluded. The owner resolves findings; no additional reviewer delegation.

## Standards

No documented-standard violations found. The change retains one runtime module, separates authoritative rules from presentation, uses reflected strong UObject references, validates commands before mutation, and documents asset provenance and rules adaptations.

Two judgment-call smells:

- Possible Duplicated Code — attack eligibility. LegalTargets and AI firing-cell planning repeat the opposing-side, positive-HP, Manhattan-range and sight predicate. A private predicate would keep preview, validation and AI planning aligned.
- Possible Duplicated Code — party-card layout. Controller hit geometry and HUD rendering separately encode logical width, card positions and spacing. Shared constants would prevent clickable areas drifting during tuning.

Resolution: extracted private `CanTargetFrom` used by both rules query and AI planning; shared HUD design width and party card geometry constants used by controller and HUD. Existing public behavior tests exercise targeting and enemy decisions after this change.

Standards: 0 hard violations, 2 optional smells, both resolved by owner.

## Spec

One P2 code finding: rejected party input resets EnemyDelay in Execute, so requests faster than once per second can indefinitely postpone Tick's enemy action gate. This conflicts with “bounded legal enemy turns” and the V1 story “I want enemies to take legal, understandable turns without hanging.” Remove the unconditional reset or restrict it to an accepted transition.

No other code-level omissions, incorrect implementations or scope creep found for ticket #2. Deferred roles, rescue/death, routing and persistence belong to later slices. Native keyboard/camera/end-turn and final teardown evidence were still pending at review time.

Resolution: removed the unconditional reset; Tick alone owns its presentation pacing. Added `Project.Expedition.Presentation.RejectedInputCannotStallEnemy`, which spawns a real runtime board in a transient world and submits rejected party commands between frames. It failed on the candidate, then passed after the fix. This is an adapter regression test, separate from the agreed authoritative combat seam.

Spec: 1 code finding, resolved and regression-tested by owner. Final visual evidence is in the ticket session. During final native verification the owner additionally corrected game viewport mouse capture: the pinned engine suppresses primary game mouse input in NoCapture mode; capture now lasts only while a button is held. Temporary diagnostics were removed before final validation. The owner also added a bounded keyboard grid cursor (arrows/Enter) through the existing cell dispatch after the review; this adapter-only addition was owner-inspected, compiled, covered by the final project suite, and exercised through actual native input. No extra review agents were started.
