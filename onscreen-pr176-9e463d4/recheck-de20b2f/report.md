# PR #176 short re-check at `de20b2f`

Head `de20b2f5b65be889541c86e36750d01e0cd4e428` (unchanged when packaged; contains #175 `a32e405`). Binary SHA-256 `cf78702f798fcd8959dc98a1d73f0d6912c354ea1b2321c2e2a6bc0d0cae37c4`; package 0 errors, 0 warnings. Captures only (game's capture mode, 1920x1080 and 1280x720, `-UseFixedTimeStep -FPS=30`, `-RenderOffscreen`), under the machine lock; staging `7f29981` package used for the Vigil comparison. 8 captures, 0 Error in all logs.

| Check | Result | What was seen |
| --- | --- | --- |
| Cage / banner / rail pose on the Vigil pillars board (`targeting-yawm90-default`) | **Pass** | PSNR staging vs #176: 52.8 dB (1920x1080), 51.0 dB (1280x720), the same as #175 `a32e405` alone (52.8 / 51.0). The cage bars match staging (`sheets/CAGE-staging-vs-176.png`). Faint stipple-phase difference only, as in the #175 re-run. |
| Reach contour pose (`battle-yaw000-default`, pillars board) | **Pass** | 68.1 dB / 73.1 dB; contour pixels identical in colour to staging (mean RGB 218, 232, 211, 4797 vs 4796 pixels). Not whitened (`sheets/CONTOUR-staging-vs-176.png`). |
| `board-root-r002` `battle-yawm90-default` | **Pass** | Renders correctly at both sizes (`sheets/ROOT-r002-yawm90-default-1920-1280.png`): wooden arches, trunks, mushroom lamps, continuous paving, shadows, no grey LOD, no gaps. |
| Lead's Root silhouette evidence reproduced on this package (`evidence/pr176-root-silhouettes` `b857fc1`: his checkpoint and 16 poses, Resume path) | **Pass** | Pose `hidden-yaw225-d1300` at 1920x1080 (`sheets/lead-checkpoint-yaw225-1920.png`, `SILHOUETTES-enemy-party-colour-grey.png`) and 1280x720 (`sheets/lead-yaw225-1280.png`): the **scout behind the Root arch shows the diagonal orange hatch** (enemy), and **Arden (rigged) behind a Root trunk shows the solid light-blue fill** (party), both only where hidden; in greyscale the enemy is striped and the party solid, so they read without colour. The scout stands in for the raider (raiders do not spawn on Root boards). The other 15 poses were captured but not individually inspected. |

Counts: **Pass 4 / 4.** Caveats: captures are offscreen stills; no interactive session; only one of the 16 lead poses inspected per size.
