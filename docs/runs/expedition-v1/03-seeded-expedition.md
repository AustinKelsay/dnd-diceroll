# Ticket #4 — seeded expedition session

Sole implementation writer/editor owner on `feature/expedition-v1`; fixed baseline `c1be5672308d0d604bb08c2a1f44c688ffa2fc5f`. No branch/checkout, push, PR, tracker mutations or external messages. Root owns external delivery and ticket #5. Skills: implement, TDD, codebase-design, code-review; native validation uses cua-driver.

Read requested project/domain/architecture/spec/ticket/rules and party session with both review axes; fetched full issue #4/comments via explicit repository. Doctor passed on host Python 3.15 and pinned UE 5.8.4 CL58902052. Initial status at 2026-10-31 03:11:11 UTC: no project processes or MCP listener. Source/editor ownership accepted from root.

## Decisions and seams

Approved seam: public `FExpeditionRun` commands and value snapshot. Run owns the sole battle plus reflected progress. Read-only combat queries serve presentation. Snapshot assembles both without duplicating writable battle state. Offers are generated once, retain encounter seeds and exact preview facts, and history retains selected opportunity/outcome/recipient/survivor count. Generation uses its own retained random stream. No disk adapter or writable restore interface in this slice.

Three combats. Explicit loot after the first two victories; selected conscious survivor receives +1 gear (cap 3) and route-specific potions. One take/skip recovery stage after first reward; taking it grants conscious survivors +10 HP (capped) and +1 role use/slot (capped at 2). No potions, revival, or silent rest. A dangerous route secures supplies, adding one potion to the next guarded opportunity; its preview discloses the effect. Third victory recovers artifact and ends run; wipe ends immediately. Fresh NewRun(seed) resets party power and history.

## Initial validation

Compiled fresh-start tracer failed before implementation: `.local/automation/20261031T031212025640Z` (three expected assertions). Build evidence: `.local/run-red-build.log`.

Fresh-start green: `.local/automation/20261031T031255919962Z` (1/1). Route red: `.local/automation/20261031T031333521283Z`; route green: `.local/automation/20261031T031441381476Z` (2/2). Full-flow stub red: see `.local/run-flow-red-test.log`; green `.local/automation/20261031T031721887737Z` (3/3). Initial expanded full Project run: `.local/automation/20261031T031825115622Z`, 25/25 with zero warnings. Offline helpers: 22/22 in `.local/run-helper-tests.log`. Added skip-rest and victory-reset/terminal checks afterward. Engine-generated unrelated AndroidFileServer defaults removed from tracked config, as in previous slices.

Implementation composes `FPartyContinuity::Carry/Reward/Recover`; legacy Advance remains an atomic compatibility wrapper for party tests. Run exposes only const Battle/Progress and value Snapshot. Rejected commands validate phase/recipient before mutation; route start validates a candidate copy before replacing authority. No asset changes or helper rewrites. Runtime board tracks/destroys tiles/obstacles/units on each layout rebuild, and resets framing for generated grid dimensions.

Independent two-axis review and native acceptance were pending at this initial candidate; completed results follow below.

## Candidate checks and review

Candidate `645cdd49697d782c9c08f626a29c78a46398fe25`: `.local/run-candidate-build.log` fresh Development editor build; `.local/automation/20261031T031929460873Z` **26/26 full Project tests, zero warnings**; 22/22 helper tests. Tests cover 256 opportunities across seeds -32..95, full floor connectivity and actual occupied-spawn target access, bounded variation, repeatable public play/RNG, phase rejection atomicity, truthful history effects, reward exactly once, camp take/skip, actual death through smaller-party victory/camp/continuation, finite AI, three victories/artifact terminal, wipe terminal, and fresh reset after earned gear. Existing combat/party/adapter/starter tests remain passing. `.local/bootstrap/20261031T032002958939Z.log`: existing sandbox preserved and validated with zero errors/warnings. One attempted bootstrap while automation still owned the helper lock was safely refused; retried after completion.

Exactly two native foreground-session read-only gpt-6.1-sol reviewers inspected the pinned three-dot diff. Separate [Standards](03-seeded-expedition-standards.md): zero hard violations, two optional cleanup smells. Separate [Spec](03-seeded-expedition-spec.md): zero code findings; native acceptance pending at candidate. No additional review delegation.

## Public run and later serialization contract

