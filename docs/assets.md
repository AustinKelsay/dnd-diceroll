# Asset provenance

Before importing or replacing assets, follow [asset intake](agents/asset-intake.md) for candidate validation and handoff. For generation, editable source preparation or revisions, follow [asset production](agents/asset-production.md) and keep a [versioned asset record](agents/asset-manifests.md) with the retained source. Record each asset's provenance here.

The sandbox uses Unreal Engine's bundled basic cube. The dark-fantasy theme and the game splash below are the only added art; both are original, agent-authored work, and nothing was downloaded.

For each imported asset or generated source, record its destination, author/provider, source URL or generation recipe, license, and modifications here. Keep editable source assets in `Art/` when introduced and import the runtime form into `Content/`.

## Dark-fantasy theme (issue #38)

The game's art set lives under `/Game/DnDDiceRoll/Theme`.

**Authorship and license:** all of it is original work by this project's agents, made with a local Blender pipeline from the owner-approved look tests (2026-11-01). No third-party art, files, names or downloads are included; in particular, nothing from the inspiration game or its reference videos. It is project-owned, with the same terms as the repository.

**Sources:** the editable sources are `Art/dark-fantasy-sources/r001/`:
- the packed Blender scenes `vigil.blend`, `root.blend` and `mission.blend`;
- `scripts/`, the Blender recipes that export each asset (`build_*.py`) plus the rogue rescale and root-tile repaint.

Each runtime mesh has a versioned record at `Art/<asset-id>/r001/asset.json`, with its glTF export beside it.

**Import:** `Content/Python/import_dark_fantasy_theme.py` imports the exports. It:
- creates `M_DiceRollTheme`, and rebuilds its graph in place on every run (the asset and its instances survive); see "Theme material" below;
- gives each mesh its own `MI_*` instance;
- sets textures to `TF_DEFAULT` in `TEXTUREGROUP_World`, uncompressed and sRGB. Gate 1 chose trilinear + anisotropic 8 (nearest, trilinear and sharp texels looked nearly identical at 256 texels/m). `TF_TRILINEAR` maps straight to a plain trilinear sampler, and Vulkan enables anisotropy only for anisotropic sampler filters, so `r.MaxAnisotropy=8` (`Config/DefaultEngine.ini`) would do nothing for it. `TF_DEFAULT` defers to the texture group; the engine's World group is aniso with point mips (an anisotropic-point sampler), so `Config/DefaultDeviceProfiles.ini` replaces that one group entry with aniso + linear mips, the anisotropic-linear sampler. This is a Config change (a new file) and needs coordinator approval. The filter fix reimport is expected to change only the 21 `T_*` textures (their `filter` property);
- fills `DA_DiceRollTheme`, a `UDiceRollTheme` data asset.

