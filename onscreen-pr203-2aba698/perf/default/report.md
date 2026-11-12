# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit 2aba698641b2b8406af5913fce95e7c30478bd58; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.36 ms | 16.7 ms | pass |
| p95 GPUTime | 2.69 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.20 ms | 4.0 ms | pass |
| max RSS | 1247.4 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 25214 | measured pass, gate | 2.77 ms | 3.36 ms | 5.85 ms | 2.69 ms | 1.20 ms |
| 25152 | measured pass, fight only | 2.77 ms | 3.36 ms | 5.85 ms | 2.69 ms | 1.20 ms |
| 20 | resume pass | 2.77 ms | 2.94 ms | 3.10 ms | 2.62 ms | 0.84 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 23.5 ms (frame 183) | 23 ms | 1 | 2.1 ms | 23.3 ms |
| start->route | measured | 7.0 ms (frame 207) | 2267 ms | 1026 | 2.2 ms | 7.4 ms |
| route->battle | measured | 60.1 ms (frame 1256) | 161 ms | 31 | 5.0 ms | 59.4 ms |
| battle->reward | measured | 5.6 ms (frame 27900) | 5507 ms | 1965 | 4.9 ms | 2.9 ms |
| quit | measured | 3.8 ms (frame 28431) | 68 ms | 31 | 2.6 ms | 1.6 ms |
| resume | resume | 74.6 ms (frame 181) | 2085 ms | 704 | 5.3 ms | 74.0 ms |
| reward->route | resume | 4.0 ms (frame 932) | 2388 ms | 861 | 2.9 ms | 3.9 ms |

## Memory

VmRSS sampled every 0.5 s (177 samples): max 1247.4 MiB; kernel high-water mark (VmHWM) 1241.6 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.85 ms | 0.69 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.27 ms | 0.27 ms |
| GPU/Unaccounted | 0.21 ms | 0.19 ms |
| GPU/SSAO | 0.21 ms | 0.20 ms |
| GPU/Lights | 0.18 ms | 0.17 ms |
| GPU/Basepass | 0.13 ms | 0.12 ms |
| GPU/BuildRenderingCommandsDeferred | 0.09 ms | 0.09 ms |

## Run

Warm-up pass: yes. Idle time between passes (other jobs may have run): warmup to measured: 27 s, measured to resume: 1 s.

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
