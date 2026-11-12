# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit 2c3ac779606646afdfd93fa7a724178ecd7a2c36; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.38 ms | 16.7 ms | pass |
| p95 GPUTime | 2.66 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.20 ms | 4.0 ms | pass |
| max RSS | 1253.5 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 23993 | measured pass, gate | 2.77 ms | 3.38 ms | 6.42 ms | 2.66 ms | 1.20 ms |
| 23931 | measured pass, fight only | 2.77 ms | 3.38 ms | 6.42 ms | 2.66 ms | 1.19 ms |
| 20 | resume pass | 2.77 ms | 2.94 ms | 3.02 ms | 2.63 ms | 0.79 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 18.3 ms (frame 183) | 18 ms | 1 | 2.2 ms | 18.3 ms |
| start->route | measured | 6.4 ms (frame 384) | 2268 ms | 1007 | 4.6 ms | 4.3 ms |
| route->battle | measured | 66.3 ms (frame 1237) | 159 ms | 31 | 4.3 ms | 65.3 ms |
| battle->reward | measured | 6.4 ms (frame 25465) | 5641 ms | 2017 | 6.0 ms | 3.7 ms |
| quit | measured | 4.2 ms (frame 27252) | 74 ms | 31 | 3.7 ms | 2.0 ms |
| resume | resume | 62.6 ms (frame 181) | 2086 ms | 719 | 4.2 ms | 62.0 ms |
| reward->route | resume | 4.4 ms (frame 947) | 2393 ms | 858 | 3.5 ms | 4.2 ms |

## Memory

VmRSS sampled every 0.5 s (170 samples): max 1253.5 MiB; kernel high-water mark (VmHWM) 1253.5 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.85 ms | 0.69 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.27 ms | 0.27 ms |
| GPU/SSAO | 0.21 ms | 0.20 ms |
| GPU/Unaccounted | 0.20 ms | 0.19 ms |
| GPU/Lights | 0.18 ms | 0.17 ms |
| GPU/Basepass | 0.13 ms | 0.12 ms |
| GPU/BuildRenderingCommandsDeferred | 0.09 ms | 0.09 ms |

## Run

Warm-up pass: yes. Idle time between passes (other jobs may have run): warmup to measured: 83 s, measured to resume: 8 s.

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
