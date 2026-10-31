# Ticket #5 — Standards review

Fixed baseline `bf804da5ed4d8d5e892525d0d42a5a390a701c86`; immutable candidate `accd4c070199abbc667cb50d4960cbf6a8fc80e4`. Command: `git diff bf804da5ed4d8d5e892525d0d42a5a390a701c86...accd4c070199abbc667cb50d4960cbf6a8fc80e4`. Exactly one of two independent read-only gpt-6.1-sol reviewers; no editor, builds, writes or further children.

## Report

No documented-standard violations found in the candidate.

The change preserves ADR-0001’s authoritative command/snapshot ownership, keeps persistence outside presentation, retains the existing runtime module, and introduces no persistent unreflected UObject references. Restore validates detached candidates before installation; checkpoint commands commit disk state before changing the live run. Tests exercise the public restore, serialization and session seams, with isolated filesystem fixtures.

One optional judgment-call smell:

- **Possible Duplicated Code — generation facts repeated in restore validation.** `Private/Run/ExpeditionRun.cpp`, `OfferRoutes()` and `Restore()`’s `ValidRoute` lambda at lines 215–222 independently encode guarded/dangerous enemy counts and reward rules. For example, generation uses `Route.EnemyCount = Index == 1 ? 3 : 2` while restore checks `R.EnemyCount == (Choice == 1 ? 3 : 2)`; both also separately calculate the supplies/finale potion rule. Routine encounter tuning could update generated saves while leaving their validator incompatible. A small private route-profile definition or shared construction/validation helper would keep those facts local without adding a general framework.

**Standards: 0 hard violations; 1 optional smell.** The duplicated route facts are the only maintenance concern identified. Rendered and packaged acceptance remains the implementation owner’s validation work; this review performed no editor operations, builds or writes.

## Owner resolution

Resolved by one private route-description/profile function shared by generation and restore validation. Restore now compares all retained route facts and canonical text, also resolving the separate Spec preview finding. No general generation abstraction was introduced. Fresh build and all **34/34 Project tests, zero warnings**, including contradictory saved-preview rejection, passed in `.local/automation/20261031T042150471725Z`. No additional reviewers.
