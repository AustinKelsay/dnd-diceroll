# Root acceptance and delivery

All four implementation slices have per-ticket Standards/Spec reviews and real Unreal evidence. The additional [full-feature integrity review](05-integrity-review.md) found no actionable issues at functional head `83cfb63`. The [local CodeRabbit gate](06-coderabbit-local.md) completed with two valid startup findings; root fixed both directly and verified public red/green regressions.

## Final checks after CodeRabbit fixes

- Fresh editor build: `.local/final-startup-green-build.log`, succeeded.
- Full explicit `Project.` automation: **36/36 passed, zero warnings**, `.local/automation/20261031T050502397408Z`; `.local/final-startup-green-tests.log`.
- Offline helpers: **22/22 passed**, `.local/final-startup-helper-tests.log`.
- Final Mac Development build/cook/stage/archive and exact-map headless startup: `Builds/20261031T050537650861Z/verified.json`, zero errors/warnings. Full log `.local/final-reviewed-package.log`.
- Preserved sandbox bootstrap, exact MCP identity, rendered combat/role/rescue/death/survivor/run flow and fresh-process combat resume are recorded in the four sessions. Functional startup fixes did not change assets or combat rules.

## Final packaged startup verification

Root used the cua-driver native GUI skill, exact packaged executable and owned container fixtures. LaunchServices initially resolved an already-running headless package-verification process, which had no windows; no input was sent to it. After package verification completed, the final archived executable was launched directly with its own save-root override.

PID **54911**, window **44618**: native N at startup displayed “A saved expedition is unfinished. Press O to resume it.” The entire preexisting checkpoint remained equal. Native O restored the retained run `4474DB00…`, seed 20261031, roster and route offers. Inspected images: `.local/captures/final-startup-protected.png` and `final-startup-resumed.png`. Normal Cmd-Q closed the app, and window/process disappearance was verified.

PID **55944**, window **44626**: native N with an owned truncated save started a fresh run and displayed that the unreadable original was preserved. The unique sibling archive retained the exact original bytes. Inspected `.local/captures/final-recovery-preserved.png`. Normal Cmd-Q closed the app. No direct gameplay API produced either UI result. Both checks used working background key delivery; no foreground escalation was needed.

Proof and guarded cleanup: `.local/final-startup-native-proof.json`. Only the unique `Saved/Automation/RootFinalStartup/<uuid>` fixture was removed after both owned processes exited and identity/content checks passed. No default/user save was modified. CUA session ended. Final helper status at **2026-10-31 05:09:08 UTC**: no associated project processes or MCP listener. Other project/browser windows untouched. Unrelated engine-generated Android file-server settings were removed from config.

## Limits

Mac Development and the native keyboard path are verified. CUA mouse coordinate delivery remains unreliable; this is not a claim of verified packaged mouse targeting. No Shipping/notarization, other platforms, performance target, cloud/concurrent save writers, migration or fsync/power-loss guarantee is established. Local snapshots are validated coherent state, not adversarial replay proof. The supported SRD subset and deliberate tactical adaptations are documented.

## PR gate

Local CodeRabbit findings resolved; affected full checks pass. PR creation and the bounded PR CodeRabbit round are root-owned and recorded in the ledger. No merge into staging/main or production distribution is part of this delivery.
