# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit 79468e1fc1a33099eed2c88be570206a0a2b9f35; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.44 ms | 16.7 ms | pass |
| p95 GPUTime | 2.67 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.29 ms | 4.0 ms | pass |
| max RSS | 1240.8 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 22165 | measured pass, gate | 2.77 ms | 3.44 ms | 7.46 ms | 2.67 ms | 1.29 ms |
| 22103 | measured pass, fight only | 2.77 ms | 3.44 ms | 7.46 ms | 2.67 ms | 1.28 ms |
| 20 | resume pass | 2.78 ms | 2.93 ms | 3.02 ms | 2.62 ms | 0.84 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 22.9 ms (frame 183) | 23 ms | 1 | 1.9 ms | 22.9 ms |
| start->route | measured | 8.3 ms (frame 207) | 2265 ms | 1018 | 5.0 ms | 8.8 ms |
| route->battle | measured | 63.4 ms (frame 1248) | 154 ms | 31 | 4.7 ms | 62.8 ms |
| battle->reward | measured | 5.3 ms (frame 23420) | 4774 ms | 1702 | 5.7 ms | 2.9 ms |
| quit | measured | 5.5 ms (frame 25110) | 67 ms | 31 | 2.6 ms | 4.6 ms |
| resume | resume | 67.1 ms (frame 181) | 2085 ms | 715 | 5.0 ms | 66.8 ms |
| reward->route | resume | 6.1 ms (frame 1743) | 2392 ms | 855 | 4.4 ms | 5.8 ms |

## Memory

VmRSS sampled every 0.5 s (159 samples): max 1240.8 MiB; kernel high-water mark (VmHWM) 1235.6 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.85 ms | 0.70 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.27 ms | 0.27 ms |
| GPU/Unaccounted | 0.21 ms | 0.20 ms |
| GPU/SSAO | 0.21 ms | 0.20 ms |
| GPU/Lights | 0.18 ms | 0.17 ms |
| GPU/Basepass | 0.12 ms | 0.12 ms |
| GPU/BuildRenderingCommandsDeferred | 0.10 ms | 0.09 ms |

## Run

Warm-up pass: yes. Idle time between passes (other jobs may have run): warmup to measured: 75 s, measured to resume: 13 s.

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
