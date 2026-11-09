# Frame timing: silhouettes and the r002 kit (PR #163)

Offscreen, 1920×1080, fixture `board-18x12` (Vigil, Watchtower road), yaw -90 at default zoom. CSV profiler, 500 steady frames after 300 frames of settling.
- **Base:** PR #159 head `7a0c6ef` (r001 Vigil).
- **Head:** PR #163 head `b6d86b7`: the r002 kit, silhouettes, and the overlay drawn without a depth test.

| Measure | Base median | Head median | Change |
| --- | --- | --- | --- |
| GPU | 1.99 ms | 2.33 ms | +0.34 ms |
| Frame | 2.19 ms | 2.53 ms | +0.34 ms |
| Shadow depths | 0.09 ms | 0.31 ms | +0.22 ms (r002 world and kit geometry) |
| Custom depth | none | 0.005 ms | 7 draw calls |
| Post-processing | 0.236 ms | 0.261 ms | +0.025 ms (silhouette) |

The silhouettes themselves cost about 0.03 ms of GPU time. The rest of the change is the heavier r002 world and kit.