**Theme material (#143):** `M_DiceRollTheme` computes `base = Tex × vertex colour × Color` and adds three emissive terms, each driven by a per-instance parameter:
- `Glow` × the vertex-colour alpha mask (flames, runes, glass). There is no blanket glow share any more.
- `Fill` × a hemisphere fill (sky above, ground below, from the world normal) × the base colour, so shadowed sides keep their form.
- `Rim` × a cool Fresnel rim (exponent 3), so dark figures read against dark ground.

`Roughness` sets the surface: characters and metal 0.5 to 0.65, stone, cloth and ground 0.9 to 0.95. Characters get `Rim` and `Fill` 1; worlds and tiles get none; props a trace of `Fill`. `SelfLit` is a constant emissive share kept only for the flame (a light source) and the tabletop overview, which stays as it was.

**Accepted exception to gate 1's mask-only glow (decided by the lead, 2026-11-09):** the battle flame's `SelfLit` (1.4) and the tabletop overview's self-light stay. The r001 flame has no glow mask and is itself a light source. The exception lasts until r002 art with real glow masks lands (#145, #147), when both move onto `Glow` and `SelfLit` goes away.

**Vertex-alpha guard:** glTF `COLOR_0` is optional, and Unreal reads a missing vertex alpha as 1, which would make a whole mesh glow. At import the script reads each export's accessors and enables `Glow` only when `COLOR_0` is RGBA with a varying alpha. No r001 export has one (most have no `COLOR_0`; the tabletop and the two worlds have a constant alpha of 1), so every r001 instance imports with `Glow` 0 until a revised export paints a mask. The log lists `THEME_MATERIAL <mesh> glow=<n> mask=<bool>` per mesh.

**Reimport (#143, 2026-11-09):** one headless run (`UnrealEditor-Cmd -run=pythonscript -nullrhi -corelimit=2`) exited 0 with `THEME_IMPORT_COMPLETE 21/21` and no warnings. The script saves only dirty packages and assigns a mesh material only when it differs, so the run changed exactly `M_DiceRollTheme`, the 21 `MI_*` instances and the 21 `T_*` textures (filtering and mips). Hashes of `Config/` and `Content/` taken before and after showed no mesh, `DA_DiceRollTheme` or other file changed. It imported nothing new: every source is still a self-authored r001 export, so no licence changes.

**Reimport for anisotropic filtering (#143 review, 2026-11-09):** textures now use `TF_DEFAULT` in `TEXTUREGROUP_World`, which `Config/DefaultDeviceProfiles.ini` makes anisotropic with linear mips.
- **First headless run:** it also re-saved `M_DiceRollTheme` and the 21 `MI_*`, because the script rebuilt the master and re-set every parameter on each run. Those 22 files were restored to their reviewed versions, and the script was made idempotent.
- **Validation run, from the committed assets:** exit 0, no warnings, `THEME_MASTER kept`. The hash diff showed exactly the 21 `T_*` changed.
- **Immediate rerun:** changed nothing.

**Camera clip (#143 review, 2026-11-09):** the master became Masked, with a `CameraFade` clip (see "Theme material"). One headless run rebuilt it (`THEME_MASTER rebuilt`, exit 0, no warnings). The hash diff showed exactly `M_DiceRollTheme.uasset` changed: the instances and textures were untouched.

**Camera clip and the occluder (#143 and #145, decision):** `M_DiceRollTheme` is masked and clips scenery closer to the camera than `CameraFade` (0.5) x the camera's height, so a diorama around the board never swallows the camera. That clip is shared code (`camera_clip()` in the import script).
- **Occluder material:** it uses both, and they do not conflict. A pixel is kept only if the sight-line dither keeps it and it is outside the camera clip (the two masks are multiplied). Near-architecture is the geometry most likely to sit around the camera, so it needs the clip too; the dither handles what blocks the view farther along the line. The clip is a hard cut with a roughly 50 cm ramp, the dither a 15 % floor, so one never fights the other: near the camera the clip wins, along the sight line beyond it the dither does.
- **World and props:** the r002 world, props, scatter and the r001 worlds keep the master's clip. For the r002 world it is harmless: the ground is always farther from the camera than 0.5 x its height, and tall architecture within that radius is removed exactly like the occluder's. It matters mainly for the r001 worlds, whose scenery surrounds the board. Mist cards and light shafts (`M_DiceRollCard`) are translucent and are not clipped.

**Drowned Vigil world kit r002 (#145 phase 2):** 25 original assets generated by `Art/dark-fantasy-sources/r002/scripts/` in Blender 5.2.3 LTS from the r001 sources; no third-party content, same terms as the repository. Records are `Art/<asset-id>/r002/asset.json` (the world and variant-A props) or `r001/asset.json` (new ids), import policy v2. The list and what each is for are in `Art/dark-fantasy-sources/r002/README.md`; the table that drives the import is `Content/Python/theme_plan.py` (`R002`), and `tools/tests/test_theme_plan.py` checks every row against its record, hash and package.

Import policy (v2), applied by `import_dark_fantasy_theme.py`:
- **New names, explicit replacement.** 21 assets are new packages (`SM_World_Vigil_Occluders`, `SM_Prop_*_B/_C`, `SM_Prop_Arch2Cell`, `SM_Scatter_*`, `SM_Fx_*`); an existing package is never overwritten. Four revised packages keep their recorded path and are replaced only on request: `DND_THEME_REIMPORT=world-vigil,prop-pillar,prop-wall,prop-stones` (or `all`). An id outside that allow-list is an error, and the four go together because the board scales r001 art and places r002 art unscaled (`UDiceRollTheme::bMetricKit`, set by the script only when all four carry the r002 tag).
- **Per mesh:** Nanite forced off (UE's glTF importer enables it), simple collision removed, and the package tagged `DiceRollAssetRevision=vigil-kit-r002`. **Explicit LODs for meshes over ~1k triangles; cards, light shafts and small scatter are exempt.** The world, the occluders, the nine obstacle variants, the 2-cell arch and `SM_Scatter_Roots` have `<name>_LOD1.glb` (about 50 %) and `<name>_LOD2.glb` (about 25 %, up to 40 % where the silhouette needed it) beside the export, made by `build_lods_r002.py` and listed in the asset record with their hashes and triangle counts; the script imports both, checks them against the record and fails unless the mesh ends with exactly the recorded LOD count (`lods=3`; exempt meshes `lods=1`). r001 meshes are only reported; `DND_THEME_FIX_R001_NANITE=1` switches their Nanite flag off too and changes their bytes.
- **Textures:** one atlas per mesh (the occluder repeats the world's 2048 px atlas), `TF_DEFAULT` in `TEXTUREGROUP_World` (anisotropic 8 with linear mips through `Config/DefaultDeviceProfiles.ini`, as #143), mips generated, sRGB, default compression (r001 stays uncompressed). Texture and instance settings are written only when they differ, so a rerun leaves unchanged assets byte-identical.
- **Materials:** a `MI_*` per mesh. Lit pieces use `M_DiceRollTheme` (Glow only where COLOR_0 is RGBA with varying alpha, which the r002 world, occluder and candles have). `M_DiceRollOccluder` (masked, dithered around the camera sight lines; parameters `SightEye`, `SightFocus`, `SightUnit`, `FadeRadius`, `FadeMin`, plus the master's `CameraFade` clip, see below) and `M_DiceRollCard` (translucent, unlit, two-sided; mist cards and light shafts, opacity = COLOR_0.a) are created once and never overwritten.
- **Log and save:** one `THEME_ASSET` line per asset (action, source SHA-256, triangles, Nanite, LOD count, collision shapes), a `THEME_FILE` line for every Theme file whose SHA-256 changed, `THEME_FILES_CHANGED <n>`; only dirty packages are saved and a failed save raises.
- **Run record:** pending. The commandlet run, its log, the file-change set and the before/after hashes are recorded here by the lead after the Unreal pass; no r002 asset is validated until then.

**Contents:**

| Group | Assets |
| --- | --- |
| Characters | Seven: four heroes, raider, scout and archer |
| Worlds | Root of the World and Drowned Vigil, each with baked vertex lighting, plus their board tiles |
| Props | Stones, pillar, wall, brazier, flame, banner and cage |
| Overview | The candlelit tabletop and two route dioramas |

**Fallbacks:** `AExpeditionBattleView` falls back to the engine primitives below when an entry is missing. The `-ExpeditionPrimitives` command-line flag forces that fallback.

## Game splash (issue #131)

`Content/Splash/Splash.bmp` is the splash the packaged Linux game shows while it loads. It is a raw file rather than an Unreal asset: the engine reads `<Project>/Content/Splash/Splash.bmp` directly, and Linux packaging stages that exact file outside the pak (`LinuxPlatform.Automation.cs`). Non-editor builds load it with `SDL_LoadBMP`, so it is an uncompressed 24-bit BMP, 720×370 like the engine's own splash. `Content/Splash/Icon.bmp` (64×64, 24-bit) is the splash window's icon: once a splash exists, the engine also looks up `Splash/Icon` and logs `Game icon not found.` without it. Packaging stages only `Splash.bmp` by itself, so `Config/DefaultGame.ini` adds `+DirectoriesToAlwaysStageAsNonUFS=(Path="Splash")` to ship the folder. Both files are Git LFS tracked (`*.bmp`).

**Authorship and license:** original project art, made by this project's agent for the repository owner from the dark-fantasy theme above; project-owned, same terms as the repository. No third-party, downloaded or AI-image-service art.

**How it was made:** `python3 Art/splash/r001/make_splash.py` (Blender 5.2.3 LTS, ImageMagick 7.1.2):
- Blender renders the theme's own `Art/tabletop/r001/SM_Tabletop.glb` unlit (texture × baked candlelight, via `render_lib.py`) at 1440×740 → `Art/splash/r001/backdrop.png`.
- ImageMagick dims the bookshelf behind the title, adds a vignette, the gilded double frame with the corner diamonds of the in-game panels, an ornamental rule and the title "dnd-diceroll", then box-downsamples to 720×370 with no stretching → `Art/splash/r001/Splash.png` (preview) and `Content/Splash/Splash.bmp`.
- The same script draws the icon, a gold "d" monogram in the gilded frame → `Art/splash/r001/Icon.png` (preview) and `Content/Splash/Icon.bmp`.
- The title is set in Noto Serif Display Bold (SIL Open Font License 1.1, system font); only rendered letterforms are in the image, and no font file is included.
- Rebuilding: the script strips the render's metadata, so the same Blender, GPU driver and font give identical files. Another GPU or driver may shift the render's pixels slightly. On another system, point `SPLASH_FONT` at the font file (Arch package `noto-fonts`).

These are raw engine files, not `/Game/DnDDiceRoll` packages, so they have no `asset.json` record (its schema needs an Unreal package path). The editor splash is a separate image (`EdSplash`) and still uses the engine default.

## Tactical battle runtime presentation

`AExpeditionBattleView` builds every board tile, obstacle, themed decoration, character figure, marker and effect at runtime from `/Engine/BasicShapes/Cube`, `Cylinder`, `Sphere`, `Cone` and `/Engine/BasicShapes/BasicShapeMaterial`, plus runtime point lights, height fog and a post-process grade. Provider: Epic Games, bundled Unreal Engine content under the applicable Unreal Engine license. Runtime material instances apply original colors; no imported files or binary asset edits. The existing `/Game/DnDDiceRoll/Maps/L_AgentSandbox` remains the saved fixture. Original character/encounter names are authored in C++; supported SRD math and attribution are documented in [rules reference](rules-reference.md).

## Runtime audio

All sound effects (dice, hits, misses, spells, healing, footsteps, turn chimes, camp, loot, victory and defeat) are synthesized in C++ by `ExpeditionAudio` and played through runtime `USoundWaveProcedural` objects. No audio files are imported or stored.

## Board overlay material (issue #142)

`/Game/DnDDiceRoll/Theme/Materials/M_BoardOverlay` is a translucent, unlit, two-sided material (emissive `Color`, opacity `Opacity`) used by the contextual grid and footprint rings. Original work by this project's agents, created by `Content/Python/create_board_overlay_material.py`; no third-party content. Created on 2026-11-09 by one headless run (`UnrealEditor-Cmd -run=pythonscript -nullrhi -corelimit=2`) that exited 0 with no warnings. Hashes of `Config/` and `Content/` taken before and after showed that `M_BoardOverlay.uasset` was the only added Unreal asset; the hash diff showed nothing else changed apart from three ignored `Content/Python/unreal_starter/__pycache__/*.pyc` files, which were removed. A rerun leaves an existing asset alone.
