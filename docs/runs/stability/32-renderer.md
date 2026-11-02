# Scene-culling diagnosis (#32)

Implementation unit, 2026-11-02. Branch `feature/32-renderer-stability`, fixed base `df25f32ae47dded0d1832cb9ef2eddaa251a52d9` (menu #110 integrated). Root owns review/integration and the separate startup issue #35. This unit has no authority for subagents, tracker writes, PRs, pushes or merges.

## Identity and boundaries

Owned checkout: `/Users/plebdev/.t3/worktrees/dnd-diceroll/t3-cf27a55f`. Original checkout/editor PID50002 and MCP18765 belong elsewhere and were untouched. Doctor passes Python3.15 via `/opt/homebrew/bin/python3`, Apple Silicon macOS26.5.2, UE5.8.4 CL58902052. Preflight/status correctly refuse the occupied, different-checkout MCP endpoint; associated owned editor processes are empty. No editor was launched or closed.

Unmodified verified package: `Builds/20261102T032206321426Z/Mac/DnDDiceRoll.app`; native executable UUID `B44B6629-0127-3BA8-BB1A-06C2A988B1AE`. Runtime Source fingerprint (sorted repository-relative Source file paths (including `Source/`), NUL, bytes, NUL) `8e12d7bd31d555e06243ddb1dab2c0706728af46af841f5d94cd12e9c826757d`. The fresh ad-hoc QA clone is recorded in `.local/stability32/qa.json`; its `__TEXT,__text` matches the verified package. Only its bundle/display identity and signature differ. All fixture writes are inside that clone's unique sandbox container; default player saves and the original fixture are untouched.

Reward fixture: `Saved/CaptureFixtures/reward/current-v1.json`, SHA-256 `d17acc4f12fcba902ed6c51ef3233f7a60c7b886fd1f06ad022201fc8a557470`. All action events include time, exact target PID/window and CUA response. Native keyboard actions use cua-driver with snapshots before/after. No foreground shim, forced logging, renderer feature disable, asset mutation or shared engine change was used. The clone executable SHA-256 is `0f95000234ff9be399a7505466efedac443a8c59ab115e0c118fa1051ca9d0e3`; code-section SHA-256 is `4f1ce12c65afa3b7ea0d6facfc609202123336f439215431e3e0357088686084`.

## Feedback loop and experiments

Applied `/Users/plebdev/.codex/skills/diagnosing-bugs/SKILL.md`: build the exact-symptom loop before selecting causes, capture/minimize, rank falsifiable hypotheses before their probes, vary one input, and retain unsupported outcomes as unknown. Historical 4,000 rebuild/GC/task-jitter passes did not reproduce; those are non-recurrence evidence only. The current issue and all comments were read.

Preparation (ignored script):

```sh
/opt/homebrew/bin/python3 .local/stability32/prepare.py
```

| Experiment | Exact invocation after `/opt/homebrew/bin/python3 .local/stability32/native_loop.py` | Outcome |
| --- | --- | --- |
| Initial capture loop | `--name baseline-menu --cycles 5 --debugger --flow menu` | **RED** on first process, PID81885. Assertion entry captured ~148.6s after arming. Five cycles requested; two mechanically completed, but screenshots show Tab/Enter opened Help on cycle0. Cycle1 O was blocked by Help; its Enter closed Help, and cycle2 O resumed again. The snapshots and the second FlushAsyncLoading timestamp establish **two confirmed Resumes**, not three or five rebuilds. Last sent key was Left in attempted cycle2; assertion occurred before the next Right. Checkpoint byte-identical. |
| Minimize to one Resume and idle | `--name resume-idle-1 --cycles 1 --debugger --flow idle --observe-seconds 180` | **NO_RECURRENCE**, PID91826; one rendered Resume and180s observation, two resolved assertion locations with0 hits. Byte-identical checkpoint, debugger detached and normal Cmd-Q. |
| Two Resumes separated by idle | `--name two-resumes-1 --cycles 2 --cycle-delay 90 --debugger --flow resume --observe-seconds 90` | **NO_RECURRENCE**, PID97930; two Resumes90s apart plus90s observation;220.38s total. |
| Narrow collector repeat | `--name historical-repeat-1 --cycles 5 --debugger --flow menu` | **NO_RECURRENCE**, PID20960;5 mechanical cycles in257.22s. Same Help/focus caveat as the red run; these are not5 verified rebuilds. |
| Boundary trace attempts | `--name historical-trace-N --cycles 5 --debugger --trace --flow menu` for N=1,2,3 | All **INCONCLUSIVE** for different reasons below. |
| First-writer watchpoint attempts | `--name watch-writer-N --cycles 5 --debugger --watch --flow menu` for N=1,2 | Both **INCONCLUSIVE**, callback incompatibility then pre-attach startup exit; details below. |

The initial command is red-capable and actually went red on the exact issue signature. Its rate is not yet established and it is not a seconds-long deterministic repro. Native action snapshot overhead changes timing. No minimization or causal hypothesis is yet called proven.

## First live failure

`.local/stability32/baseline-menu/assertion.json` captures the assertion **at function entry**, before Unreal fatal logging destroys the argument registers:

- Expression: `UploadDataProxy == nullptr`.
- Installed source: `Runtime/Renderer/Private/RendererPrivateUtils.h:317` (`Reserve`).
- Caller: `FSceneCullingBuilder::ProcessPostSceneUpdate +20176`; worker `Background Worker (Standby #0)`, LLDB thread34.
- Caller disassembly retains builder in x20; the guard reads builder+`0x108`, the BlockData uploader proxy. `NumDirtyBlocks` at +`0x98` is0.
- Builder `0x75000bc810`, scene-culling object `0x770004c800`; the latter's active-builder field (+`0x2f8`, verified by this binary's disassembly) equals the asserting builder. The active PostUpdateTaskHandle (+`0x570`) is `0x7500123e00`.
- BlockData proxy is `0x75000fa600`. At stop it contains a vtable symbol for `UE::Tasks::Wait<...>`'s executable task, instead of upload arrays. This is an observation of invalid lifetime/ordering state, **not yet the identity of the corrupting writer or a root cause**.
- `builder.bin`, `scene-culling.bin`, full `lldb.log`, `assertion-context.txt`, timestamped `events.jsonl`, and before/after native images are preserved in that run directory. The initial generic CLI context dump used LLDB's selected main thread for some register/frame commands; `assertion.json` uses the callback's actual SBFrame and is correct. Subsequent explicit thread34 selection captured the asserting caller's full disassembly/registers/memory. The collector was corrected to use explicit `SBExecutionContext` for later runs.