`FExpeditionRun::NewRun(seed, error)`, `ChooseRoute(routeId, error)`, `ClaimReward(survivorId, error)`, `Recover(take, error)`, `Submit(combatCommand)` and `PlayEnemyTurn()` are the only mutating run commands. `Battle()` and `Progress()` expose const queries; `Snapshot()` assembles a detached reflected `FExpeditionSnapshot`. The run owns one `FCombatBattle`; callers cannot obtain a mutable battle. Generation and combat streams are independent and both random states are retained. Route commands accept retained IDs rather than arbitrary seeds/layouts.

The composite contains `Progress` (original seed, deterministic EXP identity, generation stream state, phase, victories, current offered opportunities including encounter seeds/recipe/dimensions/enemy count/reward/risk, chosen history/outcomes/recipients/survivor counts, camp resolved/used and artifact flags) plus `Battle` (exact layout/combatants, stable party IDs, injuries/life/deaths/countdowns, inventory/resources/equipment, initiatives/order/index/round/action budgets and combat RNG). History includes the in-progress selected encounter, then records its settled outcome once. Four roster IDs remain present even after death. No Actor/UObject pointers enter this contract.

Ticket #5 must add a versioned, validated restore/serialization adapter to this seam; it must validate/reconstruct private initialized flags and cross-field run/combat invariants, then install both values atomically. Do not call `NewRun`/`Start` to restore, as those roll initiative/generation. There is no permissive writable snapshot or disk persistence in #4. Snapshots exported after successful `NewRun` represent initialized runs; the initial Route stage holds a fresh staging battle for party/board preview until a retained opportunity is selected. Restore must preserve that phase gate as well.

## Native input evidence

Editor PID 61677 verified exact project/path, UE 5.8.4 CL58902052, clean packages. Sequential discovery/described EditorAppToolset; StartPIE floating. An initial raw call used a fully-qualified tool name where the dispatcher requires separate toolset/bare tool names and was refused without mutation; corrected from the installed helper interface. CUA session `seeded-expedition` bound exact PIE window 44464. Background keys were refused with `same_pid_keyboard_ambiguity`; documented exact-window foreground fallback delivered real keys. A bounded background/foreground mouse probe was a no-op; no prolonged retry or mouse-success claim. All gameplay below is keyboard input, with state snapshots before/after and inspected PNGs; no rules-state mutation or console cheats.

- `.local/captures/run-initial.png`: seed 20261030 / EXP-013528A6, fresh four-role party and both retained route/risk/terrain/reward offers readable.
- `.local/captures/run-first-0-z.png`: guarded choice starts encounter 1 on a generated 9x7 ruined-pillar layout; actual movement/targets rendered. Sera's arrows+Enter move and miss in `run-sera1-3-return.png` / `run-sera1-7-return.png`; wizard cast and slot expenditure in `run-ivo-missile-9-return.png`.
- `.local/captures/run-finish1-11-return.png`: real first victory (all four survivors), explicit reward gate and 1/3 victories.
- `.local/captures/run-camp-1-l.png` / `run-camp-2-l.png`: Arden receives +1 gear and one potion exactly once; repeat L rejected. `run-camp-3-c.png` / `run-camp-4-c.png`: explicit recovery restores Sera 20→22 and Ivo 0→1 slot; repeated C rejected, camp USED and next offers retained.
- `.local/captures/run-camp-5-z.png`: encounter 2 begins on a new 8x8 split-stone layout with gear/potions/camp state carried. `run-ivo2-9-return.png` shows Ivo's restored slot spent, dealing 15 to kill the archer. `run-sera-nix2-10-return.png` shows qualified Sneak Attack and `run-arden2-6-return.png` shows second victory, 2/3.
- `.local/captures/run-finale-route-0-l.png` / `run-finale-route-1-c.png`: second reward grants Arden gear +2 / three potions; no new camp, C rejected. `run-finale-route-2-z.png`: third encounter explicitly marked FINALE; wizard remains at zero slots.

Native inspection found the 8-row board slightly overlapped the bottom HUD and the reward caption could blend into its bright floor. Owner adjusted framing and added dark reward-caption backing after the verified close; final fresh render inspection is recorded below. The first two native victory flows above are on the reviewed candidate.
- `.local/captures/run-final-nix-7-return.png`: actual finale finishing Sneak Attack against the first guardian, with +2 earned gear on Arden and zero wizard slots retained. Basic attacks/remaining party resources must finish the final guardian; no implicit restoration occurred.

