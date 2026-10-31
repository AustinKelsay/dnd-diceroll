# Ticket #3 — Spec review

Fixed baseline `7c119bace4821ac04c7cf27a80e5ea9175aa8aaf`; candidate `56a4766`. Command: `git diff 7c119bace4821ac04c7cf27a80e5ea9175aa8aaf...56a4766`. One read-only gpt-6.1-sol reviewer, no editor access or file writes. This is one of exactly two independent review axes. Scope is ticket #3 only.

## Report

No code-level findings in the candidate diff.

The candidate implements the ticket’s distinct role actions, validated costs and targets, separate Action/Bonus Action budgets, downed countdown and rescue, permanent death, survivor continuation, wipe, persistent party ownership, and useful equipment/consumable rewards. Invalid commands and failed transitions validate before mutation; dead IDs are retained and cannot regain health through the transition layout.

The deterministic rescue and unresolved-downed-at-victory policies are permitted adaptations and are explicitly documented. The simple repeated-encounter fixture fits the ticket’s allowed scope. Generated routes, camp recovery and save/resume remain appropriately deferred.

The acceptance line “Compile and observe rendered role actions, status feedback, and continuation/end-state behavior” remains **pending owner evidence** at this candidate: the session records successful compilation and automation, while native play verification is still underway. This is an outstanding verification step, not a functional omission found in the code.

Spec: **0 code findings; native acceptance evidence pending.**

## Owner validation

Owner completed real keyboard-driven PIE play of all four role features, healing cap and potion use, downing and rescue, countdown expiry to permanent death, victory, dead-recipient rejection, smaller-party continuation with carried injuries/resources/death and single loot, and repeated-transition rejection. Detailed inspected captures and exact limitations are in [the session](02-party-consequences.md). Wipe remains automation-verified; no intermediate package repeat was required. The native acceptance step pending at review is complete. No extra review delegation.
