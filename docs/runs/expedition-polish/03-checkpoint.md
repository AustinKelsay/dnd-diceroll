# Expedition checkpoint — 2026-10-31

The user requested this checkpoint after explicitly putting the rendering investigation on hold. Resume work from this record and the user's next instruction.

## Repository and delivery

- Repository: `AustinKelsay/expedition-rpg`; branch: `feature/expedition-polish`.
- Starting head: `aba3d6b3d0ad639ee2b5fbc977cfa75e5490de72`. This checkpoint changes documentation only.
- [PR #27](https://github.com/AustinKelsay/expedition-rpg/pull/27) targets `staging` and remains open and unmerged. [Issue #26](https://github.com/AustinKelsay/expedition-rpg/issues/26) remains open.
- V1 and the presentation polish are implemented. Shared-skills reconciliation is complete, with the original customizations preserved locally. [The delivery report](02-delivery.md) owns the detailed changes, review findings, validation and backup references.

## Paused blocker

**A1 remains unresolved, and staging merge remains blocked.** The pause is not risk acceptance or merge approval. Await an explicit request before resuming this investigation; main promotion and production deployment remain outside this work.

The sole known incident is the agent-observed rendering assertion in `TBufferScatterUploader::Reserve` (`UploadDataProxy == nullptr`), reached through `FSceneCullingBuilder::ProcessPostSceneUpdate`. The user reports no additional crash during their own playtesting. The matching-executable reproduction and subsequent rebuild, GC and scheduling probes did not reproduce the assertion. No cause or fix is established. S1–S5 are resolved, including the rendered fallen-party label check.

## Evidence at checkpoint

- Three independent Sol-loop rounds: Standards and Spec source approval; A1 remains the runtime acceptance blocker.
- Latest clean functional validation: pinned UE 5.8.4 / CL 58902052 editor build, **41/41 Project automation with zero warnings**, and Mac Development build/cook/archive/startup. Clean package: `Builds/20261031T211242113261Z/verified.json`.
- Latest helper/contribution run: **50/50 passed**, plus doctor, contribution check and preflight. GitHub CI passed on `aba3d6b`.
- Mac Development and keyboard interaction are the validated paths. Sound-by-ear and complete automated mouse targeting remain verification limits.
- At 22:30:49 UTC, project status reported no associated editor process or MCP listener. Owned diagnostic apps and debuggers had already exited; registrations and CUA sessions were cleaned up. Recheck live state on resumption.

Generated packages, diagnostic builds, captures and logs remain ignored locally; shared-skills backups also remain local. They are not included in the Git checkpoint. The clean game executable was hash-verified unchanged after diagnostics; no temporary diagnostic source remains under `Source/`.

## Resumption

1. Read the user's new direction, this checkpoint and the current issue/PR discussion. Inspect the actual branch, working tree, remote refs and editor ownership under [the contribution workflow](../../agents/workflow.md).
2. Preserve the merge block while A1 is open. If diagnosis is requested again, consult the [recorded probes](02-delivery.md#lifecycle-and-scheduling-probes--2026-10-31) and capture the actual assertion with live renderer state before selecting a fix. Successful stress runs establish non-recurrence only.
3. Continue any newly authorized game work as a separately scoped contribution, with the unmerged polish dependency made explicit. No new gameplay scope was selected at this checkpoint.
