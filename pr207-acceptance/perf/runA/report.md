# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit f09abfe0fdae499db7a31511c43dc07229de8e11 (dirty); engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.38 ms | 16.7 ms | pass |
| p95 GPUTime | 2.67 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.19 ms | 4.0 ms | pass |
| max RSS | 1252.0 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 25232 | measured pass, gate | 2.77 ms | 3.38 ms | 6.59 ms | 2.67 ms | 1.19 ms |
| 25170 | measured pass, fight only | 2.77 ms | 3.38 ms | 6.59 ms | 2.67 ms | 1.18 ms |
| 20 | resume pass | 2.74 ms | 2.99 ms | 3.05 ms | 2.62 ms | 0.87 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 22.8 ms (frame 183) | 23 ms | 1 | 2.1 ms | 22.8 ms |
| start->route | measured | 6.2 ms (frame 207) | 2274 ms | 1017 | 4.9 ms | 6.6 ms |
| route->battle | measured | 63.7 ms (frame 1247) | 160 ms | 31 | 4.8 ms | 62.9 ms |
| battle->reward | measured | 6.5 ms (frame 27861) | 5507 ms | 1961 | 6.1 ms | 2.9 ms |
| quit | measured | 4.2 ms (frame 28436) | 68 ms | 31 | 2.5 ms | 1.7 ms |
| resume | resume | 61.9 ms (frame 181) | 2084 ms | 708 | 5.5 ms | 61.0 ms |
| reward->route | resume | 7.2 ms (frame 1741) | 2387 ms | 860 | 2.7 ms | 7.0 ms |

## Memory

VmRSS sampled every 0.5 s (177 samples): max 1252.0 MiB; kernel high-water mark (VmHWM) 1247.6 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.85 ms | 0.70 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.27 ms | 0.27 ms |
| GPU/Unaccounted | 0.21 ms | 0.19 ms |
| GPU/SSAO | 0.20 ms | 0.20 ms |
| GPU/Lights | 0.18 ms | 0.17 ms |
| GPU/Basepass | 0.13 ms | 0.12 ms |
| GPU/BuildRenderingCommandsDeferred | 0.09 ms | 0.09 ms |

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
