# Feature run: journey checkpoint

- Issue/spec and approved acceptance: spec [#239](https://github.com/AustinKelsay/dnd-diceroll/issues/239) (passable bodies, sound overhaul, war-table journey map, finale and polish), approved by Austin on 2026-11-15. Each child ticket carries its own acceptance; the checkpoint's integrated acceptance is in the spec and is recorded in [integrated-acceptance.md](integrated-acceptance.md). The plain-language summary is [release-notes.md](release-notes.md).
- Owner, branch, baseline SHA, dependencies: the lead agent owns this run record. Children branched from `origin/staging` (`5223a0b4` or later). The release candidate is `staging` `6a1613b7` (the merge of #266).
- Granted authority and decisions still requiring the owner: agents do reviews, gates and `staging` merges under Austin's standing delegation (spec #239, "Owner and authority"). Still with Austin: whether to add a post-rescue protection for #187 item 1 (a save schema change), and his listen and playtest of the release.
- Current slice/status and next action: every ticket except #253 is merged to `staging` and closed. #253 (Landon's SRD track) is not in this release. Integrated acceptance of `6a1613b7` is done; next, the lead opens the `staging` → `main` release PR.
- Editor owner and last observed state (verify live before reuse): none. Every Unreal job uses the Ultra machine lock; see [development](../../development.md#shared-machine).

## Slice ledger

Every PR below merged to `staging` after the lead's review and a batched on-screen pass on a package. Each PR carries its own gate, commands and evidence.

| Issue/slice | PR (head → merge) | What | Review and on-screen pass | Remaining work |
| --- | --- | --- | --- | --- |
| #240 M1 move-through, revision 11 | [#258](https://github.com/AustinKelsay/dnd-diceroll/pull/258) (`50f5dfc2` → `91fd28b0`) | SRD move-through, no stopping on an occupied cell ([contract](M1-movement-contract.md)); fixes #217 | lead review; batched pass passed | none |
| #241 M2 revision 11 balance | [#264](https://github.com/AustinKelsay/dnd-diceroll/pull/264) (`62ddfa32` → `149cc2aa`) | Rescue, target choice, card refills, foes by route and stage, Monk and Paladin ([contract](M2-balance-contract.md), [report](M2-balance-report.md)) | lead review after one fix; batched pass passed | #228 and #187 item 1 misses accepted; the post-rescue protection is Austin's call |
| #242 S1 recorded cues | [#257](https://github.com/AustinKelsay/dnd-diceroll/pull/257) (`f15abb5f` → `605bbefc`) | CC0 recordings for every cue, synthesis fallback | lead review; batched pass passed | sound by ear (Austin) |
| #243 S2 missing cues | [#261](https://github.com/AustinKelsay/dnd-diceroll/pull/261) (`7ef596c5` → `b144ebe4`) | resource spend, protection, round start, finale, run end | lead review; batched pass passed | sound by ear |
| #244 S3 ambience | [#259](https://github.com/AustinKelsay/dnd-diceroll/pull/259) (`46a7de57` → `3255fb4d`) | Vigil, Root, Vault and war-table beds | lead review; batched pass passed | sound by ear |
| #245 S4 music | [#262](https://github.com/AustinKelsay/dnd-diceroll/pull/262) (`fb23fb57` → `5a2df421`) | table theme, two combat loops, finale, stings | lead review; batched pass passed | sound by ear |
| #246 S5 mix and settings | [#263](https://github.com/AustinKelsay/dnd-diceroll/pull/263) (`be631e9e` → `5f5b6a4b`) | four buses, ducking, Settings panel, set-piece cues | lead review and re-check; batched pass passed | sound by ear |
| #247 J1 journey query | [#255](https://github.com/AustinKelsay/dnd-diceroll/pull/255) (`685185cd` → `1b6b79e4`) | `FJourneyQuery` from saved state | lead review | none |
| #248 J2 journey map | [#265](https://github.com/AustinKelsay/dnd-diceroll/pull/265) (`5af99761` → `044c49ce`) | the war-table map, fog, raised dioramas, diorama press-and-release | lead review after one fix; batched pass passed | none |
| #249 J3 fly-in | [#266](https://github.com/AustinKelsay/dnd-diceroll/pull/266) (`aa35a353` → `6a1613b7`) | the fly-in into the chosen diorama, and the exposure-snap fix | lead review; batched pass passed | none |
| #250 F1 Waystone set piece | [#260](https://github.com/AustinKelsay/dnd-diceroll/pull/260) (`3ac9e191` → `b8dbaee5`) | the finale set piece (#104, rescoped) | lead review; batched pass passed | none |
| #251 P1 banners and camp line | [#254](https://github.com/AustinKelsay/dnd-diceroll/pull/254) (`a7c11f65` → `29aeccdc`) | VICTORY and DEFEAT banners; the camp line after camp | lead review; batched pass passed | none |
| #252 P2 polish | [#256](https://github.com/AustinKelsay/dnd-diceroll/pull/256) (`6ef485d3` → `a7c11f65`) | SLAIN text, banner dimming; the first click moved to #171 | lead review; batched pass passed | #171 physical-mouse check |
| #222 dev tooling | [#233](https://github.com/AustinKelsay/dnd-diceroll/pull/233) (`7dff121f` → `9205420d`) | `tools/dev.py test` disables the editor home-screen probe | lead review | none |
| #253 R1 SRD track | — | Landon's #177 and spells | — | not in this release |
| Integrated acceptance | this record's PR | gate, perf, contrast and on-screen pass of `6a1613b7` ([record](integrated-acceptance.md)) | — | the release PR |

## Handoff

- Uncommitted/unfinished work and how it is preserved: none.
- Shared asset paths and any active reservations: none.
- PR, latest reviewed/tested head, CI/bot state: recorded on each child PR; the candidate `6a1613b7` is accepted in [integrated-acceptance.md](integrated-acceptance.md).
- Parked blockers, known limits, and their owners: #228 and #187 (unowned, the protection question with Austin), #171 physical mouse, #230, #157, #174, Landon's #177 and #253. See [current state](../current-state.md#open-work-and-owners).
- Integration result and issue closure: #240–#252 are closed, #217 and #104 with them. #239 stays open until the release reaches `main`.
