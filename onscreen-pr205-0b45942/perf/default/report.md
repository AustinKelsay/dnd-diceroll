# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit 0b459428b0381af41cf9db30f543804d33a6ae33; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.38 ms | 16.7 ms | pass |
| p95 GPUTime | 2.69 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.20 ms | 4.0 ms | pass |
| max RSS | 1227.4 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 25230 | measured pass, gate | 2.77 ms | 3.38 ms | 7.25 ms | 2.69 ms | 1.20 ms |
| 25168 | measured pass, fight only | 2.77 ms | 3.38 ms | 7.25 ms | 2.69 ms | 1.20 ms |
| 20 | resume pass | 2.83 ms | 4.20 ms | 4.60 ms | 3.84 ms | 1.09 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 22.8 ms (frame 183) | 23 ms | 1 | 2.2 ms | 22.8 ms |
| start->route | measured | 7.0 ms (frame 207) | 2268 ms | 1018 | 2.2 ms | 7.0 ms |
| route->battle | measured | 58.2 ms (frame 1248) | 147 ms | 31 | 4.4 ms | 57.4 ms |
| battle->reward | measured | 5.2 ms (frame 27541) | 5504 ms | 1968 | 4.0 ms | 2.9 ms |
| quit | measured | 3.4 ms (frame 28442) | 68 ms | 31 | 2.6 ms | 1.6 ms |
| resume | resume | 61.7 ms (frame 181) | 2090 ms | 712 | 5.1 ms | 61.3 ms |
| reward->route | resume | 5.8 ms (frame 1740) | 2387 ms | 855 | 3.2 ms | 5.6 ms |

## Memory

VmRSS sampled every 0.5 s (177 samples): max 1227.4 MiB; kernel high-water mark (VmHWM) 1227.4 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.85 ms | 0.70 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.27 ms | 0.27 ms |
| GPU/Unaccounted | 0.20 ms | 0.19 ms |
| GPU/SSAO | 0.20 ms | 0.20 ms |
| GPU/Lights | 0.18 ms | 0.17 ms |
| GPU/Basepass | 0.13 ms | 0.13 ms |
| GPU/BuildRenderingCommandsDeferred | 0.09 ms | 0.09 ms |

## Run

Warm-up pass: yes. Idle time between passes (other jobs may have run): warmup to measured: 78 s, measured to resume: 80 s.

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
