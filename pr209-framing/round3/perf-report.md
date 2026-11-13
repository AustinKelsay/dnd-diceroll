# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit 322936a63cde7c5f3b17f31eef33f5e77c9dd959; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.59 ms | 16.7 ms | pass |
| p95 GPUTime | 2.96 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.23 ms | 4.0 ms | pass |
| max RSS | 1264.3 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 24378 | measured pass, gate | 2.81 ms | 3.59 ms | 7.09 ms | 2.96 ms | 1.23 ms |
| 24316 | measured pass, fight only | 2.81 ms | 3.59 ms | 7.09 ms | 2.96 ms | 1.23 ms |
| 20 | resume pass | 2.80 ms | 3.40 ms | 3.51 ms | 2.94 ms | 0.88 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 22.8 ms (frame 183) | 23 ms | 1 | 3.8 ms | 22.8 ms |
| start->route | measured | 8.9 ms (frame 207) | 2269 ms | 980 | 3.5 ms | 9.2 ms |
| route->battle | measured | 55.5 ms (frame 1210) | 147 ms | 31 | 4.9 ms | 54.6 ms |
| battle->reward | measured | 6.6 ms (frame 26095) | 5509 ms | 1869 | 5.8 ms | 3.0 ms |
| quit | measured | 3.7 ms (frame 27453) | 68 ms | 31 | 2.7 ms | 1.6 ms |
| resume | resume | 54.5 ms (frame 181) | 2088 ms | 697 | 5.0 ms | 54.3 ms |
| reward->route | resume | 4.5 ms (frame 1596) | 2388 ms | 813 | 5.4 ms | 3.9 ms |

## Memory

VmRSS sampled every 0.5 s (177 samples): max 1264.3 MiB; kernel high-water mark (VmHWM) 1264.3 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.90 ms | 0.71 ms |
| GPU/ShadowDepths | 0.49 ms | 0.34 ms |
| GPU/Postprocessing | 0.43 ms | 0.28 ms |
| GPU/Basepass | 0.30 ms | 0.14 ms |
| GPU/BuildRenderingCommandsDeferred | 0.24 ms | 0.09 ms |
| GPU/Unaccounted | 0.21 ms | 0.20 ms |
| GPU/SSAO | 0.21 ms | 0.20 ms |
| GPU/Lights | 0.19 ms | 0.19 ms |

## Run

Warm-up pass: yes. Idle time between passes (other jobs may have run): warmup to measured: 0 s, measured to resume: 1 s.

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
