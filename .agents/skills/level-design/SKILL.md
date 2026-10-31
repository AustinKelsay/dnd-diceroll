---
name: level-design
description: Plan and playtest Unreal level or tactical encounter blockouts, sightlines, reachability, pacing, and player guidance. Use when authoring layouts or reviewing how an encounter plays before dressing it.
license: Apache-2.0
---

# Level and encounter design

Adapted from the pinned upstream skill; source and file hashes are in [UPSTREAM.json](UPSTREAM.json).

## Project contract

Follow the [contribution workflow](../../../docs/agents/workflow.md) for scope, dependencies and asset claims. Read [development](../../../docs/development.md) before editor work and [architecture](../../../docs/architecture.md) before changing implementation. Read the [game brief](../../../docs/game-brief.md) and any available domain docs, ADRs, rules reference and accepted feature spec before selecting mechanics or pacing. Preserve the starter sandbox fixture; use approved feature asset paths under `/Game/DnDDiceRoll`.

## Workflow

1. Record the encounter's objective, available party/abilities, terrain, camera constraints and observable success/failure criteria. For a game-neutral starter, resolve missing gameplay requirements before inventing a level around them.
2. Derive metrics from implemented rules. For Expedition, use grid cell size, movement budgets, attack/ability ranges, occupancy and the actual targeting/visibility rules. Include surviving smaller parties when relevant. Jump heights, cover bonuses and new traversal mechanics apply only when the accepted rules support them.
3. Block out with simple Unreal primitives at the correct scale. Mark spawns, obstacles and objective cells explicitly. For generated encounters, change the authoritative recipe/data and its validation; the scene is a presentation of that data. Hand-placing a convincing scene does not satisfy a generator requirement.
4. Validate nonoverlapping legal spawns, movement connectivity, access to actionable targets and the objective under the implemented rules. Test occupancy and reachability together. Bound generation attempts and exercise multiple reproducible seeds when changing generation; keep authored layout work separate from introducing a new generator.
5. Make decisions readable through silhouette, landmarks, contrast and camera framing. Preview reachability and targets consistently with the command interface. Confirm decorative geometry does not suggest walkable cells or targeting options the rules reject.
6. Tune tension and teaching within the approved run structure. Expedition's encounter count, route flow and bounded recovery remain governed by its spec; a generic pacing pattern does not authorize extra rests, saves, rewards or mechanics.
7. Playtest the blockout before dressing it. Observe where players misread the space, get trapped or lose the objective. Inspect the overhead view at supported rotations, edges and HUD sizes. Iterate on geometry/data first, then add art that preserves the validated footprint.
8. Save/reload changed assets through Unreal and report layout/recipe paths, validation cases, seeds, playtest findings and inspected captures. Separate automated reachability evidence from observed readability and interaction.

## Reference

Read [pacing and blockout review](references/pacing-and-flow.md) when designing the encounter rhythm or reviewing player guidance. Game rules and accepted scope determine which techniques apply.
