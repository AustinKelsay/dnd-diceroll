# Contribution workflow

Read [CONTRIBUTING.md](../../CONTRIBUTING.md) first. This is the repo-owned execution contract for an agent, including agents with no installed skills. [Development](../development.md) owns Unreal commands and identity rules. This workflow owns contribution steps, decisions, and evidence.

## 1. Establish the session

Confirm the repository is `AustinKelsay/dnd-diceroll`, the chosen checkout and branch, the requested outcome, and who owns the editor. Read the full issue and discussion with `gh issue view <number> --repo AustinKelsay/dnd-diceroll --json title,body,comments,assignees,labels,state`. Record authority already granted; ask only for unresolved product intent, unavailable access, or actions outside that grant.

For gameplay, read the brief, architecture, asset provenance, and any available `CONTEXT.md`, `GLOSSARY.md`, and relevant ADRs before proposing changes. The approved V1 contract is `docs/specs/expedition-v1.md`, captured from its [spec issue](https://github.com/AustinKelsay/dnd-diceroll/issues/1). New work does not inherit V1's session-specific delegation.

Completion: scope, workspace, issue, dependencies, and authority are known. Leave another writer's checkout untouched. An issue in this repo is the work record; PRs deliver implementation rather than act as raw feature requests.

## 2. Inspect and update deliberately

Run `python3 tools/contribute.py preflight`. It reads local Git state, LFS setup/hydration, editor processes, and the endpoint listener. It neither fetches nor changes files. Its ahead/behind counts use the last fetched `origin/staging`; passing is an observation, not a lock or permission grant. An integration-branch notice means create a feature branch before implementation.

When there is unfinished work, inspect it and preserve it in its owning checkout. A scoped commit or a named WIP commit on the owner's branch can be appropriate after reviewing its contents. Preserve unknown or unrelated work; do not automatically stash, reset, clean, or force-push.

Fetching remote information is safe without changing working assets. Integrating updates requires Git to be clean, unfinished merge/rebase/cherry-pick state resolved, and this project's editor closed. If the editor is open, coordinate with its owner; stop PIE, intentionally save through Unreal, verify clean packages, and close through the helper. Unknown editor state is a refusal, not evidence it is closed. Recheck immediately before a workspace-changing command.

For a clean, closed integration checkout:

```sh
git fetch origin
python3 tools/contribute.py preflight
git switch staging
git merge --ff-only origin/staging
git lfs pull
git switch -c feature/<issue>-<slug>
```

For an existing feature branch, stay on that branch and explicitly merge `origin/staging` after inspection instead of switching away and losing context. Use merge for published branches; do not rewrite another contributor's history. Resolve text conflicts with the affected contract in view. Stop on binary conflicts and coordinate an Unreal resolution. Then hydrate LFS assets and repeat relevant verification. A changed source tree requires fresh build evidence before editor launch.

Each simultaneous writer uses a separate checkout. On Ultra, clones/worktrees still share the MCP port and the machine: keep one editor at a time, transfer ownership intentionally, and wrap every build, test, and package job in the machine lock (see "Shared machine" in [development](../development.md)). Do not edit the four tracked endpoint settings just to run another feature. The per-checkout operation lock only covers helper calls; it is not a session lease or a cross-machine claim lock.

Completion: the intended baseline is available, current work is preserved, the branch is intentional, and editor ownership is clear.

## 3. Choose the work path

| Starting point | Agent actions | Completion |
| --- | --- | --- |
| New idea or expansion | Align against docs, ask one product question at a time with a recommendation; prototype only for a concrete unanswered question. Draft the spec, testing seams, and thin vertical tickets with blocking edges. | Owner approves scope/testing and ticket granularity, or explicitly delegates those decisions; approved issues carry `ready-for-agent`. |
| Approved ticket | Read its parent, all comments, dependencies, and acceptance. Confirm dependencies are integrated into the agreed base, not merely marked closed. Check assignee, claim comments, and existing PRs. | An unambiguous claim names owner, branch, scope, and binary paths. |
| Small fix | Diagnose the bounded problem and record a short issue with expected behavior and relevant checks. | The issue has approval or fits recorded delegated authority; no unrelated feature planning. |

`ready-for-agent` means specified, not unclaimed or unblocked. Use `needs-triage` for incoming ideas, `needs-info` for missing information, and `ready-for-human` for a precise human-only decision/access block. Keep productive independent work moving around parked blockers. Follow `docs/agents/triage-labels.md` and `issue-tracker.md` when present; the existing repository setup does not need rerunning.

GitHub issues own approved specs, acceptance, dependencies, assignment, and live progress. Git owns versioned domain vocabulary, architecture, asset provenance, and engineering rules. Existing `docs/specs/` and `docs/tickets/` copies are captured issue snapshots: reconcile material disagreement before implementation and identify source issue/revision when refreshing them. New work need not mirror full issue bodies into Git. An unapproved comment does not change accepted scope.

Dependent work normally waits for its base feature to integrate. An exception requires an agreed published base SHA, explicit dependency, and maintainer-approved stacked PR; after integration, retarget to `staging`, inspect the resulting diff, and revalidate. Do not branch from another worker's uncommitted files.

## 4. Implement and verify a bounded slice

For editor, presentation or asset tasks, follow [editor contributions](editor-contributions.md) for the brief, preservation constraints, tool outcomes, recovery and completion report. Refine the existing approved issue; retain its authority and avoid a duplicate work record.

Record a pre-ticket commit as the fixed review baseline. Use the approved public testing seam for TDD where behavior can be specified; then implement the smallest complete slice. Reuse the safe Unreal lifecycle from development. Keep assets under `/Game/DnDDiceRoll` and provenance in `docs/assets.md`; declare existing binary paths in the issue claim before editing. Preserve the starter sandbox fixture. New feature assets should avoid shared binary edits where practical. LFS locking is an optional experiment until the team validates enforcement.

Run `python3 -m unittest discover -s tools/tests -v`. Stage only reviewed in-scope files, then `python3 tools/contribute.py check` to inspect the actual Git index. This catches raw binaries that should be LFS pointers, generated output, machine-generated config tokens, inconsistent MCP settings, and broken local Markdown links. It does not prove an asset is downloadable from the remote.

| Changed behavior | Additional evidence |
| --- | --- |
| Documentation | Review instructions/links and exercise changed commands where relevant. |
| Python tooling | Focused success/refusal tests and real read-only inspection on Ultra. |

| C++ rules/state | Pinned-engine editor build and explicit relevant automation prefix. |
| Presentation, input, assets | Saved/reloaded assets as applicable, real interaction, and inspected rendered output. |
| Engine pin, build/package configuration | Affected upgrade/package validation from development, with maintainer agreement. |

If you are not on PlebDev Ultra (for example Windows or a Mac), do not offer `doctor` or engine runs as proof. Do the source and docs work, state your host in the PR, and the maintainer verifies the Unreal work on Ultra before merge.

Use the explicit `Project.Expedition.` filter for game tests. The helper's default `Project.Starter.` and starter smoke only prove their fixture. Headless package startup alone does not prove rendering or input. Report tested commit/source state, host/engine, exact commands, counts, limitations, and accessible selected reports/captures. Bulk reports stay ignored in `.local/`, `Saved/`, or `Builds/`; local absolute paths alone are not reviewer evidence.

Completion: acceptance is demonstrated, unrelated work is excluded, and failures/limits are visible.

## 5. Review, deliver, and resume

Arrange independent review against the fixed baseline on two axes: **Standards** (repo rules) and **Spec** (approved outcome/acceptance). Use fresh reviewer sessions or another person; an author's self-check is not independent review. A harness may run axes in parallel or a fresh reviewer may do both sequentially. Fix valid findings, explain declined ones, rerun affected checks, and refresh review for substantive changes. CodeRabbit is an optional additional reviewer; unavailable tools do not count as approval or block an available independent fallback.

Commit each completed slice with its issue reference; record commit and review outcomes. Push the owned feature branch with LFS hooks active and open a non-draft PR targeting `staging` using the PR template. Check its final base-to-head diff, scope, CI, review comments, and evidence freshness. CI checks repository hygiene; local Unreal evidence establishes game claims. A maintainer (or agent with explicit recorded authorization) merges only after gates pass. Close implementation issues explicitly after staging integration; GitHub default-branch closing behavior is not the completion signal. Keep a parent spec open until its accepted work is integrated. `main` promotion is separate.

At interruption, leave enough state on the issue/PR for another session: branch/head, last completed step, unfinished work, editor ownership, checks/reviews, and next action. Multi-ticket features use [the run template](../templates/feature-run.md), one owner per feature directory. Local metadata never proves current editor state.

To resume, read those records, re-read issue/PR discussion, inspect actual Git/editor state, fetch remote information, and reconcile dependencies before integrating updates. Retest evidence affected by subsequent changes. Reassign a stale claim through an acknowledged handoff rather than assuming ownership from its age.

## Pipeline provenance and optional skills

This contract applies the Matt Pocock sequence: `grill-with-docs` → optional `prototype`/`handoff` → `to-spec` → `to-tickets` → `implement` with `tdd` where useful → two-axis `code-review`. `triage` handles incoming requests; approved generated tickets skip it. `domain-modeling` supports durable terminology/ADRs; `diagnosing-bugs` supports a tight reproduction loop. A fresh execution session per ticket is useful when supported, with the issue/run record as its handoff.

The reference is [mattpocock/skills at 2ab958093e83e0ec752e6c1c5932da465bf23e0c](https://github.com/mattpocock/skills/tree/2ab958093e83e0ec752e6c1c5932da465bf23e0c), [MIT licensed](https://github.com/mattpocock/skills/blob/2ab958093e83e0ec752e6c1c5932da465bf23e0c/LICENSE), inspected for this workflow on 2026-10-31. No upstream skill files are copied here. Optional installation must follow the upstream documentation for the chosen agent and preserve its custom skills; it must not rewrite this repo's already configured tracker or global skill stores. Record installed revision separately, since a latest-version installer is not a pin.

Invoke the applicable Matt Pocock skills directly. This document supplies the repository's Unreal, collaboration, evidence, and staging-PR rules; no personal launcher mediates the pipeline. Agents without installed skills can follow this contract directly. Historical run records describe earlier execution choices and do not select the workflow for new or resumed work.

No personal filesystem path, T3, specific model, paid review service, or skill installer is required to execute this document. Unreal verification counts only on the validated host (Ultra, Linux); Windows support is tracked in #51.
