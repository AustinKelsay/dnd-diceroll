# Ticket #5 — Spec review

Fixed baseline `bf804da5ed4d8d5e892525d0d42a5a390a701c86`; immutable candidate `accd4c070199abbc667cb50d4960cbf6a8fc80e4`. Command: `git diff bf804da5ed4d8d5e892525d0d42a5a390a701c86...accd4c070199abbc667cb50d4960cbf6a8fc80e4`. Exactly one of two independent read-only gpt-6.1-sol reviewers; no editor, builds, writes or further children.

## Report

Two code findings:

- **P2 — Saved route descriptions can contradict their encounter.** `Private/Run/ExpeditionRun.cpp`, `ValidRoute` in `Restore`, validates `Name` and `Preview` only for nonempty bounded length. Altering an offered route’s preview to describe different enemies, terrain or rewards still passes restore and is displayed unchanged. The user requires “validate … crossfield invariants” and “coherent bounded state”; the approved rules promise previews disclose the actual reward and terrain. Share canonical route-description formatting between generation and validation, and test that contradictory saved descriptions fail atomically. This needs no replay proof.
- **P2 — Older ending records cannot be inspected.** `Private/Presentation/ExpeditionBattleView.cpp:590–595` draws records backward until the screen fills, then stops. There is no paging, scrolling or selection input for the remaining records. Once history exceeds the visible rows, those durable endings become inaccessible through the UI. Ticket #5 requires the player to “inspect history after winning or losing” and “UI offers … run records.” Add a bounded keyboard paging/scroll path and display the current range.

No unrequested scope creep found. The candidate otherwise implements strict versioned serialization, candidate-based atomic restore, command-before-install checkpoints, exact retained random streams, permanent-loss preservation, distinct same-seed identities, idempotent endings and records without power.

**Acceptance evidence remained pending at this candidate:** inspected rendered PIE save/resume and ending-record interaction, followed by visible packaged save → normal quit → fresh-process relaunch → resume. The recorded **33/33 Project tests and 22/22 helper tests** support domain/file behavior but cannot establish native input or rendering. This review does not replace root’s separate final full-feature integrity review.

Spec: **2 P2 code findings; native/package acceptance pending at candidate.**

## Owner resolution

Both code findings resolved. Generated and restored route facts/text share one private profile function; contradictory saved names/previews reject atomically through snapshot and real JSON interfaces. Records now have bounded four-row pages, Page Up / Page Down controls and a visible range; a public presentation test loads ten durable records, reaches the oldest partial page and returns to newest, while proving startup/history did not overwrite the current run. Fresh build and **34/34 full Project tests, zero warnings** passed in `.local/automation/20261031T042150471725Z`. Owner also changed save/resume to unused K/O after native F5 triggered Unreal’s rendering shortcut. Native/package evidence follows in the session. No additional reviewers.
