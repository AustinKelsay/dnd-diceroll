# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit b2d747581bcd10f02b3cf1593028a0722683b9d1; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.35 ms | 16.7 ms | pass |
| p95 GPUTime | 2.55 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.17 ms | 4.0 ms | pass |
| max RSS | 1261.3 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 25246 | measured pass, gate | 2.79 ms | 3.35 ms | 7.78 ms | 2.55 ms | 1.17 ms |
| 25184 | measured pass, fight only | 2.79 ms | 3.35 ms | 7.78 ms | 2.56 ms | 1.17 ms |
| 20 | resume pass | 2.75 ms | 3.55 ms | 3.67 ms | 2.49 ms | 1.04 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 22.7 ms (frame 183) | 23 ms | 1 | 2.0 ms | 22.7 ms |
| start->route | measured | 9.3 ms (frame 207) | 2268 ms | 1020 | 3.6 ms | 9.5 ms |
| route->battle | measured | 61.8 ms (frame 1250) | 151 ms | 31 | 4.7 ms | 61.0 ms |
| battle->reward | measured | 5.8 ms (frame 26973) | 5506 ms | 1965 | 5.2 ms | 2.9 ms |
| quit | measured | 2.8 ms (frame 28456) | 67 ms | 31 | 2.5 ms | 2.0 ms |
| resume | resume | 55.2 ms (frame 181) | 2085 ms | 722 | 5.2 ms | 54.8 ms |
| reward->route | resume | 5.9 ms (frame 1122) | 2386 ms | 851 | 5.0 ms | 3.9 ms |

## Memory

VmRSS sampled every 0.5 s (177 samples): max 1261.3 MiB; kernel high-water mark (VmHWM) 1256.6 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.70 ms | 0.69 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.26 ms | 0.26 ms |
| GPU/SSAO | 0.20 ms | 0.20 ms |
| GPU/Unaccounted | 0.20 ms | 0.19 ms |
| GPU/Lights | 0.19 ms | 0.19 ms |
| GPU/Basepass | 0.13 ms | 0.13 ms |
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