- `.local/captures/run-final-sera-6-return.png`: actual finishing hit kills the last guardian, updates victories to **3/3** and displays **ARTIFACT RECOVERED / Expedition complete**. All three combats were played through real player input; no API state served as native proof. `.local/captures/run-artifact-editor.png` is the separately discovered MCP full-editor capture, also inspected; the CUA window image has larger readable HUD text.
- `.local/captures/run-fresh-0-n.png`: real N at the ending starts seed **20261031 / EXP-013528A7**, resets gear +2→0, potions 3→1, wizard slots 0→2, full HP, 0/3 victories, camp availability and fresh retained offers. No permanent power carries.

At **2026-10-31 03:50:16 UTC**, `.local/run-before-close.json` verified exact PID 61677/path/engine, PIE stopped and `dirty_packages: []`. Normal helper close exited that exact PID (`.local/run-close.log`) before owner source cleanup. Other project browser untouched. Manual severity-field scan found no Error/Fatal entries in this native editor log.

## Owner follow-up

Both Standards smells were resolved locally: one private recipe definition now supplies its preview name, blocked cells and spawn offsets, and one survivor-count query serves encounter entry/settlement. Generated layouts/random draws are unchanged. Camera framing now accounts for eight-row boards and shifts the view slightly above the bottom panel; a dark reward-caption backing improves contrast. No runtime rules were moved into presentation.

Owner strengthened replay checks to complete whole expeditions across 16 seeds with both camp decisions and guarded/dangerous later choices, comparing every reflected value and both random streams after each combat. Removed an unused test-policy parameter. A new literal public assertion caught that finale snapshot metadata still carried a potion reward despite correctly previewing/awarding no loot: compiled red `.local/automation/20261031T035053629364Z` failed only the expected `Finale metadata offers no consumable loot` assertion. Finale opportunities now retain `RewardPotions = 0`; the terminal behavior remains unchanged. This makes the future serialization contract truthful at the data level as well as in UI. No further delegates/re-reviews were started.


## Final validation and handoff

Implementation commits: `645cdd49697d782c9c08f626a29c78a46398fe25` and owner follow-up `a13fafaa8a11d804a0e2915ccabfde9df0b000ea`. Final fresh Development editor build succeeded (`.local/run-final-build.log`). Full **Project automation: 26/26, zero warnings** in `.local/automation/20261031T035207894828Z` / `.local/run-final-test.log`. The helper code is unchanged and its **22/22** tests passed in `.local/run-helper-tests.log`. Sandbox bootstrap preserved the existing binary asset with zero errors/warnings. Final fresh editor smoke (`.local/run-final-smoke.log`, 2026-10-31 03:53:35 UTC) started/stopped PIE, verified clean packages and reported zero errors plus the known host AudioUnit sample-rate warning `2003332927`.

Inspected `.local/captures/run-final-framing.png` on fresh final-code editor PID 89957, exact floating PIE window 44494: the eight-row staging grid fits above the bottom HUD and route/party/objective text remains readable. Final post-relaunch Z input probes, including exact-window foreground focus, did not deliver a route transition. Captures named `run-final-generated-frame.png` and `run-final-route-confirmed.png` therefore show Route staging, not a new generated combat. They are not replay evidence. The earlier full native three-combat playthrough, artifact ending and N reset remain the composition evidence; owner changes afterward were recipe/count cleanup, truthful finale reward metadata and framing/contrast. No extra review agents were launched.

Native wipe and smaller-party continuation remain automation-only for this slice (the prior party session has native smaller-party evidence). Mouse CUA delivery remains unreliable. No package repeat was required for this intermediate ticket; root retains final packaging/packaged play acceptance and ticket #5 disk adapters. These limits do not replace or weaken the completed keyboard composition evidence.

At **2026-10-31 03:57:40 UTC**, `.local/run-final-before-close.json` verified exact project/path, pinned UE 5.8.4, PID **89957**, PIE stopped and `dirty_packages: []`. The safe helper closed that exact PID normally (`.local/run-final-close.log`). Final status at **2026-10-31 03:57:45 UTC** (`.local/run-final-status.json`) showed **no associated project processes and no MCP listener**. CUA session ended. Other project browser untouched. Engine-generated unrelated AndroidFileServer defaults were removed from tracked config after close; no binary assets changed.

Source/editor ownership is returned to root. Ticket #4 implementation and its two review axes are complete locally. Root owns final integrity review, ticket #5, tracker/PR delivery and final package acceptance. No branch/checkout, push, PR, issue mutation or external message was performed by this worker.
