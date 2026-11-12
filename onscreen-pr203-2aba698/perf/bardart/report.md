# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit 2aba698641b2b8406af5913fce95e7c30478bd58; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.34 ms | 16.7 ms | pass |
| p95 GPUTime | 2.67 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.17 ms | 4.0 ms | pass |
| max RSS | 1248.0 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 37030 | measured pass, gate | 2.77 ms | 3.34 ms | 8.65 ms | 2.67 ms | 1.17 ms |
| 36968 | measured pass, fight only | 2.77 ms | 3.34 ms | 8.65 ms | 2.67 ms | 1.17 ms |
| 20 | resume pass | 2.88 ms | 3.38 ms | 3.39 ms | 3.07 ms | 0.98 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 30.1 ms (frame 183) | 30 ms | 1 | 2.0 ms | 30.6 ms |
| start->route | measured | 5.3 ms (frame 207) | 2267 ms | 1020 | 2.3 ms | 5.8 ms |
| route->battle | measured | 75.0 ms (frame 1250) | 166 ms | 31 | 4.6 ms | 74.3 ms |
| battle->reward | measured | 5.9 ms (frame 38732) | 5638 ms | 2009 | 4.3 ms | 2.9 ms |
| quit | measured | 3.3 ms (frame 40285) | 68 ms | 31 | 2.5 ms | 1.6 ms |
| resume | resume | 60.3 ms (frame 181) | 2090 ms | 721 | 4.7 ms | 59.9 ms |
| reward->route | resume | 4.4 ms (frame 1025) | 2387 ms | 852 | 4.1 ms | 4.0 ms |

## Memory

VmRSS sampled every 0.5 s (244 samples): max 1248.0 MiB; kernel high-water mark (VmHWM) 1248.0 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.85 ms | 0.70 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.26 ms | 0.27 ms |
| GPU/SSAO | 0.20 ms | 0.20 ms |
| GPU/Unaccounted | 0.20 ms | 0.19 ms |
| GPU/Lights | 0.18 ms | 0.17 ms |
| GPU/Basepass | 0.13 ms | 0.13 ms |
| GPU/BuildRenderingCommandsDeferred | 0.08 ms | 0.09 ms |

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
