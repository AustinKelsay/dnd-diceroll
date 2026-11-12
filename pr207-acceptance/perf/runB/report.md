# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit f09abfe0fdae499db7a31511c43dc07229de8e11 (dirty); engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.38 ms | 16.7 ms | pass |
| p95 GPUTime | 2.67 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.20 ms | 4.0 ms | pass |
| max RSS | 1248.9 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 25216 | measured pass, gate | 2.77 ms | 3.38 ms | 5.97 ms | 2.67 ms | 1.20 ms |
| 25154 | measured pass, fight only | 2.77 ms | 3.38 ms | 5.97 ms | 2.67 ms | 1.20 ms |
| 20 | resume pass | 2.74 ms | 3.59 ms | 3.62 ms | 2.62 ms | 0.86 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 22.9 ms (frame 183) | 23 ms | 1 | 2.0 ms | 23.0 ms |
| start->route | measured | 6.0 ms (frame 207) | 2267 ms | 986 | 4.3 ms | 6.5 ms |
| route->battle | measured | 58.0 ms (frame 1216) | 149 ms | 31 | 4.6 ms | 57.3 ms |
| battle->reward | measured | 5.0 ms (frame 27322) | 5507 ms | 1964 | 4.5 ms | 2.9 ms |
| quit | measured | 3.4 ms (frame 28392) | 68 ms | 31 | 2.6 ms | 1.7 ms |
| resume | resume | 65.3 ms (frame 181) | 2086 ms | 716 | 4.5 ms | 65.0 ms |
| reward->route | resume | 5.2 ms (frame 1351) | 2387 ms | 840 | 4.5 ms | 4.4 ms |

## Memory

VmRSS sampled every 0.5 s (177 samples): max 1248.9 MiB; kernel high-water mark (VmHWM) 1248.9 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.84 ms | 0.70 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.27 ms | 0.27 ms |
| GPU/SSAO | 0.21 ms | 0.20 ms |
| GPU/Unaccounted | 0.20 ms | 0.19 ms |
| GPU/Lights | 0.18 ms | 0.17 ms |
| GPU/Basepass | 0.13 ms | 0.13 ms |
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
