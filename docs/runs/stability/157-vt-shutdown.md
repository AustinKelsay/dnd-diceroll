# Virtual-texture crash at shutdown of a failed automation run (#157)

[Issue #157](https://github.com/AustinKelsay/dnd-diceroll/issues/157). Status as of 2026-11-09: **trigger confirmed, cause identified from the engine source and the cores. A known, reported, low-impact failure. Open, no active work.**

## What fails

When an automation run on Ultra has at least one failing test, the editor can crash with SIGSEGV while it shuts down, after the test report is written. The assert is an array index of -1 into an array of size 1, inside the virtual-texture system's forced clean-up when the renderer module unloads.

## Impact

None for players and none for passing runs. A run only reaches this after a test has already failed, and `tools/dev.py` ([PR #161](https://github.com/AustinKelsay/dnd-diceroll/pull/161)) now reports a shutdown crash as a failed lifecycle even when tests also fail. Merge gates are unaffected: a gate needs every test to pass.

## Trigger (confirmed, 2026-11-09)

Deliberately failing tests in an uncommitted scratch checkout of `staging` (`9b1bffd`), each run as its own job with `--cores 2`:

| Variant | Runs | Crashed |
| --- | --- | --- |
| Failing test with an input-harness world | 7 | **7** |
| Failing test with no world | 6 | **6** |
| Matched passing test with no world | 6 | 0 |
| Matched passing test with a harness world | 3 | 0 |
| Real long passing test (the compact HUD menu) | 6 | 0 |

All 13 failing runs crashed with the same stack; none of the 15 passing runs did. One of the passing runs returned a non-zero exit code because the engine logged an unrelated class-initialisation error at start-up; it did not crash. **A failing test is the trigger.** No world, texture or project code is needed, and run length does not matter.

## Cause (identified; the chain is explained, not demonstrated end to end)

- **What is freed.** Debugger reads of three preserved cores (pids 399379, 845567, 937805) and one fresh core show the virtual texture's built data, its tile-offset arrays and its address arrays all as freed memory: the first bytes of each block hold an allocator link, and the rest is stale. The address array's first entry therefore reads as a nonzero junk value, which makes the lookup index -1.
- **Which texture.** The engine's own `BaseFlattenNormalMap_VT` (allocated as `FlattenMaterial_VT`). It is engine content, not a project asset. This also explains why every occurrence looks the same.
- **Why a failed test matters.** On any test failure the engine's automation command line logs "Setting GIsCriticalError due to test failures (will cause non-zero exit code)" and shuts down with that flag set. In the pinned engine source, releasing a render resource does nothing while that flag is set. The texture's objects are still destroyed at exit, so the virtual-texture system is left with a registered producer whose built data is gone, and the renderer module's final clean-up reads it.
- **Not shown.** No run turned the flag off to see the crash disappear, because that would be a workaround, not a diagnosis.

## What was ruled out

- Not related to [#32](32-renderer.md) or [#35](35-llm-startup.md).
- Not a page-table or thread race: only the game thread is in virtual-texture code in every crash context.
- Not the harness: the first suspicion was worlds torn down early. The harness now flushes and collects garbage on teardown, and the crash occurs with no world at all.

## Next action

None. It stays open as a documented, reported failure. Do not add a flush-plus-garbage-collection workaround for it. If a future engine upgrade changes the behaviour, re-run the failing-plain-test check above.

## Where the evidence is

Private and ignored on Ultra: `.local/stability32-ultra/trigger-157-20261109/` (run logs, 13 crash folders, the scratch test source and runner scripts) and `.local/stability32-ultra/vt157-core-analysis-20261109.md` in the stability checkout (the latter names engine source, so it is not published). The earlier occurrences are in `.local/desktop-setup/linux-vt-shutdown-*`. The original cores stay compressed under `/var/lib/systemd/coredump`.
