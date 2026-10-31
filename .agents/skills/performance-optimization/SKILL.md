---
name: performance-optimization
description: Diagnose measured Unreal frame-time, hitching, memory, or rendering costs and verify an optimization on the same workload. Use for reproducible performance problems or an agreed performance budget.
license: Apache-2.0
---

# Performance optimization

Adapted from the pinned upstream skill; source and file hashes are in [UPSTREAM.json](UPSTREAM.json).

## Project contract

Follow the [contribution workflow](../../../docs/agents/workflow.md) for scope and evidence. Read [development](../../../docs/development.md) before operating the editor, compiling or packaging, and [architecture](../../../docs/architecture.md) before changing ownership or interfaces. Profile the pinned engine and approved target; this skill does not add platforms or choose a new performance target. For gameplay changes, read the game brief and available domain/spec docs first.

## Workflow

1. Agree on the symptom and target in frame time, hitch duration or memory. Record hardware, engine/build, resolution, quality settings and a repeatable scene/seed/action sequence. Use comparable warmup and sample duration; distinguish first-use loading/shader work from steady play.
2. Measure before changing code. Use Unreal's `stat unit` to distinguish frame/game/render-thread/GPU time, then Unreal Insights for CPU timelines, loading or allocation investigation. For rendering, use supported GPU timing/capture tools; on this Mac verify Metal tool support rather than assuming RenderDoc works.
3. Profile a rendered Mac Development package when practical; record editor-only evidence as such. The helper's headless `-nullrhi` package startup is a startup check and cannot establish rendering performance. Follow the existing package/launch ownership procedure.
4. Identify the dominant cost in a trace or capture. CPU and GPU interact: draw submission, synchronization, present limits or frame caps may obscure the bottleneck. Investigate the measured thread/pass rather than treating draw calls as exclusively GPU work.
5. Make one bounded change to that cost. Consider doing less work, invalidating cached queries correctly, sharing materials or instancing repeated meshes. Add pools, LODs or new abstractions only when measurements justify them. Preserve deterministic commands/RNG, legal targets and save behavior; presentation caching cannot become authoritative game state.
6. Repeat the same workload and settings on the same hardware. Report before/after frame time or memory and relevant tail/spike observations, including sampling limits. Preserve another worker's changes when removing an unsuccessful optimization.
7. Add only the regression check or budget justified by this slice. For rules changes, use the public behavior seam and appropriate engine automation. For rendering changes, inspect appearance and interaction as well as timing. Report evidence paths and remaining limits; do not claim an unmeasured speedup.

## Reference

Read [Unreal profiling and budgets](references/profiling-and-budgets.md) for tool selection and a measurement record. Budget examples require agreement and measurement on the actual target before adoption.
