# Performance gate: PASS

Resolution 1920x1080; mode packaged Development; commit 79468e1fc1a33099eed2c88be570206a0a2b9f35; engine 5.8.4.

## Budgets

| Budget | Measured | Limit | Result |
| --- | --- | --- | --- |
| p95 FrameTime | 3.38 ms | 16.7 ms | pass |
| p95 GPUTime | 2.67 ms | 12.0 ms | pass |
| p95 GameThreadTime | 1.19 ms | 4.0 ms | pass |
| max RSS | 1231.7 MiB | 5120.0 MiB | pass |

## Steady state (transition windows excluded)

| Frames | Set | FrameTime p50 | p95 | max | GPUTime p95 | GameThreadTime p95 |
| --- | --- | --- | --- | --- | --- | --- |
| 27106 | measured pass, gate | 2.77 ms | 3.38 ms | 11.18 ms | 2.67 ms | 1.19 ms |
| 27044 | measured pass, fight only | 2.77 ms | 3.38 ms | 11.18 ms | 2.67 ms | 1.19 ms |
| 20 | resume pass | 2.76 ms | 2.96 ms | 3.53 ms | 2.62 ms | 1.27 ms |

## Transition hitches (reported separately; not in the steady state)

| Transition | Pass | Worst frame | Duration | Frames | Worst GPU | Worst game thread |
| --- | --- | --- | --- | --- | --- | --- |
| plan | measured | 37.6 ms (frame 183) | 38 ms | 1 | 1.9 ms | 37.7 ms |
| start->route | measured | 4.1 ms (frame 412) | 2266 ms | 1034 | 3.2 ms | 3.6 ms |
| route->battle | measured | 62.4 ms (frame 1264) | 153 ms | 31 | 4.4 ms | 61.7 ms |
| battle->reward | measured | 6.2 ms (frame 28747) | 4773 ms | 1703 | 5.2 ms | 2.8 ms |
| quit | measured | 3.6 ms (frame 30069) | 67 ms | 31 | 2.6 ms | 2.4 ms |
| resume | resume | 61.4 ms (frame 181) | 2087 ms | 721 | 4.7 ms | 60.5 ms |
| reward->route | resume | 6.1 ms (frame 1203) | 2386 ms | 843 | 5.3 ms | 3.8 ms |

## Memory

VmRSS sampled every 0.5 s (186 samples): max 1231.7 MiB; kernel high-water mark (VmHWM) 1226.1 MiB. The budget uses the larger.

## Most expensive GPU passes (steady state, -csvGpuStats)

| Pass | p95 | mean |
| --- | --- | --- |
| GPU/TemporalSuperResolution | 0.84 ms | 0.69 ms |
| GPU/ShadowDepths | 0.33 ms | 0.33 ms |
| GPU/Postprocessing | 0.27 ms | 0.27 ms |
| GPU/SSAO | 0.21 ms | 0.20 ms |
| GPU/Unaccounted | 0.20 ms | 0.19 ms |
| GPU/Lights | 0.18 ms | 0.17 ms |
| GPU/Basepass | 0.12 ms | 0.12 ms |
| GPU/BuildRenderingCommandsDeferred | 0.10 ms | 0.09 ms |

## Run

Warm-up pass: yes. Idle time between passes (other jobs may have run): warmup to measured: 14 s, measured to resume: 1 s.

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
