# Menu polish: verification and native observations

Issue [#109](https://github.com/AustinKelsay/dnd-diceroll/issues/109), baseline `9b1837e657b0b87b9ce8da527cddec19f6082587`. The owner requested a full-send menu/UI pass. The implementation keeps Canvas, the themed tabletop, 70 builds, existing rules and schema 4/catalog revision 7. Only presentation/input, their public regression coverage and these records change.

## Root corrections

The first rendered builder had no visible default Enter target. Class buttons also shared identifiers with their first subclass. Root separated those IDs, retained the chosen subclass when its current class is selected, and distinguished focus with a parchment outline. Route cards now show focus and an Enter hint with stable hit regions. Shift-Tab moves backward. Back/Escape returns from the next-party draft to the result, and successful Resume returns to the saved result rather than retaining a draft screen.

A second native observation exposed stale focus registration: Help closed over the ending's builder and focus moved from Begin to Back. Refresh now runs after the HUD registers the current frame's controls. Before capture `menu109-restored-focus.png` shows the erroneous Back focus; final `menu109-accepted-restored-focus.png` shows Begin restored. The same correction makes reward and camp default to their primary action instead of a control from the preceding screen. A dark route-heading panel and a narrower camp panel address observed background contrast and party-card clearance. The route screen retains the party sidebar so current HP and resources inform route choice.

## Checks

Host: Apple Silicon macOS 26.5.2, Python 3.15, pinned UE 5.8.4 / CL 58902052. Final source fingerprint is `171a9046224974e6fe40a585d348bf2937e97eb8c51087dfeafd1e190c71e80d` (sorted `Source/` relative paths and bytes, each followed by NUL).

| Check | Observed result |
| --- | --- |
| Doctor | Passed pinned host/engine checks |
| Development editor build | Passed; no compiler warning/error lines (`.local/menu-polish/delivery-build.log`) |
| `tools/dev.py test --filter Project.` | 83/83 passed, zero warnings; `.local/automation/20261102T030223497252Z/index.json` |
| Helper unittest discovery | 50/50 passed; `.local/menu-polish/root-helpers.log` |
| Mac Development package | Built/cooked/archived; default-map startup passed with zero verification errors/warnings; `Builds/20261102T030256302140Z/verified.json` |
| Staged contribution check and whitespace | Passed on the scoped source and evidence files |

The earlier source-unit record retains its own intermediate failures. Root's premature test invocation during a running build was refused by the helper lock; after the build completed the full suite passed. The native driver rejected the key name `slash`; the documented literal `/` worked. Generated AndroidFileServer config suffixes were removed only after verifying the exact tracked prefix. No endpoint settings, original editor, binary assets or user checkpoints were changed.

The owned checkout has no associated editor process. The shared MCP port remains occupied by the original checkout's editor PID 50002, so preflight cannot certify exclusive MCP access; that editor was not operated on. Owned QA apps were closed normally after observation, except the recorded engine crash below. The native automation session was ended.

## Rendered evidence

Root inspected actual native captures with keyboard input using the cua-driver skill. The package was cloned and ad-hoc signed under unique QA bundle IDs; only bundle display/identity/signature changed. The game binary and cooked content come from the verified package. QA saves live in those isolated app containers. Existing real `Project.Tools.CaptureFixtures` checkpoints supplied noncombat phases; they were intentionally copied only into owned QA storage, not represented as a human playthrough. Pagination used five QA copies of a valid ending record with independent run IDs.

At 1280×720 content size, inspected startup, all ten visible Wizard subclass choices, two-action previews, contextual Help, empty Records, missing-checkpoint feedback, reward recipient changes, camp recovery, route comparison, victory and defeat recaps, next-party editing, Back, and five-record pagination. Final captures confirmed primary focus on reward/camp, the separated camp panel, the route-heading backing, and Help focus restoration. Selected local artifacts use `.local/menu109-*.png`; the authored observations here are the portable evidence summary.

Final-source package observations:

- **1440×900 content:** startup hierarchy and previews fit; fresh-process Resume restored the four saved builds, health and resources, with both route cards and the party sidebar visible (`menu109-actual-1440x900.png`, `menu109-final-resume.png`). Checkpoint SHA-256 remained `72166e9bdf10e81ddaae799e3a7db0aa37c234c46b7df65bec035083deaa3859` across Resume. Enter on the focused first route started combat (`menu109-final-route-enter.png`). This is a real native route transition, not a fixture substitution.
- **1600×640 content:** all 13 class labels, all ten Wizard subclasses, both Enchantment action descriptions, primary buttons, feedback and contextual Help fit (`menu109-wide-choices-1.png`, `menu109-wide-help.png`). The window was launched with exact dimensions; the observed native frame was 1600×672 including its 32-pixel title bar.
- Earlier native resize attempts preserved the window aspect ratio, yielding **1440×810** and **1137×640**, not the requested dimensions. Those observations are supplemental only. Exact-size launches supplied the required final checks.

Independent Standards/Spec review and staging integration are still pending at this checkpoint.

## Limits and recurrence

Pointer attempts at the visible Wizard class control were dispatched in background and foreground modes but produced no visible selection change. Native pointer acceptance remains unverified; keyboard activation and public handler tests provide separate evidence. Shift-Tab injection also behaved like forward Tab in both delivery modes; physical modifier-key behavior is unverified. Ordinary Tab, Enter, opposite-arrow selection and modal close/restoration were observed. No gamepad, larger-text setting, sound-by-ear, human balance or production signing/notarization claim is made.

A QA clone of package `20261102T025141058796Z` closed after the camp-to-route flow and before the next input. The macOS report shows `FDebug::CheckVerifyFailedImpl2` → `FSceneCullingBuilder::ProcessPostSceneUpdate` on worker thread 12, followed by SIGSEGV. This matches the path tracked in [accepted-risk #32](https://github.com/AustinKelsay/dnd-diceroll/issues/32#issuecomment-6567976336); missing assertion/live renderer state prevents establishing an identical cause. Evidence: `.local/menu-polish/DnDDiceRoll-2026-11-01-205638.ips` and `accepted-before-scene-crash.log`. One unchanged-package restart succeeded, with checkpoint bytes preserved, and final menu QA continued. No engine fix is claimed. Existing #35 launch risk and #105 material warnings also remain outside this menu change.
