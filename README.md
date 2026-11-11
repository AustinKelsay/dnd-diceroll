# Evidence for PR #197 (Vault finale world, #149), round 1

Tied to head `068cfb143b580172382066853babbb6cbb61eaf2` (index.json: commit `068cfb143b580172382066853babbb6cbb61eaf2`, dirty false, engine 5.8.4). Captured offscreen with `python3 tools/dev.py --cores 2 gallery` (the default gallery, 18 runs, 576 pictures) on the merged head (staging `4d4db3f`, #196 included, so the party is the rigged cast: fighter downed, rogue acting, cleric and wizard standing). `board-vault` is seed 4, the 18 x 12 broken-wall finale.

- `vault/<res>/yaw{m90,000,090,180}-{near,default,far}.jpg`: four yaws x near, default and far, 1920x1080 and 1280x720 (`yawm90` is the home yaw; the front is open and the Waystone apse is on the far wall).
- `gallery-run/index.json` and `board-vault-<res>-manifest.json`: the run's index and per-resolution manifests (shot, mode, yaw, distance, camera, seed, board).
- `vault-pillars/`: the 16 x 12 ruined-pillar finale (`board-vault-pillars`, a separate run).
- `themes/`: Meadow, Ruins and the overview for comparison; contact sheets `vault-contact-sheet-1080-{near,default,far}.jpg`, `themes-contact-sheet.jpg`, `blender/blender-contact-sheet.png` (Blender stand-in lighting).