The latest historical ResizeAndUploadTo report has the **same executable UUID**; the new Reserve failure directly matches the two historical ProcessPostSceneUpdate+20176 stacks. The other upload stack remains a separate signature until its exact predicate/state is captured. No claim that all recurrences share a cause.

The stopped process was detached after read-only capture, allowing its original assertion to proceed. LLDB exited; CUA reported no remaining windows. No termination signal or force quit was used. A fresh process is used for minimization.

## Ranked hypotheses before causal probes

After the first red native loop and the first minimization pass, the following provisional ranking was shown before targeted probes. A high-rate minimal repro is not established, so this is a capture plan rather than a diagnosis/fix claim.

1. **Builder reuse after upload:** per-generation trace should show UploadToGPU followed by another ProcessPostSceneUpdate for the same builder generation.
2. **Concurrent update/upload:** trace should show UploadToGPU beginning before the same builder's post-update ends; `r.SceneCulling.Async.Update=0` should suppress the symptom under the same workload.
3. **Memory corruption outside uploader lifecycle:** a hardware watchpoint on the proxy should first catch an unexpected writer outside the valid upload/construction lifecycle.
4. **Theme-specific update pattern:** a repeated `-ExpeditionPrimitives` differential should separate from a matched themed baseline; a single clean trial cannot establish this.

No hypothesis has passed its falsification test yet. Static installed-source reads locate the lifetime/scheduling boundaries; they do not establish which writer/order caused the observed state. Two Resumes90s apart plus90s observation also produced NO_RECURRENCE (PID97930, checkpoint byte-identical, normal exit). Neither minimization establishes load-bearing actions.

## Capture-tool development and probe limits

The durable LLDB collector and portable invocation instructions are in [tools/diagnostics](../../../tools/diagnostics/README.md). It verifies exact executable path and SHA-256 before arming, refuses editor binaries, captures the exact assertion arguments plus actual-worker contexts/stacks, and keeps the target stopped. Layout-specific renderer reads are gated on the exact UUID and verified caller offset. Its default has only the two assertion locations: it does not execute target expressions, add Unreal logs, launch, resume or quit a target. The native controller owns those separately. Evidence is never a fix verdict.

A narrow Python safety/classification suite covers wrong-process refusal, changed binary refusal, and distinguishing the exact uploader/scene assertion from unrelated renderer/assertion/startup failures. An initial test compared a temporary `/var` path with its canonical `/private/var` result; the expectation was corrected to use the canonical path. This tooling test failure is unrelated to the game.

Additional failed attempts:

- `historical-trace-1`: PID4645 exited before debugger attach/window. Preserved IPS identifies the distinct #35 `FLLMTracker::PopTag` startup path; no #32 evidence and no #35 diagnosis/fix claim.
- `historical-trace-2`: PID9069, arming refused by an incompatibility with LLDB's embedded Python (`hashlib.file_digest` unavailable). No gameplay input. Chunked hashing replaced that call. Debugger detached and a separately recorded CUA normal quit cleaned up the owned process.
- `historical-trace-3`: corrected arming succeeded (PID14488), with eight boundary/return breakpoint sites.126 events =21 begin/post/upload sequences were recorded, with zero assertion hits. Then renderer events stopped progressing and screenshots remained on startup despite O. This is **INCONCLUSIVE**, not a clean workload. The owned Python controller (PID9984) received SIGINT to run its cleanup; the app was not signaled. LLDB interrupted/detached and CUA normal Cmd-Q completed. The exact reason for the instrumented stall is unknown. The intrusive optional tracer is retained only as ignored `.local/stability32/trace_lldb_v1.py`, not shipped as a supported diagnostic. Its records do not prove or exclude any cause.

- `watch-writer-1`: PID32358, two-address hardware write-watchpoint probe. The first address armed, then the first write stopped because LLDB passed three callback arguments although its help documented two. No complete workload/cause verdict. Read-only cleanup stacks preserve `FMallocPoisonProxy::Free` through `FSceneCulling::FinalizeUpdateAndClear +380` on RenderThread0, poisoning the watched builder allocation. That is consistent with normal reset/free; tracking a raw address without builder generation is insufficient to identify corruption. The callback now accepts the third argument in ignored `watch_lldb.py`; it has not yet completed a live trial. The Python controller alone was interrupted for cleanup; LLDB detached and normal CUA Cmd-Q exited the app.
- `watch-writer-2`: corrected probe launch PID38265 exited before attachment in6.61s. Preserved IPS again identifies #35's `FLLMTracker::PopTag` startup path, not #32. No debugger armed or gameplay input occurred.

Each attempt has `.local/stability32/<name>/receipt.json`, timestamped CUA event/response records, and available native images/debugger logs. Own startup reports are copied into the corresponding run directories. The actual red report is `.local/stability32/baseline-menu/DnDDiceRoll-2026-11-02-082709.ips`. The diagnostic controller/preparation/prototype probes remain ignored and available to root in this checkout. Do not silently reuse the clone/container in another concurrent session.

## Verification and handoff state

- `/opt/homebrew/bin/python3 -m unittest discover -s tools/tests -v`: **53/53 passed** in6.873s (50 existing helpers plus3 capture safety/classification tests). These are host helper tests, not Unreal automation. The suite's printed single-test automation is a mocked helper fixture.
- `/opt/homebrew/bin/python3 tools/contribute.py check`: **PASS** against the staged diagnostic unit. `git diff --check`: clean.
- Doctor passed earlier in this unit. Preflight/status refused the other editor endpoint as intended. No actual MCP, PIE or editor interaction was attempted.
- No runtime source, asset, project configuration or shared installed-engine bytes changed. No fresh UBT, package or full Unreal `Project.` automation was run in this unit. The supplied baseline84/84, zero-warning result belongs to the unchanged-source package provenance, not fresh diagnostic verification.
- Native totals:9 launch attempts; **1 exact #32 red**,3 completed non-recurrence trials,5 inconclusive attempts (2 distinct #35 startup exits,1 arming incompatibility,1 intrusive trace stall,1 watchpoint callback incompatibility). These unequal/timing-instrumented trials do not estimate a defensible crash probability.
- Every attempt retained the reward checkpoint hash above. All native/debugger processes were cleaned up normally or exited through their original failure. Explicit absence checks cover owned app PIDs81885,91826,97930,4645,9069,14488,20960,32358,38265. No owned native controller or LLDB remains. Original editor PID50002 was untouched.

At the coordination boundary, **HOST BUILD/TEST RELEASED** was reported. New UBT, package, automation and rendered/timing loops are held pending root reacquisition; native QA has not been granted to the gameplay session. Source and read-only evidence analysis may continue. This record is diagnostic progress, not a fix or independent review.

The strongest next targeted experiment is the corrected, bounded hardware watchpoint probe on the same red workload, tracking builder allocation generations and preserving writers through upload/reset/free. Correlate its watched builder with the actual asserting builder; otherwise the result cannot distinguish lifecycle reuse from outside corruption. The exact original workload remains runnable via the initial command above, with before/after snapshots needed to count real Resumes. Repeat the narrow baseline to establish reproduction rate before interpreting a matched process-only `r.SceneCulling.Async.Update=0` differential. That CVar/theme differential has **not** been run, and no production feature disable or broad flush is proposed.
