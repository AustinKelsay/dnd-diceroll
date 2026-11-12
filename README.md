# Evidence for PR #199 (#149: Tabletop, route dioramas and the Vigil window)

Head `6022877` (round 1 of the lead's spot check of `8b8d161`), PlebDev Ultra, UE 5.8.4. The gallery indexes (`gallery-index/`) record the commit and a clean tree.

- `overview/`: the title (`start-screen`) and the route screen (`overview`) on `default-seed-route` (the trailhead: forest on the left, cathedral on the right) and `final-route` (before the finale: two Vault roads), 1920x1080 and 1280x720. **The dioramas now stand beside the route cards on the lower table** (route and finale) and between the drawers (title), clear of the banner, cards, party cards and tooltip.
- `closeups/`: the dioramas cropped from the 1920x1080 captures at pixel size (the overview camera is fixed), and the tankard before (`8b8d161`) and after.
- `window/`: the Vigil stained-glass rose at the pan stop, before (14.2 texels/m) and after (222.2 texels/m), poses `a`, `d`, `e` (`tools/capture/vigil-window-poses.json`).
- `blender/`: Blender previews through the game's overview camera with the HUD blocks drawn in grey, and a close view per diorama; stand-in lighting.
- `metrics/`: `tankard-luma-*` (peak 255 and 1.25 % pure white before; 237 to 245 and 0 % after, from `tools/overview_probe.py`), `rose_metrics.json`, tabletop and diorama metrics, the import hash diffs.
- `gallery-index/`: `index.json` of the full gallery (18 runs, 576 pictures), the overview run and the window run, and the window manifest (it records the `pan`).
