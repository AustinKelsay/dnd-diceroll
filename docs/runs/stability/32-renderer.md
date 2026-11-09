# Recurring scene-culling assertion (#32)

[Issue #32](https://github.com/AustinKelsay/dnd-diceroll/issues/32). Status as of 2026-11-09: **unresolved, parked. A Mac-side risk with no Mac work planned.** No fix, workaround or production change exists.

## What fails

The packaged macOS game sometimes asserts in the renderer's scene-culling update shortly after a Resume (leaving the menu back into a run). The assertion says that one of the builder's GPU-upload helpers already holds data when it is asked to reserve space.

## What was captured (Mac, 2026-11-02)

A debugger-armed native loop caught one exact failure and kept the process stopped for inspection. Nine launch attempts in all: 1 exact failure, 3 completed runs with no recurrence, 5 inconclusive (two of those were the unrelated [#35](35-llm-startup.md) startup crash). These unequal and instrumented trials give no usable crash rate.

At the stop:

- The failing helper is the first of the builder's four upload helpers (block data). The other three correctly held nothing, and the dirty-block count was zero.
- The "already holds data" pointer pointed at a task object, not at upload arrays. Upload-array memory should start with an array pointer, not a task vtable.
- The task running was the scene's own post-update task, so a simple "wrong task handle" story is weakened.
- The builder and scene objects were consistent with each other (the scene's active-builder field matched the asserting builder), which weakens a freed-builder story.

## Reading the pinned engine source (UE 5.8.4, CL 58902052, same changelist as the Mac package)

- The helper's pointer is set in one place only, when its data is uploaded, and nothing ever clears it. It lives as long as the builder.
- A new builder is created for every update, and the engine waits for the post-update task before uploading and discarding the old one. So "a builder was reused after upload" and "upload ran before the update finished" both fit the captured state poorly.
- Within the failing pass, the cell-header and item-chunk helpers had already passed the same check; only the block-data helper held a pointer.
- The scratch containers used by the builder come from a lock-free block allocator. Its reference counts use acquire-release ordering and its page cache uses sequentially consistent atomics, so reading it found **no ordering defect**. That does not exclude one.

## Ranking today (hypotheses, not findings)

1. A stray write or recycled memory at that one slot, by something outside the uploader's own life cycle. Best fit to the captured state.
2. A stale pointer from an earlier graph (weaker: the source says a builder cannot outlive its upload and reset).
3. Concurrent update and upload (weaker: the engine waits explicitly).
4. A theme- or primitive-specific update pattern. Never examined.

## What was ruled out

- The unrelated startup crash ([#35](35-llm-startup.md)) and the Linux virtual-texture shutdown crash ([#157](157-vt-shutdown.md)) share nothing with this one.
- Earlier rebuild, garbage-collection and task-jitter loops (4,000 passes) did not reproduce it.
- Mac-specific layouts from the debugger collector (ARM64 offsets) must not be reused on Linux binaries.

## Linux soak (Ultra, 2026-11-09)

One bounded soak on Linux x86-64 with the pinned engine, run as locked jobs of under 10 minutes each (28.8 minutes in total). A scratch, uncommitted hook in the capture director repeated *return to menu, then Resume* inside one real-rendering offscreen process (fixed 30 fps, `-RenderOffscreen`, `--cores 2`). Every Resume rebuilds the board and drives the scene-culling update.

| Run | Fixture | Presentation | Size | Cycles | Result |
| --- | --- | --- | --- | --- | --- |
| A | 16x11 board | themed | 1280x720 | 1,000 | clean exit |
| B | 18x12 board | themed | 1920x1080 | 1,000 | clean exit |
| C | 16x11 board | primitives | 1280x720 | 1,000 | clean exit |
| D | 16x11 board | themed | 1920x1080 | 2,000 | clean exit |
| E | 18x12 board | primitives | 1280x720 | 2,000 | clean exit |

7,000 cycles plus 63 in two short checks: **no assertion, no crash.** This is a non-recurrence on Linux only. It says nothing about the Mac: the cycles are a few frames apart (not human-timed), there was no concurrent player input, and the Mac ran ARM64, whose weaker memory ordering is exactly where a hidden ordering bug would show. Both themed and primitive presentation behaved the same, so the theme-specific hypothesis got no support either way, but the soak is too small to rule it out. Stop active work here.

## Next action

None active. If a Mac package is ever made again, the one useful experiment is the debugger run with generation-correlated tracking of the builder, its task handle and the upload helper's pointer. Everything else needs the same Mac loop that caught the single failure. Linux success cannot establish a Mac fix.

## Where the evidence is

Private and ignored on Ultra: `.local/stability32-ultra/linux-engine-source-audit-20261108.md` and `portable-source-review-20261102.md` in the stability checkout (they name engine source, so they are not published). The Mac captures, with the crash reports, memory dumps and logs, are in `stability32-essential-20261102.tar.gz` (SHA-256 `42c6136b5bbf1bb65bcb2d7033b2820488f5a25738a958b1c072d9165237f657`). The Mac diagnostic tooling and the original run ledger are on the pushed branch `feature/32-renderer-stability` at `6c2f9b7`.
