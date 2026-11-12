# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit 5bb04f9a197602c997b0348a80e52b7ffac41f1b; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.37 ms | 16.7 ms | pass |
| p95 GPUTime | 2.66 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.21 ms | 4.0 ms | pass |
| max RSS | 1229.6 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 24732 | measured pass, gate | 2.77 ms | 3.37 ms | 7.67 ms | 2.66 ms | 1.21 ms |
| 24670 | measured pass, fight only | 2.77 ms | 3.37 ms | 7.67 ms | 2.67 ms | 1.21 ms |
| 20 | resume pass | 2.77 ms | 2.96 ms | 2.97 ms | 2.63 ms | 0.81 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 23.2 ms (frame 183) | 23 ms | 1 | 2.1 ms | 23.7 ms |
| start->route | measured | 8.0 ms (frame 207) | 2267 ms | 1000 | 5.0 ms | 8.2 ms |
| route->battle | measured | 66.3 ms (frame 1230) | 158 ms | 31 | 4.6 ms | 65.5 ms |
| battle->reward | measured | 5.8 ms (frame 26757) | 4775 ms | 1705 | 4.2 ms | 3.8 ms |
| quit | measured | 4.0 ms (frame 27663) | 68 ms | 31 | 2.5 ms | 2.3 ms |
| resume | resume | 65.4 ms (frame 181) | 2086 ms | 720 | 4.6 ms | 64.9 ms |
| reward->route | resume | 5.9 ms (frame 1741) | 2387 ms | 848 | 5.3 ms | 5.8 ms |

## Memory

VmRSS sampled every 0.5 s (173 samples): max 1229.6 MiB; kernel high-water mark (VmHWM) 1229.6 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.84 ms | 0.69 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.26 ms | 0.27 ms |
| GPU/SSAO | 0.21 ms | 0.20 ms |
| GPU/Unaccounted | 0.20 ms | 0.19 ms |
| GPU/Lights | 0.18 ms | 0.17 ms |
| GPU/Basepass | 0.12 ms | 0.12 ms |
| GPU/BuildRenderingCommandsDeferred | 0.10 ms | 0.09 ms |

## Run

Warm-up pass: yes. Idle time between passes (other jobs may have run): warmup to measured: 3 s, measured to resume: 3 s.

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
