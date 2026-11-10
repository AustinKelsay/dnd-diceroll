# PR #163: hatched enemy silhouette, deterministic offscreen capture

Requested by the coordinator after #163's on-screen pass, where no raider stood fully behind architecture in any frame. The
check is that side reads without colour.

- **Build:** PR #163 head `3e5986c` (`feature/145-vigil-world-kit`), PlebDev Ultra, UE 5.8.4, offscreen game at 1920×1080
  (`-RenderOffscreen -UseFixedTimeStep -FPS=30`), under the machine lock.
- **Fixture:** `board-vigil-pillars` (Watchtower road, seed 1) with two units moved onto free cells behind pillars:
  - Raider 1 from (9,4) to (8,5), behind the pillar at (7,5);
  - Sera from (4,5) to (12,3), behind the pillar at (11,3).

  The edited checkpoint is `fixture-current-v1.json`. It resumes through the normal Resume path, and nothing else changed.
- **Poses:** `poses.json` (eight yaws, distances 1200 and 2000). `manifest.json` is the capture's own manifest.
- **Frames:**
  - `shots/enemy-hatch-yaw180.png`: Raider 1 behind the pillar shows the **diagonal hatch** (HUD Danger, stencil 2).
  - `shots/party-fill-yaw000.png`: a party figure behind a pillar top shows the **solid, brighter fill** (HUD Sky, stencil 1).
- **`silhouettes-compare.png`:** the two crops side by side, in colour on top and greyscale below. In greyscale the enemy is
  striped and the party solid, so side reads without colour.
