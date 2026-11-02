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
- The task wrapper disassembly and explicit frame2 registers in `lldb.log` identify executing task `x19=0x7500123e00`, equal to the scene's stored PostUpdateTaskHandle at the stop. That weakens a simple wrong-handle-at-stop explanation; it does not establish the earlier wait/upload order.
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

At the earlier coordination boundary, **HOST BUILD/TEST RELEASED** was reported. No stability native/build/test work resumed during gameplay's slot. The subsequent owner-migration instruction supersedes reacquisition: **Mac stability work is quiesced and will not resume**. This record is diagnostic progress, not a fix or independent review.

The strongest next targeted experiment is the corrected, bounded hardware watchpoint probe on the same red workload, tracking builder allocation generations and preserving writers through upload/reset/free. Correlate its watched builder with the actual asserting builder; otherwise the result cannot distinguish lifecycle reuse from outside corruption. The exact original workload remains runnable via the initial command above, with before/after snapshots needed to count real Resumes. Repeat the narrow baseline to establish reproduction rate before interpreting a matched process-only `r.SceneCulling.Async.Update=0` differential. That CVar/theme differential has **not** been run, and no production feature disable or broad flush is proposed.


## Owner-migration handoff

Latest instruction moves further implementation/engine work to the linked Linux desktop. No active operation remained at the boundary. No job was interrupted for migration and no new engine/native/build/package/test operation was started. The Mac has **no owned QA app, native controller, LLDB, UBT or automation process running**; no stopped debugger needs detachment. Original editor PID50002 and shared MCP18765 were untouched. Native/rendered work must be coordinated separately by root; this unit has released all of its workload.

Head before migration: `fce245635d4d50e262edac4d669b4fe3bee26be0`, diagnostic commit authored before the no-commit migration instruction. No push, PR, tracker write or external message was performed. The delegate left this migration update uncommitted for root. Root inspected the diagnostic source and handoff, then prepared the preservation commit/push on `feature/32-renderer-stability`; no merge and no independent review are claimed. Runtime source fingerprint remains `8e12d7bd31d555e06243ddb1dab2c0706728af46af841f5d94cd12e9c826757d`. There is no production source WIP or proposed runtime fix.

Preserved under the owned checkout:

- `.local/stability32/migration-handoff.json`: exact head/base/source, QA manifest, fixture hash, known-owned-PID absence checks and quiesced host state.
- `.local/stability32/baseline-menu/`: actual assertion arguments/registers, all-thread stacks, explicit worker disassembly, builder/scene memory, macOS IPS, native images, timestamped input records, receipt and copied `runtime.log`.
- `.local/stability32/{resume-idle-1,two-resumes-1,historical-repeat-1}/`: completed non-recurrence receipts, native images, assertion breakpoint hit counts0, normal cleanup and copied runtime logs.
- `.local/stability32/{historical-trace-1,historical-trace-2,historical-trace-3,watch-writer-1,watch-writer-2}/`: failed/inconclusive probe evidence and both distinct startup IPS files. No successful new first-writer capture is claimed.
- `.local/stability32/preserved-private-save-root/`: copy of the owned sandbox save root, including the byte-identical reward checkpoint and all native runtime logs. Original fixture remains `Saved/CaptureFixtures/reward/current-v1.json`; both hash to `d17acc4f12fcba902ed6c51ef3233f7a60c7b886fd1f06ad022201fc8a557470`.
- `.local/stability32/{prepare.py,native_loop.py,qa.json,stability32_capture.py,trace_lldb_v1.py,watch_lldb.py}`: runnable preparation/controller and exact prototype sources as preserved at handoff. The watchpoint prototype accepts the third callback argument but its corrected live attempt exited before attachment. The boundary tracer stalled and remains unsupported/inconclusive. The narrow durable collector armed and completed a native workload; its corrected explicit-worker capture path has not yet received a second real red assertion.
- `Builds/Stability32QA24c6c241/Stability32QA24c6c241.app` and `Builds/20261102T032206321426Z/`: isolated clone and original verified package/metadata. `tools/diagnostics/` contains the versioned collector and portable instructions.

Root's next step is to inspect/commit this handoff and transfer the ledger, tools and relevant ignored evidence to the new owner. Continue the ranked hypotheses with generation-correlated writer/order evidence. Linux native experiments can establish behavior on Linux and may help isolate an engine seam, but **desktop success is not a fix or non-recurrence proof for the Mac crash**. No Mac-specific predicate/layout may be assumed valid for a Linux executable. No supported project correction or shared-engine patch has been identified; #32 remains unresolved and #35 remains outside this unit.


### Root preservation receipt

Owner destination: PlebDev Ultra, linked environment `50ceeab6-638f-49e8-ba1b-9f29ea7dfdaa`, Linux x64, project `mcp:eb6b3d30-097f-4eba-a946-678aeb898be2`, checkout `/home/plebdev/dnd-diceroll-t3`. The coordinator owns desktop continuation and transfer; root has started no desktop work. No Linux Unreal installation or validation is claimed here.

Small essential evidence archive, prepared without running Unreal:

- Path in this Mac checkout: `.local/migration/stability32-essential-20261102.tar.gz`.
- Size: **512876 bytes**; SHA-256 `42c6136b5bbf1bb65bcb2d7033b2820488f5a25738a958b1c072d9165237f657`.
- Contains 93 manifest-listed files plus the manifest: live assertion/worker LLDB transcript and memory, crash reports, action journals and receipts, private checkpoint and capture fixtures, runtime logs, engine/process identity, prototype scripts, and selected baseline build/test/helper logs. Excludes apps, caches, screenshots and credentials. `MANIFEST.json` gives every included file's size and SHA-256. Baseline menu logs are labeled as baseline evidence; the diagnostic helper result remains the delegate's 53/53 receipt above.
- Companion checksum: `.local/migration/stability32-essential-20261102.tar.gz.sha256`.
- Complete ignored evidence remains at `.local/stability32/` and the larger optional archive `.local/migration/stability32-mac-evidence-20261102.tar.gz` (181882863 bytes, SHA-256 `c0d19be017ba08e5b36fbe09f35be124a063c338df773b036fef272add3329fe`). Transfer the small archive; no native build transfer is required. Ignored archives are preserved locally, not published to GitHub or transferred to Ultra by this thread.

The diagnostic child was instructed to preserve and stop, confirmed no active engine/native/debugger operation, and wrote its private-save/log preservation and migration summary before root requested `task_cancel`. Task ID: `node:delegated-task:command%3Amcp%3Af185c915-d46b-4449-9bd9-332262ae61f9%3Adelegate-task%3Astability32-diagnose-20261102`. No replacement delegate was started.

Issue #35 remains a distinct Mac startup failure: `FLLMTracker::PopTag` / `FLLMScope::DestructInTheOpen` during allocator/LaunchServices startup, before game log/frames. This unit preserved two new occurrences plus earlier reports; no Shipping or `-nollm` differential was performed. Investigate separately and do not reinterpret a Linux startup success as a Mac fix.

Next owner should fetch the feature branch and inspect the diagnostic commit and this ledger before integration. Review the ranked #32 hypotheses using generation-correlated lifecycle/writer evidence; do not port hard-coded ARM64 offsets to Linux. Both issues remain unresolved. Independent Standards/Spec review is outstanding; the branch is a diagnostic handoff, not a merge-ready correction. The gameplay owner's separate branch is `feature/gameplay-depth` at `661e216a142a7d7cf8546cbe4cecdc94298046dd`; do not merge it implicitly into this diagnostic branch. Its 84/85 test receipt and compiled-but-untested fixture correction belong to that owner's handoff, not this unit.
