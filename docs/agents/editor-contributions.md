# Bounded editor contributions

Use this recipe for editor, presentation or asset work within the
[contribution workflow](workflow.md). It supplies the task brief and handoff detail;
the issue still owns acceptance and approval, and [development](../development.md)
still owns lifecycle, identity, discovery, saving and evidence rules. It applies to
any agent without requiring a particular model, plugin or generation service.

## Brief and checkpoint

1. Draft the [task brief](../templates/editor-task.md) into the existing issue using
   the contributor's description and approved decisions. Name the observable
   outcome, preserved behavior, paths and required approach. Resolve only decisions
   outside existing approval or delegated authority. Completion: each acceptance
   criterion has a repeatable check and any manual check has an owner.
2. Confirm the issue claim and checkout/editor ownership through the workflow.
   Record a recovery point covering this slice's starting state. In an owned clean
   checkout, the current commit can be that point; preserve any owned WIP explicitly.
   Completion: the agent can identify its own changes separately from pre-existing
   work, and exact shared binary paths are reserved before edits.

## Implement and demonstrate

Work in batches small enough to inspect and recover individually. For new playable
content, validate the blockout and behavior before dressing it; for an art-only task,
use the accepted behavior as the starting point. Import candidates through
[asset intake](asset-intake.md). Apply the lifecycle and discovery sequence from
development before editor mutations.

Read every mutation's returned result, including compile/import/save status, before
dependent work. An ambiguous response leaves the operation unresolved. Inspect
live state and new logs before retrying; a timeout is not proof that nothing changed.

When a required tool, asset or approach is unavailable, report the failed step,
observed state, effect on acceptance and a proposed alternative. Get a scope decision
only if it exceeds existing authority. Continue independent work within the approved
brief. Do not silently substitute another implementation for a required approach or
claim the original requirement passed.

Completion: relevant checks demonstrate the brief's criteria and preservation
requirements. Inspect rendered output for visual claims and exercise real input for
interaction claims. Include a relevant edge case, such as cancellation, invalid
input, defeat or restart, when it can affect this slice. A contributor's playtest
judgment about feel remains distinct from automated correctness checks.

## Reassess unsuccessful fixes

Use a concrete bug report: reproduction steps, expected/actual result, source state,
host/engine, relevant seed/save context and selected evidence. Establish a likely
cause from that evidence before making the next bounded change.

After two unsuccessful fixes of the same acceptance failure, or the brief's earlier
experiment bound, stop adding speculative fixes. Record what each attempt changed,
what was observed and which hypothesis remains. Inspect the diff, tool state and
logs; choose a smaller diagnostic experiment, restore only the owned attempt, or
request the specific missing product decision/access. Fresh evidence may justify
another attempt; record the reason before continuing.

Recovery covers only attributable work. Keep another writer's files, history and
editor session intact. If ownership is uncertain or changes overlap, coordinate
before reverting. Restore binary packages intentionally through the established
Unreal/Git workflow with the editor owner; do not replace files behind a running
editor. Broad reset/clean/stash, blind timeout replay and automatic rollback are
not recovery procedures.

Completion: attempted changes and evidence are preserved, the remaining failure is
explicit, and the next action has a reason and an owner.

## Report and hand back

Fill the [completion report](../templates/editor-report.md) after verification and
update it after review fixes. Use the existing issue/PR or feature run record rather
than creating a duplicate ledger. Every changed file or asset is accounted for,
acceptance has evidence or an explicit gap, and the next owner can distinguish
observed state from assumptions. Continue independent Standards/Spec review and
staging delivery through the contribution workflow; the report is not an approval.
