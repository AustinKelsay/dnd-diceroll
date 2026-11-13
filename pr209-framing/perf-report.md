# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit 34c4b506e25c97ada5f735ca213eb8ff174db70f; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.40 ms | 16.7 ms | pass |
| p95 GPUTime | 2.55 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.22 ms | 4.0 ms | pass |
| max RSS | 1234.9 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 25280 | measured pass, gate | 2.78 ms | 3.40 ms | 6.52 ms | 2.55 ms | 1.22 ms |
| 25218 | measured pass, fight only | 2.78 ms | 3.40 ms | 6.52 ms | 2.55 ms | 1.21 ms |
| 20 | resume pass | 2.79 ms | 2.81 ms | 2.87 ms | 2.49 ms | 0.85 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 22.6 ms (frame 183) | 23 ms | 1 | 2.0 ms | 22.6 ms |
| start->route | measured | 7.2 ms (frame 207) | 2267 ms | 995 | 4.1 ms | 7.9 ms |
| route->battle | measured | 56.8 ms (frame 1225) | 147 ms | 31 | 4.5 ms | 56.1 ms |
| battle->reward | measured | 5.3 ms (frame 26922) | 5508 ms | 1976 | 5.2 ms | 2.8 ms |
| quit | measured | 3.2 ms (frame 28477) | 66 ms | 31 | 2.5 ms | 1.6 ms |
| resume | resume | 56.6 ms (frame 181) | 2087 ms | 726 | 4.4 ms | 55.4 ms |
| reward->route | resume | 5.7 ms (frame 1045) | 2387 ms | 852 | 5.7 ms | 4.0 ms |

## Memory

VmRSS sampled every 0.5 s (177 samples): max 1234.9 MiB; kernel high-water mark (VmHWM) 1231.9 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.69 ms | 0.69 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.26 ms | 0.26 ms |
| GPU/SSAO | 0.21 ms | 0.20 ms |
| GPU/Unaccounted | 0.20 ms | 0.19 ms |
| GPU/Lights | 0.19 ms | 0.18 ms |
| GPU/Basepass | 0.14 ms | 0.13 ms |
| GPU/BuildRenderingCommandsDeferred | 0.08 ms | 0.08 ms |

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
