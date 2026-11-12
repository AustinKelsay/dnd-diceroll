# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit f09abfe0fdae499db7a31511c43dc07229de8e11 (dirty); engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.36 ms | 16.7 ms | pass |
| p95 GPUTime | 2.69 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.20 ms | 4.0 ms | pass |
| max RSS | 1243.2 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 25219 | measured pass, gate | 2.77 ms | 3.36 ms | 6.48 ms | 2.69 ms | 1.20 ms |
| 25157 | measured pass, fight only | 2.77 ms | 3.36 ms | 6.48 ms | 2.69 ms | 1.20 ms |
| 20 | resume pass | 2.75 ms | 3.03 ms | 3.52 ms | 2.63 ms | 0.85 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 22.7 ms (frame 183) | 23 ms | 1 | 2.2 ms | 22.7 ms |
| start->route | measured | 6.6 ms (frame 207) | 2268 ms | 1018 | 3.7 ms | 7.1 ms |
| route->battle | measured | 61.5 ms (frame 1248) | 151 ms | 31 | 4.5 ms | 60.7 ms |
| battle->reward | measured | 5.8 ms (frame 28401) | 5513 ms | 1967 | 5.7 ms | 2.9 ms |
| quit | measured | 4.1 ms (frame 28430) | 72 ms | 31 | 2.6 ms | 1.8 ms |
| resume | resume | 59.7 ms (frame 181) | 2090 ms | 707 | 5.7 ms | 59.3 ms |
| reward->route | resume | 4.6 ms (frame 1190) | 2387 ms | 857 | 4.3 ms | 4.1 ms |

## Memory

VmRSS sampled every 0.5 s (177 samples): max 1243.2 MiB; kernel high-water mark (VmHWM) 1239.5 MiB. The budget uses the larger.

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
