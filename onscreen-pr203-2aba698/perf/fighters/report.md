# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit 2aba698641b2b8406af5913fce95e7c30478bd58; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.35 ms | 16.7 ms | pass |
| p95 GPUTime | 2.69 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.19 ms | 4.0 ms | pass |
| max RSS | 1249.3 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 36327 | measured pass, gate | 2.77 ms | 3.35 ms | 7.10 ms | 2.69 ms | 1.19 ms |
| 36265 | measured pass, fight only | 2.77 ms | 3.35 ms | 7.10 ms | 2.69 ms | 1.19 ms |
| 20 | resume pass | 2.77 ms | 3.04 ms | 3.08 ms | 2.63 ms | 0.82 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 32.0 ms (frame 183) | 32 ms | 1 | 2.0 ms | 31.9 ms |
| start->route | measured | 4.0 ms (frame 207) | 2267 ms | 1019 | 2.2 ms | 4.5 ms |
| route->battle | measured | 55.0 ms (frame 1249) | 145 ms | 31 | 4.2 ms | 54.2 ms |
| battle->reward | measured | 4.8 ms (frame 38784) | 4635 ms | 1654 | 4.5 ms | 2.9 ms |
| quit | measured | 3.4 ms (frame 39226) | 68 ms | 31 | 2.5 ms | 1.6 ms |
| resume | resume | 60.2 ms (frame 181) | 2085 ms | 701 | 4.4 ms | 59.9 ms |
| reward->route | resume | 4.1 ms (frame 986) | 2388 ms | 860 | 2.7 ms | 3.9 ms |

## Memory

VmRSS sampled every 0.5 s (238 samples): max 1249.3 MiB; kernel high-water mark (VmHWM) 1249.3 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.85 ms | 0.70 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.27 ms | 0.27 ms |
| GPU/SSAO | 0.20 ms | 0.20 ms |
| GPU/Unaccounted | 0.20 ms | 0.19 ms |
| GPU/Lights | 0.18 ms | 0.17 ms |
| GPU/Basepass | 0.13 ms | 0.12 ms |
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
