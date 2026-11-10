# PR #176: silhouettes behind Root architecture, deterministic offscreen capture

Requested by the coordinator after #176's on-screen pass, where no figure stood fully behind a Root trunk, wall or arch.

- **Build:** PR #176 head `033ae1d` (`feature/149-root-r002-import`, which includes #175 `8b412ab` and staging with #178), on PlebDev Ultra with UE 5.8.4. It ran as an offscreen game at 1920×1080 (`-RenderOffscreen -UseFixedTimeStep -FPS=30`), under the machine lock.
- **Fixture:** `board-root-r002` (Stream trail, seed 3, the broken-wall recipe), with two units moved onto free cells behind the column-7 Root wall and its arches:
  - Scout 1 from (11,3) to (8,3);
  - Arden (the rigged fighter) from (4,3) to (8,6).

  The edited checkpoint is `fixture-current-v1.json`. It resumes through the normal Resume path.
  - Guarded-road enemies are scouts, and rigged raiders only spawn on the dangerous route, which uses the Vigil world. Renaming the scout to a raider makes the checkpoint invalid: save validation ties enemy names to the route. So the enemy here is a scout, and its silhouette is the same enemy hatch (stencil 2).
- **Poses:** `poses.json` holds eight yaws at distances 1300 and 2000. `manifest.json` is the capture's own manifest.
- **`shots/both-hidden-yaw225.png`:** both figures are hidden in one frame.
  - The scout behind a Root arch shows the **diagonal enemy hatch** (HUD Danger).
  - Arden behind a Root trunk shows the **solid, brighter party fill** (HUD Sky).
- **`root-silhouettes-compare.png`:** the two crops side by side, colour above and greyscale below. In greyscale the enemy is striped and the party solid, so side reads without colour.
