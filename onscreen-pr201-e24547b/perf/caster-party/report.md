# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit e24547b3675f3d5c91f9cc862f3facf615fdabfa; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.38 ms | 16.7 ms | pass |
| p95 GPUTime | 2.68 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.20 ms | 4.0 ms | pass |
| max RSS | 1264.7 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 27312 | measured pass, gate | 2.76 ms | 3.38 ms | 6.18 ms | 2.68 ms | 1.20 ms |
| 27250 | measured pass, fight only | 2.76 ms | 3.38 ms | 6.18 ms | 2.68 ms | 1.20 ms |
| 20 | resume pass | 2.99 ms | 3.58 ms | 4.04 ms | 3.17 ms | 0.89 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 27.0 ms (frame 183) | 27 ms | 1 | 2.7 ms | 27.0 ms |
| start->route | measured | 4.9 ms (frame 207) | 2267 ms | 1014 | 3.2 ms | 5.5 ms |
| route->battle | measured | 63.2 ms (frame 1244) | 152 ms | 31 | 4.5 ms | 62.1 ms |
| battle->reward | measured | 5.8 ms (frame 28789) | 5505 ms | 1964 | 6.1 ms | 2.9 ms |
| quit | measured | 3.4 ms (frame 30516) | 69 ms | 31 | 2.6 ms | 1.6 ms |
| resume | resume | 60.8 ms (frame 181) | 2092 ms | 723 | 4.8 ms | 60.4 ms |
| reward->route | resume | 4.2 ms (frame 982) | 2386 ms | 849 | 3.7 ms | 4.0 ms |

## Memory

VmRSS sampled every 0.5 s (189 samples): max 1264.7 MiB; kernel high-water mark (VmHWM) 1259.6 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.84 ms | 0.69 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.27 ms | 0.27 ms |
| GPU/SSAO | 0.21 ms | 0.20 ms |
| GPU/Unaccounted | 0.21 ms | 0.19 ms |
| GPU/Lights | 0.18 ms | 0.17 ms |
| GPU/Basepass | 0.12 ms | 0.12 ms |
| GPU/BuildRenderingCommandsDeferred | 0.10 ms | 0.09 ms |

## Run

Warm-up pass: yes. Idle time between passes (other jobs may have run): warmup to measured: 3 s, measured to resume: 6 s.

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
