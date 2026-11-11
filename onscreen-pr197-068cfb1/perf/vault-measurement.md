# Vault measurement (board-vault fixture, 18x12 finale, PR #197 head 068cfb1)

Packaged Development game, presented to my nested headless Hyprland output at 1920x1080, `-novsync`, `-corelimit=2`,
CSV profiler (`-csvCaptureFrames=60000 -csvGpuStats`, a boot-time capture) plus the on-screen `-ExecCmds=stat unit,stat fps`
overlay. The fixture (`board-vault`, copied from the PR author's `Saved/CaptureFixtures`, same head) was resumed with `O`
and the camera was held at each zoom for 14-20 s; the wall-clock marks (`logs/marks-*.log`) were mapped onto the CSV by
anchoring the capture end to the game log's "Capture Ended" line (alignment error under 1 s; windows have a 2-3 s margin).
`rig/analyze_csv.py` does the slicing with the repo's `tools/perf.py` parser. RSS is /proc VmRSS/VmHWM sampled each second.
This is a static scene (no fight, no animation turn), so it measures the world, kit and board rendering, not fight VFX.

| Window | Run | Frames | FrameTime p50 / p95 / max | GPU p95 / max | GameThread p95 / max |
| --- | --- | --- | --- | --- | --- |
| default zoom | D | 6095 | 2.75 / **3.38** / 5.85 | **2.63** / 4.76 | **1.33** / 2.38 |
| default zoom | C | 6436 | 2.75 / **3.51** / 6.44 | **2.68** / 5.61 | **1.39** / 3.34 |
| far zoom (first wheel-out) | H | 6404 | 2.61 / **3.27** / 5.98 | **2.49** / 6.01 | **1.27** / 3.16 |
| far zoom (re-zoomed) | H | 6050 | 2.60 / 3.20 / 5.95 | 2.48 / 5.01 | 1.28 / 2.34 |
| near zoom (900 clamp) | H | 5726 | 2.75 / **3.35** / 6.12 | **2.65** / 5.84 | **1.26** / 2.51 |
| wheel transition out | H | 4918 | 2.60 / 3.14 / 6.42 | 2.49 / 5.52 | 1.34 / 2.64 |
| **budget (p95)** | | | **4.0** | **3.5** | **1.5** |

RSS: max VmRSS 1214.0 MiB (run H), 1207.7 MiB (run D), kernel VmHWM equal; limit 1536 MiB. `stat unit` shows Mem 1.17-1.18 GB.
`stat unit` crops (`screens/stat-unit-crops.jpg`): default Frame 2.83 ms / Game 1.01 / GPU 2.45 / Draws 182 / Prims 19.3K;
far 2.69 / 1.02 / 2.31 / 181 / 19.3K; near 2.73 / 0.97 / 2.49 / 166 / 16.7K. RenderRes reads 72.9 % (1400x788) in all three.

Notes: run C's default window is a 17 s slice that includes the first seconds after the resume settled, which is why it is
slightly worse than D (the same zoom). Runs D, E and C never moved the wheel (the rig bug in report.md "Rig finding"), so their
windows are all default zoom. The game thread p95 at the Vault (1.26-1.39 ms) is higher than the Watchtower gate's 1.19 ms but under 1.5.
