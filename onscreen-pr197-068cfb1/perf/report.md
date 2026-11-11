# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit 068cfb143b580172382066853babbb6cbb61eaf2; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.38 ms | 16.7 ms | pass |
| p95 GPUTime | 2.66 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.19 ms | 4.0 ms | pass |
| max RSS | 1250.0 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 22288 | measured pass, gate | 2.77 ms | 3.38 ms | 6.39 ms | 2.66 ms | 1.19 ms |
| 22226 | measured pass, fight only | 2.77 ms | 3.38 ms | 6.39 ms | 2.66 ms | 1.19 ms |
| 20 | resume pass | 2.75 ms | 2.96 ms | 3.75 ms | 2.62 ms | 1.16 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 23.7 ms (frame 183) | 24 ms | 1 | 1.9 ms | 23.3 ms |
| start->route | measured | 8.8 ms (frame 207) | 2267 ms | 1026 | 4.0 ms | 9.2 ms |
| route->battle | measured | 61.6 ms (frame 1256) | 151 ms | 31 | 4.5 ms | 60.9 ms |
| battle->reward | measured | 5.0 ms (frame 23967) | 4772 ms | 1706 | 4.9 ms | 3.0 ms |
| quit | measured | 5.1 ms (frame 25245) | 66 ms | 31 | 2.5 ms | 4.3 ms |
| resume | resume | 57.5 ms (frame 181) | 2085 ms | 714 | 5.3 ms | 57.2 ms |
| reward->route | resume | 4.6 ms (frame 1750) | 2385 ms | 863 | 2.9 ms | 4.4 ms |

## Memory

VmRSS sampled every 0.5 s (159 samples): max 1250.0 MiB; kernel high-water mark (VmHWM) 1245.7 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.84 ms | 0.69 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.27 ms | 0.27 ms |
| GPU/SSAO | 0.21 ms | 0.20 ms |
| GPU/Unaccounted | 0.20 ms | 0.19 ms |
| GPU/Lights | 0.18 ms | 0.17 ms |
| GPU/Basepass | 0.12 ms | 0.12 ms |
| GPU/BuildRenderingCommandsDeferred | 0.10 ms | 0.09 ms |

## Run

Warm-up pass: yes. Idle time between passes (other jobs may have run): warmup to measured: 16 s, measured to resume: 8 s.

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
