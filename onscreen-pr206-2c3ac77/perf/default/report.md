# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit 2c3ac779606646afdfd93fa7a724178ecd7a2c36; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.41 ms | 16.7 ms | pass |
| p95 GPUTime | 2.69 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.25 ms | 4.0 ms | pass |
| max RSS | 1227.0 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 25031 | measured pass, gate | 2.77 ms | 3.41 ms | 46.05 ms | 2.69 ms | 1.25 ms |
| 24969 | measured pass, fight only | 2.77 ms | 3.41 ms | 46.05 ms | 2.69 ms | 1.25 ms |
| 20 | resume pass | 2.77 ms | 3.59 ms | 3.59 ms | 2.63 ms | 0.87 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 25.6 ms (frame 183) | 26 ms | 1 | 2.6 ms | 24.9 ms |
| start->route | measured | 6.9 ms (frame 207) | 2267 ms | 1012 | 3.9 ms | 7.4 ms |
| route->battle | measured | 59.4 ms (frame 1242) | 150 ms | 31 | 4.5 ms | 58.7 ms |
| battle->reward | measured | 6.6 ms (frame 27320) | 5509 ms | 1967 | 4.4 ms | 4.6 ms |
| quit | measured | 3.4 ms (frame 28236) | 68 ms | 31 | 2.6 ms | 1.6 ms |
| resume | resume | 70.1 ms (frame 181) | 2086 ms | 716 | 4.8 ms | 69.8 ms |
| reward->route | resume | 6.7 ms (frame 1737) | 2387 ms | 848 | 4.5 ms | 6.5 ms |

## Memory

VmRSS sampled every 0.5 s (178 samples): max 1227.0 MiB; kernel high-water mark (VmHWM) 1227.0 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.85 ms | 0.70 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.27 ms | 0.27 ms |
| GPU/Unaccounted | 0.21 ms | 0.20 ms |
| GPU/SSAO | 0.21 ms | 0.20 ms |
| GPU/Lights | 0.18 ms | 0.17 ms |
| GPU/Basepass | 0.13 ms | 0.12 ms |
| GPU/BuildRenderingCommandsDeferred | 0.09 ms | 0.09 ms |

## Run

Warm-up pass: yes. Idle time between passes (other jobs may have run): warmup to measured: 10 s, measured to resume: 1 s.

## Passes

| Pass | Exit | Log | CSV |
| --- | --- | --- | --- |
| warmup | 0 | warmup/game.log | n/a |
| measured | 0 | measured/game.log | measured/measured.csv |
| resume | 0 | resume/game.log | resume/resume.csv |

## Host

- cpu: Intel(R) Core(TM) i9-14900K
- logical_cpus: 32
- memory_total_mb: 15736
- kernel: 7.2.15-1-omarchy
- gpu: NVIDIA GeForce RTX 4080 SUPER, 615.78.08, 16376 MiB
