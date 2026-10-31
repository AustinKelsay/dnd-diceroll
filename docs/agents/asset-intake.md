# Asset intake

Use this recipe when introducing or replacing art, audio or animation. Start from
the approved [editor task brief](../templates/editor-task.md), then follow
[development](../development.md) and [editor recipes](../editor-recipes.md) for actual
Unreal operations. [Asset provenance](../assets.md) is the durable import record.
Prepare generated or revised sources through [asset production](asset-production.md)
first; this recipe covers their Unreal inspection and integration.

## Prepare the candidate

Confirm the asset's purpose, visual direction and target scale against an accepted
reference in the game. Agree task-specific geometry, texture/material, audio or
performance limits where relevant; do not invent global budgets. Use existing
assets when suitable. Any generation service is optional.

Keep editable sources in `Art/` when introduced. Before import, record a provenance
entry in `docs/assets.md` with provider/author, source URL or generation recipe,
license/use terms, source and intended runtime paths, and modifications. For
generated sources include the tool/model or version when known, relevant settings
and retained source inputs; keep credentials out of the record. If usage rights
cannot be established, resolve them before adding the asset to the shared game.

Completion: the intended source, destination, compatibility requirements and
provenance are known. The issue claim covers any existing binary paths to edit.

## Import and inspect

Use Unreal APIs/MCP to create the binary assets. Import into the issue's agreed
candidate folder under `/Game/DnDDiceRoll`, leaving accepted assets available until
the candidate passes. A revision of an accepted asset that has a versioned record
instead reimports at its recorded package through asset production; its earlier
record and source bytes are the recovery point. Use a dedicated validation map/scene when useful and include
its path in the claim. Preserve existing recipe output on reruns.

Check the applicable properties below against the brief and accepted reference.
Record explicit import/compile results and investigate mismatches before replacement.

| Candidate | Inspect before integration |
| --- | --- |
| Mesh/prop | Scale and facing in-engine; origin/pivot and transforms; normals, UVs and materials; collision appropriate to the intended interaction. |
| Skeletal mesh/animation | Skeleton and retargeting compatibility; reference pose, facing and scale; motion/in-place or root-motion policy; joints, deformation, clipping and animation transitions in motion. A matching preset name alone is not proof. |
| Texture/material/icon | Dimensions, color-space settings appropriate to its use, alpha/transparency, material compilation, readability and appearance under the game's actual lighting/UI scale. |
| Audio | Format/import settings, level and looping, spatial/non-spatial behavior, and playback in the intended game context. |

Completion: applicable checks pass in Unreal, individual saves succeed, and the
assets reload with the expected references and settings. A source-tool preview
alone does not establish runtime suitability. Report incompatible output through
the [editor contribution recipe](editor-contributions.md) before changing scope.

## Integrate and hand off

Replace only the approved assets, preserving the brief's rules, input, interaction
and layout constraints. Check the actual game view and relevant interaction after
the replacement; inspect animation in motion and check camera/collision clearance
where applicable. For a performance claim, compare the same representative scene
on the same host/configuration before and after and report the measurement.

Update provenance with the final paths and transformations. Check references,
explicit saves, full dirty-package state and reload results. Use the existing Git
LFS and review process; resolve binary conflicts with the owners through Unreal.
Complete the [editor report](../templates/editor-report.md) with the changed
inventory, observed results and accessible selected evidence. Keep unfinished
candidate work identifiable rather than presenting it as an accepted replacement.
