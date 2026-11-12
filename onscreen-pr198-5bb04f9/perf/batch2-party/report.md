# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit 5bb04f9a197602c997b0348a80e52b7ffac41f1b; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.34 ms | 16.7 ms | pass |
| p95 GPUTime | 2.67 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.16 ms | 4.0 ms | pass |
| max RSS | 1267.7 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 29268 | measured pass, gate | 2.77 ms | 3.34 ms | 6.82 ms | 2.67 ms | 1.16 ms |
| 29206 | measured pass, fight only | 2.77 ms | 3.34 ms | 6.82 ms | 2.67 ms | 1.16 ms |
| 20 | resume pass | 2.77 ms | 2.93 ms | 2.94 ms | 2.62 ms | 0.77 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 37.4 ms (frame 183) | 37 ms | 1 | 2.0 ms | 37.3 ms |
| start->route | measured | 5.4 ms (frame 207) | 2269 ms | 1021 | 2.8 ms | 5.6 ms |
| route->battle | measured | 59.2 ms (frame 1251) | 150 ms | 31 | 4.3 ms | 58.1 ms |
| battle->reward | measured | 5.0 ms (frame 31782) | 5640 ms | 2016 | 4.2 ms | 2.9 ms |
| quit | measured | 3.3 ms (frame 32532) | 68 ms | 31 | 2.6 ms | 1.5 ms |
| resume | resume | 59.7 ms (frame 181) | 2085 ms | 723 | 5.2 ms | 58.6 ms |
| reward->route | resume | 6.0 ms (frame 1710) | 2392 ms | 857 | 5.6 ms | 3.9 ms |

## Memory

VmRSS sampled every 0.5 s (200 samples): max 1267.7 MiB; kernel high-water mark (VmHWM) 1262.7 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.85 ms | 0.70 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.27 ms | 0.27 ms |
| GPU/SSAO | 0.21 ms | 0.20 ms |
| GPU/Unaccounted | 0.20 ms | 0.19 ms |
| GPU/Lights | 0.18 ms | 0.17 ms |
| GPU/Basepass | 0.12 ms | 0.12 ms |
| GPU/BuildRenderingCommandsDeferred | 0.10 ms | 0.09 ms |

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
