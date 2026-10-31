# Expedition presentation polish — 2026-10-31

Branch `feature/expedition-polish`, based on `feature/expedition-v1` (PR #6). The user asked to make the V1 bones feel better without new game design decisions and to defer committing to external assets. Rules, balance, content and the save format are unchanged; everything is built from engine primitives and synthesized audio.

## What changed

- Combat results now carry presentation facts (actor, target, path, dice in roll order, downed/died). `PathTo` and `Forecast` are read-only queries. Random-number order is unchanged.
- `FExpeditionPlayback` replays committed results: walking, lunges, projectiles, impacts, floating numbers, falls, rescues, turn banners and the dice tray. Tab skips; T cycles 1x, 2x and instant.
- Canvas HUD with clickable buttons: party cards, initiative strip, action bar, hit-chance forecasts, path preview, target markers, keyboard cursor, and screens for title, routes, spoils, camp, endings, records and help.
- Themed boards per existing route (meadow stream, dusk ruins and watchtower, torch-lit vault with the Waystone), per-role figures, lighting, fog and grading, camera smoothing, right-drag orbit and screen-relative arrows.
- Synthesized sound effects.
- UX: commands always act for the adventurer whose turn it is; selecting another card only inspects or chooses the loot recipient.

## Review

A fresh read-only review of the full change found no change to random-number order or rule outcomes and confirmed playback convergence. Its findings were fixed: HUD overlays (help, records) let clicks reach buttons hidden underneath; rejected input skipped running animations; the drop warning ignored critical hits; the fixture tool hid write errors; and assorted stale comments, hard-coded encounter counts, origin-relative picking, an integer-overflow hash, raw HUD pointers and dead code. One finding (a stale display after a failed enemy checkpoint) does not occur, because a failed checkpoint commits nothing; that path now flows through playback anyway. Rendered play also showed that F1 toggled Unreal's wireframe view mode in development builds, so Help is bound to `?` and the Help button only.

## Evidence

- Editor build passes with no warnings.
- `Project.Expedition.`: 36/36 passed, zero warnings, including `Combat.PresentationFactsAndForecast` and `Presentation.PlaybackConvergesOnCommittedState`.
- Helper tests: 22/22.
- Mac Development package built, cooked, and entered the startup map.
- Rendered standalone play with real keyboard input: title, route choice, movement with path preview, an attack with the dice tray and damage, enemy turns, and resumed fixtures for spoils, camp, vault combat, victory and defeat. Captures live in ignored `.local/captures/polish/`.

## Limitations

- Sound was not verified by ear; no audio errors were logged beyond the known macOS sample-rate warning.
- Mouse clicks could not be delivered reliably by the automation tool, so mouse play was only partly observed; the keyboard path was exercised fully.
- Screenshot capture was too slow to catch frames mid-animation; playback timing and convergence are covered by automation.

## Continuation handoff — 2026-10-31

- Branch: `feature/expedition-polish`, based on V1 delivery commit `ac2c615acf8a62e7a01f171e8351736c6e75f1db`. The presentation changes and this handoff remain uncommitted at this check; no polish PR has been opened. Preserve the working tree before starting another feature.
- Latest validated Mac app: `Builds/20261031T162117660113Z/Mac/DnDDiceRoll.app`; startup evidence is in the same build's `verified.json`. The 36-test Expedition automation report is `.local/automation/20261031T162040213376Z/index.json`. These are recorded execution evidence; engine automation and packaging were not repeated during the factory reconciliation.
- Root owns editor coordination. Fresh helper status at 19:30 UTC reported no associated editor process or MCP listener. Recheck before taking ownership; process state is not a durable guarantee.
- Factory reconciliation: the Plebdev launchers are retired, and new work uses Matt Pocock skills directly under the repository instructions. README and AGENTS now route continuation here rather than into the completed V1 ledger. The generated Android file-server config block was removed without changing authored settings.
- Fresh checks: project doctor and 22/22 helper tests passed; the factory's static checks and isolated skill-automation tests passed. Its full `check:skills` audit stopped because `/Users/plebdev/Desktop/Skills/skills` contains local modifications. Those edits were preserved; source/store synchronization is not certified by this check. Installed skill availability and retired-launcher absence were checked separately.
- Remaining delivery work: commit the polish as its own reviewed change and prepare its PR with the correct V1 dependency/base. The older V1 PR does not contain these uncommitted changes. Existing mouse/audio verification limits above remain applicable.
