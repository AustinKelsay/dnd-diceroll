# Native scene-culling assertion capture

`scene_culling_lldb.py` captures an assertion in an explicitly owned Mac Development
packaged game. Use the pinned engine and the editor/GUI ownership rules in
[development](../../docs/development.md). Do not attach to another session's editor
or game. Native launch, fixture isolation, input, screenshots and normal shutdown
belong to the caller; this module performs none of those actions.

Attach LLDB to the owned game PID, then import the module using the absolute path
of this checkout:

```text
xcrun lldb -p <owned-game-pid>
(lldb) command script import /absolute/checkout/tools/diagnostics/scene_culling_lldb.py
(lldb) scene-culling-arm --output /absolute/fresh/capture-directory --expected-executable /absolute/OwnedQA.app/Contents/MacOS/DnDDiceRoll --sha256 <expected-executable-sha256>
(lldb) continue
```

Obtain the expected executable path and SHA-256 from the prepared, isolated QA
clone before attachment. Arming requires the process to be stopped, validates its
actual executable path and on-disk hash, refuses editor binaries, and requires
resolved assertion breakpoints. Check `armed.json` for the target PID, UUID and
nonzero `assertion_locations` before beginning the workload. Use a fresh LLDB
session and output directory for each run. The module imports without LLDB for its
host-side safety tests; LLDB is required for arming/capture.

The assertion breakpoint stops on `FDebug::CheckVerifyFailedImpl2` entry. It reads
ARM64 argument registers before fatal logging alters them and leaves the process
stopped. `assertion.json` records the predicate, source location, actual triggering
thread and stack, and an exact scene-culling signature classification. An unrelated
assertion also stops and is retained with `scene_culling_signature: false`.
`assertion-context.txt` uses explicit worker-frame contexts for registers,
disassembly, available variables, and all thread stacks. `capture-complete.json`
marks completed collection; inspect `capture-error.txt` if collection fails.

Additional renderer fields and raw memory are captured only for executable UUID
`B44B6629-0127-3BA8-BB1A-06C2A988B1AE`, `Reserve` line317 and its verified
`ProcessPostSceneUpdate +20176` caller. They use disassembly-verified offsets, not
an assumed layout for other builds. Other signatures still receive generic
assertion/context evidence. The collector reads memory and debugger metadata; it
does not evaluate target expressions or add Unreal logging.

After inspecting a stopped failure, explicitly choose whether to detach/resume
and allow its original crash to proceed, or retain it for further debugging.
For a clean trial, interrupt the owned target in LLDB, record `breakpoint list`
(including hit counts), detach, and quit the app normally through the native GUI.
A capture error, early startup exit, stalled workload or unverified input sequence
is inconclusive. A clean trial is non-recurrence evidence, never a fix verdict.

The [#32 run ledger](../../docs/runs/stability/32-renderer.md) records the actual red
native workload, its private fixture identity, failed probes and remaining tests.
