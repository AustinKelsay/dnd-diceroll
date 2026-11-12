# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit e24547b3675f3d5c91f9cc862f3facf615fdabfa; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.36 ms | 16.7 ms | pass |
| p95 GPUTime | 2.66 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.22 ms | 4.0 ms | pass |
| max RSS | 1231.6 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 25149 | measured pass, gate | 2.76 ms | 3.36 ms | 2499.23 ms | 2.66 ms | 1.22 ms |
| 25087 | measured pass, fight only | 2.76 ms | 3.36 ms | 2499.23 ms | 2.66 ms | 1.22 ms |
| 20 | resume pass | 2.75 ms | 2.96 ms | 3.14 ms | 2.62 ms | 0.98 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 23.7 ms (frame 183) | 24 ms | 1 | 2.0 ms | 23.4 ms |
| start->route | measured | 10.2 ms (frame 207) | 2267 ms | 1027 | 2.3 ms | 10.5 ms |
| route->battle | measured | 61.1 ms (frame 1257) | 151 ms | 31 | 4.4 ms | 60.3 ms |
| battle->reward | measured | 5.9 ms (frame 27251) | 5508 ms | 1968 | 5.1 ms | 3.1 ms |
| quit | measured | 3.5 ms (frame 28370) | 68 ms | 31 | 2.6 ms | 1.6 ms |
| resume | resume | 61.4 ms (frame 181) | 2087 ms | 700 | 4.9 ms | 60.9 ms |
| reward->route | resume | 6.2 ms (frame 1734) | 2386 ms | 861 | 3.0 ms | 6.1 ms |

## Memory

VmRSS sampled every 0.5 s (182 samples): max 1231.6 MiB; kernel high-water mark (VmHWM) 1226.4 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.84 ms | 0.70 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.27 ms | 0.27 ms |
| GPU/Unaccounted | 0.21 ms | 0.19 ms |
| GPU/SSAO | 0.21 ms | 0.20 ms |
| GPU/Lights | 0.18 ms | 0.17 ms |
| GPU/Basepass | 0.12 ms | 0.12 ms |
| GPU/BuildRenderingCommandsDeferred | 0.09 ms | 0.09 ms |

## Run

Warm-up pass: yes. Idle time between passes (other jobs may have run): warmup to measured: 576 s, measured to resume: 3 s.

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
