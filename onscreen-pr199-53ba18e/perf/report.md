# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit 53ba18ee1fd267d93c03ad2adb3731cb881b84b6; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.40 ms | 16.7 ms | pass |
| p95 GPUTime | 2.67 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.23 ms | 4.0 ms | pass |
| max RSS | 1247.2 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 22246 | measured pass, gate | 2.77 ms | 3.40 ms | 6.47 ms | 2.67 ms | 1.23 ms |
| 22184 | measured pass, fight only | 2.77 ms | 3.40 ms | 6.47 ms | 2.67 ms | 1.22 ms |
| 20 | resume pass | 2.77 ms | 3.41 ms | 3.62 ms | 2.64 ms | 0.83 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 22.6 ms (frame 183) | 23 ms | 1 | 2.0 ms | 22.5 ms |
| start->route | measured | 7.2 ms (frame 207) | 2268 ms | 1021 | 2.4 ms | 7.7 ms |
| route->battle | measured | 64.1 ms (frame 1251) | 158 ms | 31 | 4.4 ms | 63.5 ms |
| battle->reward | measured | 6.0 ms (frame 24981) | 4775 ms | 1701 | 4.9 ms | 2.9 ms |
| quit | measured | 3.4 ms (frame 25194) | 68 ms | 31 | 2.5 ms | 1.9 ms |
| resume | resume | 61.0 ms (frame 181) | 2085 ms | 709 | 5.0 ms | 60.6 ms |
| reward->route | resume | 4.4 ms (frame 1743) | 2386 ms | 861 | 2.7 ms | 4.2 ms |

## Memory

VmRSS sampled every 0.5 s (159 samples): max 1247.2 MiB; kernel high-water mark (VmHWM) 1243.7 MiB. The budget uses the larger.

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

Warm-up pass: yes. Idle time between passes (other jobs may have run): warmup to measured: 75 s, measured to resume: 80 s.

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
