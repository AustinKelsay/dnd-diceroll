# Packaged startup crash in the LLM tracker (#35)

[Issue #35](https://github.com/AustinKelsay/dnd-diceroll/issues/35). Status as of 2026-11-09: **cause explained from preserved evidence and the pinned engine source, not validated. Mac only. Open, waiting for the next Mac package.**

## What fails

A packaged macOS Development build sometimes crashes about 0.2 to 0.5 seconds after launch, before any game log line. The crash report ends in a deliberate engine exception (a fault at address 3) raised from the Low Level Memory Tracker's tag pop.

## What six preserved reports show

All six `FLLMTracker::PopTag` reports (two from 2026-11-02 diagnostics, four earlier) show the same two threads at the same instant:

- **Main thread (the one that crashes).** AppKit is handling the "open" Apple Event that launches the app. LaunchServices allocates through the engine allocator. The allocator's slow path holds an LLM scope, and when that scope ends the tag pop fails an engine check.
- **Game thread (sampled, not crashed).** Engine start-up is processing the command line and tearing the tracker down, in the middle of freeing the tracker's maps.

## Why it is expected from the source

Reading UE 5.8.4 (CL 58902052), with no engine text copied here:

- The tracker is compiled into every build except Shipping. With the engine's default settings it stays on only if the command line carries `-LLM`.
- A normal packaged Development launch has no `-LLM`, so every launch tears the tracker down during start-up. The teardown deletes the per-thread tag state and the thread-id map.
- Only the store of the enabled flag is locked. A scope that another thread opened earlier is not waited for, so when it ends its pop finds no valid thread state and the check fails.
- macOS is the one host where the Cocoa main thread and the game thread run at the same time during start-up. That explains why it is intermittent and Mac-only. A Linux run cannot show it.

## Expected consequence

LLM is compiled out of Shipping builds, so this race should not exist in a Shipping package. It would then be a **Development-package problem on Mac only**. This is expected from the source and has **not** been checked on a built package.

## What was ruled out or does not apply

- Not related to the scene-culling assertion in [#32](32-renderer.md).
- The earlier idea of an `-nollm` comparison does not apply: with these defaults that flag is never read, and the disable path is already the default.
- A project cannot patch this. The engine is an installed build and the tracker lives in its precompiled core.

## Next action

No Mac work is scheduled, because building and testing moved to Ultra on 2026-11-07. The next time a Mac package is made, run these checks, cheapest first, with N launches in each mode against a baseline of the same package:

1. **Shipping package.** The expected fix, and the shape a real distribution would take.
2. **`-LLM` on the command line** (for example in the bundle's command-line file). It keeps the tracker enabled so the teardown does not run. This is a Development-only comparison.
3. **Direct executable launch versus `open`.** The crash path goes through the launch Apple Event, so this shows whether that path is required.

## Where the evidence is

Private and ignored, on Ultra: `.local/stability32-ultra/issue35-llm-startup-race-20261109.md` in the stability checkout (it names engine source files, so it is not published). The six crash reports are in the preserved Mac evidence archive (`stability32-essential-20261102.tar.gz`, SHA-256 `42c6136b5bbf1bb65bcb2d7033b2820488f5a25738a958b1c072d9165237f657`). The Mac diagnostic branch is `feature/32-renderer-stability` at `6c2f9b7`.
