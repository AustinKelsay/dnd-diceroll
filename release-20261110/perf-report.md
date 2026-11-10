# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit 0f3042ebc1e7622b9f8559e9dfdd988667e486ca; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.23 ms | 16.7 ms | pass |
| p95 GPUTime | 2.71 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.19 ms | 4.0 ms | pass |
| max RSS | 1236.9 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 22177 | measured pass, gate | 2.75 ms | 3.23 ms | 6.04 ms | 2.71 ms | 1.19 ms |
| 22115 | measured pass, fight only | 2.75 ms | 3.23 ms | 6.04 ms | 2.71 ms | 1.18 ms |
| 20 | resume pass | 2.75 ms | 3.10 ms | 3.53 ms | 2.70 ms | 0.85 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 22.9 ms (frame 183) | 23 ms | 1 | 2.1 ms | 22.9 ms |
| start->route | measured | 7.3 ms (frame 207) | 2266 ms | 1052 | 4.5 ms | 7.7 ms |
| route->battle | measured | 59.5 ms (frame 1282) | 151 ms | 31 | 4.3 ms | 58.8 ms |
| battle->reward | measured | 4.8 ms (frame 24478) | 4775 ms | 1708 | 4.8 ms | 3.2 ms |
| quit | measured | 3.3 ms (frame 25162) | 66 ms | 31 | 2.6 ms | 2.5 ms |
| resume | resume | 56.9 ms (frame 181) | 2085 ms | 710 | 4.7 ms | 56.7 ms |
| reward->route | resume | 5.8 ms (frame 1190) | 2384 ms | 842 | 5.2 ms | 3.9 ms |

## Memory

VmRSS sampled every 0.5 s (159 samples): max 1236.9 MiB; kernel high-water mark (VmHWM) 1234.5 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.87 ms | 0.64 ms |
| GPU/ShadowDepths | 0.50 ms | 0.35 ms |
| GPU/Postprocessing | 0.40 ms | 0.28 ms |
| GPU/Basepass | 0.26 ms | 0.12 ms |
| GPU/SSAO | 0.21 ms | 0.21 ms |
| GPU/Unaccounted | 0.20 ms | 0.19 ms |
| GPU/Lights | 0.19 ms | 0.18 ms |
| GPU/BuildRenderingCommandsDeferred | 0.09 ms | 0.09 ms |

## Run

Warm-up pass: yes. Idle time between passes (other jobs may have run): warmup to measured: 6 s, measured to resume: 6 s.

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
