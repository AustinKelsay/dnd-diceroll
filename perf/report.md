# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit 9dee48de38773bd8f6d814b0e996cb6933eb6c1d; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.43 ms | 16.7 ms | pass |
| p95 GPUTime | 2.69 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.26 ms | 4.0 ms | pass |
| max RSS | 1261.0 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 25193 | measured pass, gate | 2.77 ms | 3.43 ms | 7.06 ms | 2.69 ms | 1.26 ms |
| 25131 | measured pass, fight only | 2.77 ms | 3.43 ms | 7.06 ms | 2.69 ms | 1.25 ms |
| 20 | resume pass | 2.74 ms | 3.18 ms | 3.36 ms | 2.63 ms | 0.84 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 22.7 ms (frame 183) | 23 ms | 1 | 2.0 ms | 22.7 ms |
| start->route | measured | 6.3 ms (frame 207) | 2268 ms | 1025 | 2.4 ms | 6.8 ms |
| route->battle | measured | 58.8 ms (frame 1255) | 148 ms | 31 | 4.5 ms | 57.6 ms |
| battle->reward | measured | 5.3 ms (frame 26460) | 5512 ms | 1952 | 5.0 ms | 3.1 ms |
| quit | measured | 3.7 ms (frame 28396) | 68 ms | 31 | 2.6 ms | 1.6 ms |
| resume | resume | 68.2 ms (frame 181) | 2085 ms | 699 | 4.9 ms | 67.9 ms |
| reward->route | resume | 4.6 ms (frame 1678) | 2387 ms | 863 | 4.1 ms | 4.3 ms |

## Memory

VmRSS sampled every 0.5 s (177 samples): max 1261.0 MiB; kernel high-water mark (VmHWM) 1256.9 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.84 ms | 0.69 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.27 ms | 0.27 ms |
| GPU/Unaccounted | 0.21 ms | 0.19 ms |
| GPU/SSAO | 0.21 ms | 0.20 ms |
| GPU/Lights | 0.18 ms | 0.17 ms |
| GPU/Basepass | 0.13 ms | 0.12 ms |
| GPU/BuildRenderingCommandsDeferred | 0.09 ms | 0.09 ms |

## Run

Warm-up pass: yes. Idle time between passes (other jobs may have run): warmup to measured: 27 s, measured to resume: 156 s.

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
