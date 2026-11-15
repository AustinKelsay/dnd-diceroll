# #105 — Windows DX12/SM6 Nanite usage

2026-11-10, Amaterasu (Windows 11, RTX 5080, driver 616.92), UE 5.8.4 CL 58902052. Windows evidence is advisory; PlebDev Ultra remains the merge gate. SM5/null-RHI success cannot establish a Nanite fix.

## Scope and root cause

Based on staging `871185a6`, after graphics PRs #175/#176 and #180–#183. Unreal API readback found all 12 historically warned Windows material instances inherit `/Game/DnDDiceRoll/Theme/Materials/M_DiceRollTheme`; the master's Nanite usage was false, with no instance Nanite overrides. The fix enables that usage on the parent, recompiles and resaves **one asset**. It does not rebuild the material graph or change instances, meshes, parameters, graph revision, gameplay, Compact, saves, pins or default renderer settings.

The import recipe now checks usage even when it keeps an already-current graph. Five pure Python tests cover unconditional placement before save, already-enabled idempotence, enabling/recompile, compile errors and failed readback. The bulk importer was not executed during this repair: only its targeted helper was used through Unreal's asset API.

Only asset changed: `Content/DnDDiceRoll/Theme/Materials/M_DiceRollTheme.uasset`.
SHA-256/LFS object: `7bf3e7ac44ee26912cb04d052e61daaa2c739c58ded54eb16bdaf1e457144520` → `c9d449efbf5c7acb1595bbec9c9e33bb9f13d19298c66d1d9c27b03708091fee`.
A theme-wide before/after hash inventory confirmed no other asset changed. Fresh-process API snapshots confirmed all 12 effective flags false → true, with inherited parents, override bits, graph revision and scalar/vector/texture parameters unchanged; repeated application in the authoring process did not dirty the master again.

## Rendered reproduction and acceptance

Two deterministic fixtures: `default-seed-battle` (Root) and `board-vigil-pillars` (Vigil), each with battle and start-screen shots at 1280×720. Fresh before/after editor-game processes used `-dx12 -sm6`, not null-RHI. Both logged D3D12 and `shaderplatform="PCD3D_SM6"`.

| Run | Root missing-flag lines | Vigil missing-flag lines | Unique instances |
| --- | ---: | ---: | ---: |
| Historical Windows `7f299812` + value-preserving MSVC split | 11 | 10 | 12 |
| Current staging `871185a6`, before fix | 10 | 10 | 11 |
| Same current tree, repaired master | 0 | 0 | 0 |

Each missing-flag line had a matching explanatory `LogMaterial: Warning` line; both kinds disappear. #176 deliberately disabled Nanite on the Root world kit, so `MI_World_Root` is no longer exercised as a warning by the current fixtures. The historical Mac report's 15 materials, historical Windows 12 and current Windows 11 are observed subsets, not a complete material inventory.

Historical Windows instances (all under `/Game/DnDDiceRoll/Theme/Materials/`): `MI_Diorama_Root`, `MI_Diorama_Vigil`, `MI_Foe_Archer`, `MI_Foe_Scout`, `MI_Hero_Cleric`, `MI_Hero_Rogue`, `MI_Prop_Banner`, `MI_Prop_Brazier`, `MI_Prop_Cage`, `MI_Prop_Flame`, `MI_Tabletop`, `MI_World_Root`.

Before:
```text
LogMaterial: Warning: Material /Game/DnDDiceRoll/Theme/Materials/MI_Tabletop.MI_Tabletop missing usage flag Nanite! Default Material will be used in game.
LogMaterial: Warning:      The material instance will recompile every editor launch until resaved.
```
After: zero occurrences of either line type in both rendered SM6 fixture logs.

The editor-game startup has existing engine/editor-plugin diagnostics (70 error lines and 21 non-material warning lines per fixture). Timestamp-normalized diagnostic multisets are identical before and after, excluding the removed material warnings. These editor runs are not claimed as globally clean acceptance.

Inspected before-SM5 / before-SM6 / after-SM6 comparisons show unchanged design, materials and framing. Battle before/after-SM6 image MAE is 0.057/0.066 on the 0–255 RGB scale (Root/Vigil), with about 0.11–0.16% of pixels changing by more than 10. Ambient animation makes the title shots non-pixel-identical (MAE about 1.86). No claim of byte-identical rendered images.

## Tests, packaging and limitations

- Development Editor build: exit 0, zero compiler warnings.
- Full `Project.` on committed asset/policy head `887f0c267a46088a6430abae8f027da311cbaf3a`, rerun 2026-11-10 2:07 AM CST: **275/275**, exit 0, zero warnings/errors. `Project.Expedition.Run.CompactSeedIdenticalFingerprint` explicitly succeeded; no Compact/golden changes.
- New Python policy tests: **5/5**, zero warnings.
- Fresh Win64 Development DX12/SM6 package, completed **2026-11-10 2:06 AM CST**: UAT exit 0, zero package warnings/errors; normal startup-map exit 0, zero startup warnings/errors; actual `PCD3D_SM6`. Two fresh packaged Root/Vigil battle/title captures: normal exit 0, zero warnings/errors and zero Nanite warnings in each.

The first package attempt is preserved, not counted as clean: UAT succeeded but reported one `LogZenServiceInstance` sponsor/unresponsive warning, and the helper's 30-second normal-close budget expired. That owned game subsequently logged a normal exit without intervention (about 59 seconds after the close request). After the host recovered, the **unchanged strict helper** was rerun and passed all package/startup/capture assertions above. No timeout, warning filter or production helper was changed to manufacture that pass.

Base staging `871185a6` has an MSVC combined-exported-declaration compile failure. Windows native verification used the value-preserving private declaration/test-fixture overlay already owned by helper PR #185 and imported that PR's Windows harness. Neither overlay nor helper changes are included here; the Nanite branch has no Git dependency on #185. Base `contribute.py check` misnormalizes local links on Windows; the same full index check passes with #185's Windows-corrected checker. This inherited host-tooling limit is not repaired again in the material branch. This is not a claim that unmodified staging builds on Windows before #185 lands. No engine version pin or platform support promotion is part of this change.

A private, temporary `Config/Windows/WindowsEngine.ini` selected DX12/SM6 for the package and was removed after the run. `Config/DefaultEngine.ini` was neither changed nor staged. No service was reset, no cache deleted, no warnings suppressed and no foreign process stopped.

## Reproduce

1. Use the pinned engine; build the Development Editor and run `Project.Tools.CaptureFixtures`.
2. Copy each named fixture's checkpoint to a different private save root. Do not use a real player's save.
3. Launch `UnrealEditor.exe <project> -game -dx12 -sm6 -windowed -ForceRes -ResX=1280 -ResY=720 -unattended -nosound -NoLiveCoding -ExpeditionSaveRoot=<root> -ExpeditionCapture=<poses.json> -ExpeditionCaptureOut=<out> -ExpeditionCaptureFixture=<fixture> -abslog=<log>`.
4. Poses: `{"version":1,"settleFrames":60,"shots":[{"name":"battle","mode":"battle","yaw":0,"distance":"default"},{"name":"start-screen","mode":"start"}]}`. Verify runtime D3D12/SM6 metadata, inspect captures and count both material-warning forms.
5. For packaged proof, cook with a private SM6 Windows platform override, then run the packaged binary with the same fixture/capture flags in separate fresh processes. A default SM5 package is not proof.

Raw local logs, material snapshots, captures, hashes and the independent Standards/Spec review are retained in `.local/win105/resume/`; owner evidence is copied to `/workspace/dnd-side/` on the box. They are evidence artifacts, not files required by the runtime.
