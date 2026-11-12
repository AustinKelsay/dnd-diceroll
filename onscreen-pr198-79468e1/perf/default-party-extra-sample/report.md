# Performance gate: FAIL

Resolution 1920x1080; mode packaged Development; commit 79468e1fc1a33099eed2c88be570206a0a2b9f35; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.47 ms | 16.7 ms | pass |
| p95 GPUTime | 2.67 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.28 ms | 4.0 ms | pass |
| max RSS | 1244.8 MiB | 5120.0 MiB | pass |

Failures:

- No warm-up pass: the gate is measured on a warm package (run perf --pass warmup first)

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 22205 | measured pass, gate | 2.77 ms | 3.47 ms | 6.02 ms | 2.67 ms | 1.28 ms |
| 22143 | measured pass, fight only | 2.77 ms | 3.47 ms | 6.02 ms | 2.67 ms | 1.28 ms |
| 20 | resume pass | 2.76 ms | 2.98 ms | 3.68 ms | 2.61 ms | 0.81 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 22.7 ms (frame 183) | 23 ms | 1 | 1.9 ms | 22.6 ms |
| start->route | measured | 7.3 ms (frame 207) | 2265 ms | 1029 | 3.9 ms | 7.7 ms |
| route->battle | measured | 59.8 ms (frame 1259) | 151 ms | 31 | 4.4 ms | 59.0 ms |
| battle->reward | measured | 6.3 ms (frame 24070) | 4775 ms | 1699 | 4.6 ms | 2.9 ms |
| quit | measured | 8.3 ms (frame 25158) | 67 ms | 31 | 2.4 ms | 7.1 ms |
| resume | resume | 61.5 ms (frame 181) | 2086 ms | 711 | 5.0 ms | 61.3 ms |
| reward->route | resume | 6.2 ms (frame 1714) | 2386 ms | 830 | 5.8 ms | 6.1 ms |

## Memory

VmRSS sampled every 0.5 s (160 samples): max 1244.8 MiB; kernel high-water mark (VmHWM) 1240.1 MiB. The budget uses the larger.

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

Warm-up pass: NO (not a warm gate). Idle time between passes (other jobs may have run): measured to resume: 34 s.

## Passes

| Pass | Exit | Log | CSV |
| --- | --- | --- | --- |
| measured | 0 | measured/game.log | measured/measured.csv |
| resume | 0 | resume/game.log | resume/resume.csv |

## Host

- cpu: Intel(R) Core(TM) i9-14900K
- logical_cpus: 32
- memory_total_mb: 15736
- kernel: 7.2.15-1-omarchy
- gpu: NVIDIA GeForce RTX 4080 SUPER, 615.78.08, 16376 MiB
