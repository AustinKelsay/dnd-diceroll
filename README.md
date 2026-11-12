# Evidence for PR #199 (#149: Tabletop, route dioramas and the Vigil window)

Head `53ba18e` (review round 1 of `6022877`), PlebDev Ultra, UE 5.8.4. The gallery indexes (`gallery-index/`) record the commit and a clean tree.

- `overview/`: the title (`start-screen`) and the route screen (`overview`) on `default-seed-route` (the trailhead: forest on the left, cathedral on the right) and `final-route` (before the finale: two Vault roads), 1920x1080 and 1280x720. The dioramas stand beside the route cards on the lower table, now **below the tallest hero-details panel of every party slot** (the slot-4 panel reaches design y 408; the dioramas start at 421), and between the drawers on the title.
- `closeups/`: the dioramas cropped from the 1920x1080 captures at pixel size, and the tankard before (`8b8d161`) and after.
- `root-self-lit/`: the Root diorama on the 1280x720 route screen before (`6022877`, self-lit 0.6) and after (self-lit 0.85, and the new spot); the diorama also moved down and left with the layout.
- `window/`: the Vigil stained-glass rose at the pan stop, before (14.2 texels/m) and after (222.2 texels/m), poses `a`, `d`, `e`.
- `blender/`: Blender previews through the game's overview camera with the HUD blocks drawn in grey, and a close view per diorama; stand-in lighting.
- `metrics/`: tankard luma before and after, `rose_metrics.json`, diorama metrics, the import hash diffs, `blender-exit-code-proof.txt` and the injected-failure log (a validator that hits an unexpected exception now exits 1).
- `gallery-index/`: `index.json` of the full gallery (18 runs, 576 pictures), the overview run and the window run, and the window manifest.
