# Unreal profiling and budgets

Project adaptation of the upstream profiling reference. Use the pinned engine, approved target and the project-owned launch/build workflow.

## Choose a measurement

| Question | Starting evidence | Next investigation |
| --- | --- | --- |
| Which part of the frame dominates? | `stat unit`, with frame caps/vsync and settings recorded | Unreal Insights CPU timelines or GPU pass timing |
| Why does an action hitch? | Repeat the same seed/actions and capture the spike | Game/render thread work, loading, allocations, synchronization |
| Why is a dense view slow? | Rendered frame timing at the same camera/resolution | Draw submission, passes, materials, shadows, transparency and mesh instances |
| Why does memory grow? | Repeatable session duration and memory observations | Insights memory/allocation data when enabled; asset/reference lifetimes |

GPU timing/capture availability depends on the platform and renderer. On Apple Silicon, verify the installed Metal/Xcode tool support. RenderDoc is not the Metal capture path. A missing GPU timing metric is a limitation to report, not a zero-cost measurement.

Frame time is constrained by interacting CPU/GPU work, waits and display limits. Fewer draw calls can reduce CPU submission overhead as well as change GPU work. Confirm the expensive thread or pass rather than selecting an optimization from a single count.

## Compare fairly

Prefer a rendered Mac Development package for target observations, with an explicit warmup, capture duration and repeated trials. Record editor-only measurements as editor-only. Use identical seed/action sequence, camera, resolution, scalability, frame cap/vsync and tracing settings before and after. Separate loading/shader warmup from steady-state behavior. The package helper's null-renderer startup check is not a graphics benchmark.

Inspect the dominant interval in Unreal Insights, then make the smallest relevant change. For repeated pieces or props, shared materials or instanced meshes may reduce submission cost; measure them before adding complexity. A pool requires demonstrated lifetime/spawn cost and a bounded ownership policy. Cached rules queries need correct invalidation and must preserve authoritative command behavior.

## Measurement record

Record in the issue/PR:

- Commit/source state, pinned engine, machine/GPU, build configuration.
- Scene/map, seed, actions, camera and rendering settings.
- Warmup, sampling duration/trials and enabled profiler overhead.
- Before/after frame time or memory, representative spikes/tail measurements and variability.
- The measured cost, change made and behavior/appearance checks.
- Accessible selected trace/report evidence and limitations.

Keep bulk traces and reports in ignored `.local/`, `Saved/` or `Builds/`. Budget values belong to the approved target; generic triangle, texture or draw-call counts are not project acceptance criteria.

## Primary documentation

- [Epic: Unreal Insights](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-insights-in-unreal-engine)

Read the installed engine's trace/tool help when selecting channels or console commands; do not invent an unsupported metric or silently change the renderer to obtain one.
