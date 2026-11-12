# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit 2c3ac779606646afdfd93fa7a724178ecd7a2c36; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.43 ms | 16.7 ms | pass |
| p95 GPUTime | 2.69 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.40 ms | 4.0 ms | pass |
| max RSS | 1244.8 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 23900 | measured pass, gate | 2.77 ms | 3.43 ms | 7.35 ms | 2.69 ms | 1.40 ms |
| 23838 | measured pass, fight only | 2.77 ms | 3.43 ms | 7.35 ms | 2.69 ms | 1.40 ms |
| 20 | resume pass | 2.75 ms | 3.13 ms | 3.48 ms | 2.60 ms | 0.79 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 18.8 ms (frame 183) | 19 ms | 1 | 2.0 ms | 18.9 ms |
| start->route | measured | 4.3 ms (frame 207) | 2267 ms | 1021 | 2.2 ms | 5.1 ms |
| route->battle | measured | 66.6 ms (frame 1251) | 156 ms | 31 | 4.1 ms | 65.1 ms |
| battle->reward | measured | 5.8 ms (frame 26223) | 5640 ms | 2004 | 4.6 ms | 3.4 ms |
| quit | measured | 3.5 ms (frame 27151) | 68 ms | 31 | 2.6 ms | 2.0 ms |
| resume | resume | 60.4 ms (frame 181) | 2084 ms | 725 | 4.3 ms | 59.9 ms |
| reward->route | resume | 5.2 ms (frame 1637) | 2390 ms | 855 | 5.0 ms | 3.9 ms |

## Memory

VmRSS sampled every 0.5 s (171 samples): max 1244.8 MiB; kernel high-water mark (VmHWM) 1244.8 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.85 ms | 0.70 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.27 ms | 0.27 ms |
| GPU/Unaccounted | 0.22 ms | 0.20 ms |
| GPU/SSAO | 0.21 ms | 0.20 ms |
| GPU/Lights | 0.18 ms | 0.17 ms |
| GPU/Basepass | 0.13 ms | 0.12 ms |
| GPU/BuildRenderingCommandsDeferred | 0.09 ms | 0.09 ms |

## Run

Warm-up pass: yes. Idle time between passes (other jobs may have run): warmup to measured: 3 s, measured to resume: 146 s.

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
