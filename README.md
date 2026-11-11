# Evidence for PR #199 (#149: Tabletop, route dioramas and the Vigil window)

Head `8b8d161`, PlebDev Ultra, UE 5.8.4. Gallery indexes (`gallery-index/`) record the commit and a clean tree.

- `overview/`: the title (`start-screen`) and the route screen (`overview`) on `default-seed-route` (trailhead: forest and cathedral) and `final-route` (before the finale: the Vault), 1920x1080 and 1280x720.
- `window/`: the Vigil stained-glass rose at the pan stop, **before** (r001 occluder atlas, 14.2 texels/m) and **after** (222.2 texels/m): poses `a`, `d`, `e` (`tools/capture/vigil-window-poses.json`); `crop-*` is the bottom band of pose `a` at full resolution.
- `closeups/`: the dioramas cropped from the game captures, at pixel size (the overview camera is fixed).
- `blender/`: the Blender previews through the game's overview camera (HUD blocks drawn in grey) and a close view per diorama; stand-in lighting.
- `metrics/`: `rose_metrics.json` (texel density before/after, appearance difference), tabletop and diorama metrics, the import hash diffs.
- `gallery-index/`: `index.json` of the full gallery (18 runs, 576 pictures), the overview run and the window run, with the window manifest (it records the `pan`).
