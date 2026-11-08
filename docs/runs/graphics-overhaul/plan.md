# Graphics overhaul: council plan

2026-11-08 · baseline `origin/main` f63919d · lead: Claude Opus 5.5, slices: Claude Sonnet 5.5 · tracked in #138

## Council

Independent, read-only reviews of the same brief (code, docs, Blender recipes, #50 rendered captures, the approved Blender look test):

| Seat | Model | Result |
| --- | --- | --- |
| S | GPT-6.1 Sol | report |
| A | GPT-6 Astra | report |
| G | Gemini 4 Argon (Cursor) | report |
| O | Claude Opus 5.5 (lead, synthesis) | position written before reading the others |
| F | Claude Fable 5.1 | **absent**: failed twice on the Claude API rate limit, no output |

## Owner decisions (2026-11-08)

1. **Grid:** a lighter grid first, presentation only. Bigger arenas come later as a separate, coordinated slice (P12).
2. **Characters:** rigged and animated. Pilot one fighter, one caster and one foe, then all 13 classes. Static figures remain the fallback.

Kept unchanged: dark high fantasy with a low-poly PS1 influence, smooth and sharp, not grainy; all art original from Blender recipes; CC0 only; no kits or paid AI; full surrounding worlds; candlelit tabletop overview; `-ExpeditionPrimitives` fallback; ADR-0001 authoritative state; 5-ft square cells, cardinal moves and Manhattan ranges; save schema 4.

## Diagnosis (consensus, measured)

The core problem is spatial hierarchy rather than polygon count.

- **The floor dominates.** `BuildThemedScenery` spawns up to 216 separate `SM_Tile_*` slabs: 108 cm wide on 110 cm centres, 16 cm thick, with 2.5 cm chamfers (`build_props.py::tile`). Each repeats the same 128² patch. That gives a double border at every seam, and `UpdateTiles` then tints whole tiles. These edges are the brightest high-frequency detail on screen.
- **Figures read as tokens.** `CellSize = 110`. Figures are normalised to 1.15 m and scaled ×1.45 in `BuildRig`, about 167 cm, but stand on 74–80 cm opaque discs plus 90–100 cm target markers, covering 73–91% of a cell's width. Primitive fallbacks have inconsistent heights (86–125 cm).
- **The camera is a map view.** FOV 50°; `UpdateCamera` uses reach 0.62 and rise 1.0, a fixed ≈58° downward pitch, at 24–28 m. Character fronts, architecture and the horizon fall out of frame.
- **The world was hollowed out.** `build_worlds.py` flattens the footprint and deletes every component that touches the margin, rises within 6 m, or blocks any of 36 sampled camera rays. The architecture that conveys scale is exactly what was removed. Its single 1024² atlas gives about 15–25 texels/m on walls.
- **Nothing is grounded.** `MakeLight` sets `CastShadows(false)` on every light. The material adds emissive glow of 0.35 to 0.6 to everything, and exposure is lowered to compensate, so nothing casts a shadow.
- **Sparse, cloned dressing.** 10–12 blocked cells out of 176–216 each get the same single mesh, plus 7 braziers, 2 banners and 2 cages.
- **The HUD covers about 40% of combat.** Party cards are 375×168 px each at 1080p. The start screen's 1200×660 panel hides about 86% of the tabletop.

## Grid design

All seats agree. The authoritative grid stays 5-ft, cardinal and Manhattan. Every change below is in `Presentation/` and `Art/` only.

| Element | Spec |
| --- | --- |
| Metric | 1 cell = 150 cm (true 5 ft); figures 175–185 cm (brutes up to 195). A 16–18 × 11–12 board becomes 24–27 × 16.5–18 m. |
| Ground | One continuous ground per world, with irregular 50–75 cm flagstones, roots and soil that don't follow cell boundaries. Cosmetic relief within ±3 cm only; no elevation mechanics. |
| Grid visibility | Off at rest. In movement or targeting mode: the reach boundary as a smooth contour, faint local cell corners at 10–25% opacity, 1–2 px. Hold a key for the full grid, as an accessibility option. |
| Path / footprint | 3D path ribbon with exact cardinal corners and cost ticks; destination brackets; 56 cm hollow footprint rings instead of discs. |
| Implementation | A cell-state mask texture (reach, attack reach, obstructed, path, hover) feeds the floor material. The primitive fallback keeps the cube tiles. |
| Feels smaller because | Seams stop lining up with cells, the grid only shows in context, the camera is lower, and the world is fuller. **Rescaling alone makes cells look *larger* next to the figures** (S and A both flagged this), so it can't be the lever. |

Options the council rejected: 2.5-ft rules cells, a visible sub-grid, and diagonal moves (that one belongs to #95 as an opt-in rule). Bigger arenas are deferred to P12, as you chose.

## Target visual spec

The spike settles values marked ◆.

- **Camera:** FOV ◆40 (G proposed 38, S and A 45–50). Pitch tied to zoom: about 35° close, 45° default, 60° far; orbit is free; bounded pan with WASD or middle-drag; the board is framed to fit the HUD-safe area instead of the hard-coded 9% left pan.
- **Lighting:** baked vertex GI and AO from Blender stays, at denser vertex spacing (~75 cm). The directional light casts shadows with CSM (3 cascades, about 45 m). At most 2 shadowed local lights; other candles are unshadowed or emissive. No Lumen, virtual shadow maps or Nanite by default; Lumen gets at most one spike comparison.
- **Materials:** remove the blanket emissive. Glow comes from a vertex-alpha mask (flames, runes, glass, spells) only. Add a rim and hemisphere fill term so dark figures stand out on dark ground, and per-surface roughness. Filtering ◆A/B: trilinear + anisotropic filtering vs a sharp-texel anti-aliased filter (crisp texels without shimmer; G's proposal, closest to "PS1, smooth and sharp"). Texel density: 128–256 per m on environments, 256–512 on characters; 2048² world atlases.
- **Dressing per encounter:** 1 landmark, 3–5 secondary groups, 20–60 medium props, 100–300 instanced small props (ISM/HISM). Dressing never blocks or implies blockers. Each obstacle recipe gets 3 variants, and adjacent blocked pairs get 2-cell walls or arches.
- **Occlusion:** keep near architecture and fade it with a dithered camera-to-focus mask, instead of deleting it in `build_worlds.py`.
- **Atmosphere:** fog mostly beyond the playable area (start around 25 m, density 0.008–0.02); fixed exposure per biome; vignette 0.15–0.25; bloom 0.15–0.3; drifting mist cards and light-shaft meshes. No grain, motion blur or depth of field.
- **VFX:** 0.2–0.6 s directional impacts. Sparks become one instanced component. Effects never hide a target's footprint.
- **HUD (Canvas stays):** party rows of about 200×64 with portraits; merge the top bar and initiative into one strip about 38 px tall; the log fades out after 4 s; a compact action dock; secondary controls move into Menu. **Persistent HUD ≤ 18% of the screen.** The start screen uses side drawers so the tabletop stays visible.

## Characters

- **Skeleton:** one shared humanoid skeleton of about 20–30 bones (◆ count; G: 19 bones rigidly weighted for the PS1 feel). Three base bodies: heavy, agile, robed. 3–6k tris per equipped hero, at most 2–3 material slots, LODs at 50% and 25%. Weapons and focuses on sockets.
- **Coverage:** 13 class gear kits, not 70 meshes. 70 subclasses get 3-zone palettes (primary, secondary, trim/runes) keyed by `BuildId`, plus accents that keep duplicate party members distinguishable. Foes: raider, scout and archer move onto the skeleton, and the cast batches add 3 archetypes.
- **Animation:** 8 clips (idle, walk, melee, ranged, cast, hit, downed, death or recovery), hand-keyed or procedural in Blender, CC0 only. Motion is in place and driven by `FExpeditionPlayback`. Notifies never resolve outcomes. `Finish()`, skip, instant mode and Resume must converge at any phase.
- **Order:** the pilot is one fighter, one wizard and one raider in the vertical slice. After you approve it, the rest follow in batches of 3–4 classes.

## Architecture and pipeline

- Keep a single level with runtime assembly from data. No per-encounter `.umap`. Generation stays authoritative, and obstacle visuals derive only from `BlockedCells`.
- **One metric contract.** A single board-metric type serves `CellPosition`, `BoardPosition`, `CursorCell`, camera fit, markers and projectile heights. Cell size comes from the theme, and the primitive fallback keeps parity.
- **Decoration RNG.** Cosmetic placement uses its own stable hash stream and never consumes combat or run RNG.
- **Instancing.** `MakePart` and per-part dynamic material instances become ISM/HISM groups with per-instance custom data. Themed components drop from about 320 per rebuild to under 40, which cuts the churn suspected in #32.
- **Theme data.** `UDiceRollTheme` gains world kits (ground, shell, obstacle variant arrays, scatter, overlays) and character definitions (skeletal mesh, animation set, palette table), all as soft references. Missing entries fall back.
- **Blender revisions.** New `r002` sources and exports; `r001` is kept. The importer currently skips existing meshes, so an explicit reviewed reimport path is added. Every import has Nanite off with explicit LODs, which also addresses #105.
- **Skeletal glTF.** Round-trip import is proven in the spike before the pipeline is committed.
- **Budgets:** 1080p, warm package: p95 ≤ 16.7 ms, GPU ≤ 12 ms, game thread ≤ 4 ms. Packaged RSS ≤ 5 GB; resident theme textures ≤ 512 MB. Heavy Blender, import, build and package jobs run one at a time on the 16 GB host.

## Coordination

- **#50 / #112 (gameplay depth).** `feature/50-targeting-ultra` (PR #137, not yet in staging) rewrites `UpdateTiles`, `DrawWorldOverlays` and the forecast chips. The slices that overlap that code (#141 P2, #142 P3, #146 P7) and #151 start from staging after #50 merges, or from an agreed stacked base with owner approval. P3 and P7 keep #50's reach and target semantics and get joint acceptance.
- **#95 (SRD 5.2.1, Landon).** P0–P11 don't change rules. P12 changes Compact generation, so it must land before #95 PR 3 fixes its seeded Compact baseline, or be agreed with Landon.
- **#32 / #35 (renderer stability).** Their checkout is left alone. Instancing and the no-Lumen/VSM/Nanite default lower the risk, and rendered runs report any recurrence.

## Phases

Each P is one issue and PR to `staging` with independent Standards + Spec review (fresh seats from the council providers) and Ultra evidence. **Bold** marks owner gates.

| # | Slice (Sonnet unless noted) | Main areas | Acceptance evidence |
| --- | --- | --- | --- |
| P0 | Parent issue, child tickets and run record; **capture harness** | `tools/`, docs | Fixed poses: seed 20261030; 16×11 and 18×12 boards; 4 yaws × 3 zooms; 1920×1080 and 1280×720. Captures of today's look as the baseline. |
| P1 | **Look-dev spike** (throwaway branch): Vigil at 150 cm, continuous floor, contextual grid mock, shadowed sun, camera and filtering A/B, fighter skeletal round-trip | BattleView, Blender | **Gate 1: you choose camera, filtering and lighting from paired captures.** |
| P2 | Board-metric contract + `CellSize` 150 | BattleView, Playback | Picking round-trip for every cell at 4 yaws; primitive parity; Project automation green. |
| P3 | Continuous ground + contextual grid, path ribbon, footprint rings, hold-for-full-grid | BattleView, theme material | Reach, blocked corners and forecast targets match the rules; keyboard and pointer targeting; captures. |
| P4 | Camera rig + lighting/material/post retune | BattleView, importer material | Captures vs P1 choice; frame time recorded. |
| P5 | ISM/HISM instancing refactor | BattleView | Component-count test (< 40 themed); unchanged visuals in captures. |
| P6 | Vigil world kit r002 (Blender; can run beside P2–P5) | `Art/`, `build_worlds.py`, importer reimport | Asset records; clean import, Nanite off, LODs; no new warnings. |
| P7 | HUD compaction + start-screen drawers | `ExpeditionHUD.cpp`, controller hit areas | ≤ 18% coverage at both resolutions; focus and phase tests; #58 coordinated. |
| P8 | Character pilot: skeleton, fighter, wizard, raider, 8 clips | Blender, importer, rig, Playback | 1×, 2×, instant and skip playback; death; fresh-process Resume; contact sheet. |
| P9 | **Vertical slice**: Watchtower road at final quality (Opus integrates) | all | **Gate 2: full fight through Resume at all poses; perf gate.** |
| P10 | Root world r002, finale world, tabletop and dioramas refresh | Blender, theme | Captures of every encounter type and the overview. |
| P11 | Cast batches: remaining 10 classes, foe archetypes, 70 palettes | Blender, theme | Builder previews and combat captures with no primitive fallbacks; **you spot-check per batch**. |
| P12 | Bigger arenas (about 24×16), coordinated | `ExpeditionRun`, tests | New generation revision with schema-4 saves kept; seeded balance check; #112 and #95 sign-off. |
| P13 | Acceptance + release notes | all | Pinned build, `Project.Expedition.` zero warnings, helpers, Linux package, capture gallery, perf, `docs/assets.md` and current-state updated. |

**Parallelism.** Blender authoring (P6, P8, P10, P11) and C++ slices run at the same time in separate worktrees. Opus owns the editor, Unreal builds, imports, captures and packages, and runs them one at a time on Ultra.

## Disagreements and how they were resolved

| Topic | Positions | Resolution |
| --- | --- | --- |
| FOV and pitch | G: 38° FOV, 30–48° pitch tied to zoom. S/A: 45–50° FOV, 48–50° default pitch. O: 35–40° FOV. | FOV 40 and pitch tied to zoom; the spike decides. |
| Texture filtering | S/A: trilinear + anisotropic. G: sharp-texel anti-aliased filter. | A/B in the spike; the sharp-texel filter is favoured. |
| Lumen | S/A: try it in the spike. G: no. | Off by default; one comparison at most. |
| Arena size | S/A: defer. G: reject. O: optional. | Your choice: P12, coordinated. |
| Cast rollout | G: all 13 in one push. S/A/O: pilot first. | Your choice: pilot, then batches. |
| Bone count | G: 19, rigid weights. A: about 30. | Decided in P1/P8 from the import round-trip. |

## Further decisions (approved 2026-11-08)

1. A dedicated finale world, "Vault", instead of re-grading Vigil (in P10, #149).
2. P0 and P1 start at once. The slices that replace #50's tile, overlay and HUD code (#141, #142, #146), and #151, wait for #50 to land in staging.
3. Reviews use fresh independent seats for the Standards and Spec axes.

## Risks

- Delay or conflict with #50.
- Skeletal glTF import and animation round-trip.
- 16 GB RAM pressure during imports.
- Occlusion fading at low pitch.
- Balance drift from bigger arenas.
- Scope creep across 13 classes.

Mitigations: the spike gates, one metric contract, the fallback kept at every step, and batches with owner spot-checks.

Non-goals: elevation, diagonals, destructibility, new mechanics, photorealism, per-subclass bespoke meshes.
