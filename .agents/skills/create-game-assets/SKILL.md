---
name: create-game-assets
description: Plan, source or generate, and validate cohesive game asset families for this Unreal project. Use for art direction, icons, textures, props, character art, or asset import briefs.
license: Apache-2.0
---

# Create game assets

Adapted from the pinned upstream skill; source and file hashes are in [UPSTREAM.json](UPSTREAM.json).

## Project contract

Follow the [contribution workflow](../../../docs/agents/workflow.md) for the approved slice, claims, review and delivery. Read [development](../../../docs/development.md) before editor/import work; it owns editor identity, ownership, MCP discovery, explicit saves and evidence. Read the [game brief](../../../docs/game-brief.md), [architecture](../../../docs/architecture.md), and any available project domain docs before deciding gameplay-facing art requirements. Existing decisions and assets are the starting point.

## Production workflow

1. Inspect current assets, provenance, import settings and game captures. Record the camera, expected screen size, world scale, silhouette needs, palette and texture/material budgets. Use [the art brief](assets/art-direction-brief.md) when a visual direction needs recording.
2. Name the required family and its states or variants. Copy [the manifest template](assets/asset-manifest.json) into the slice's source-art directory, replacing its example paths and pixel-art settings with the actual Unreal asset contract. Keep editable source in `Art/` as described in [asset provenance](../../../docs/assets.md).
3. Establish one representative visual target at actual game scale before producing the family. Use an existing approved target when available. If the owner delegated the choice, select and record it; otherwise resolve only the undecided design choice. Keep the reference, view, palette, proportions and lighting consistent across variants.
4. Choose licensed source assets, author in a DCC, or use the available image-generation skill/tool. Follow the installed `imagegen` skill for actual generation/editing when available. For 3D, a generated concept is reference material; inspect the actual mesh, topology, UVs, normals, rig, pivots, collision and LODs separately.
5. Normalize source files deterministically and check the technical contract. Use the raster helpers below for dimensions, colors, alpha and contact sheets. A passing alpha check only proves that some transparency exists; inspect the image for a baked checkerboard or unwanted background.
6. Import through Unreal APIs/MCP into the approved path under `/Game/DnDDiceRoll`. Select the interchange format supported by the installed Unreal importer and this asset type. Inspect units, axes, pivots, material slots, color space, normal maps, compression, mipmaps and collision. Preserve existing accepted assets on rerun; save and reload intentionally.
7. Inspect the family in the actual game camera against real backgrounds. Check role/state distinction, selection/targeting visibility, silhouette, animation stability and scale. Art changes must preserve authoritative rules and command validation.
8. Record source URL or tool/recipe, creator, license, restrictions and modifications in `docs/assets.md`. The family manifest supplements that canonical provenance record. Report changed asset paths, checks, inspected captures and any unfinished integration.

## Raster helpers

Resolve these paths from this skill directory. Use host Python 3.11+ in a local virtual environment with [the optional Pillow dependency](scripts/requirements.txt); the Unreal helper itself remains dependency-free. Keep QA reports/contact sheets in ignored `.local/`, `Saved/` or `Builds/`. Example commands, with the slice's actual source paths and constraints:

```sh
python3 scripts/asset_report.py /path/to/icon.png --expect-size 64x64 --require-alpha --max-colors 48 --json
python3 scripts/build_preview_sheet.py /path/to/icon-a.png /path/to/icon-b.png --out /path/to/project/.local/art-review.png --columns 2
```

Raster QA is source-file evidence. Production acceptance also needs saved/reloaded Unreal assets and an inspected in-game view.

## Focused references

- For a visual system and family consistency, read [art direction](references/art-direction.md).
- For alpha, sprite/texture normalization or tiling, read [raster production](references/raster-pipeline.md).
- For mesh/material/rig work, read [3D production](references/three-d-pipeline.md).
- For source terms and generation records, read [provenance](references/provenance.md).

These upstream references contain cross-engine examples. Apply their concepts through the installed Unreal toolset and the project contract above; they do not require other catalog skills or tools.
