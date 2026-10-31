---
name: asset-production
description: Track game-asset revisions and prepare Scenario or Blender production with stable Unreal paths. Use for Scenario setup, 3D production handoffs, or reimporting revised assets.
---

Read [asset production](../../../docs/agents/asset-production.md) before selecting
tools or generating candidates. It owns the approved-reference loop, optional
Scenario/Blender setup, generation limits and live pilot criteria. Follow the
existing contribution and editor ownership rules it links.

When creating a record or revising an existing asset, read
[asset manifests](../../../docs/agents/asset-manifests.md). Resolve the existing
asset ID and destination first. Preserve approved bytes and previous records;
validate both revisions with `tools/asset_manifest.py` before an intentional
Unreal reimport. A successful manifest check is file-integrity evidence and an
import plan, not visual validation or acceptance.

Use the contributor's available tools. Scenario is optional; its cloud workflows
require a connection and cost budget, while the Blender expert family operates
on installed local software. Inspect any optional skill before executing its
scripts. Keep this project's engine pin, lifecycle, content root and connection
settings authoritative when upstream defaults differ.

For family art direction, visual targets and raster image checks, use the
[create-game-assets skill](../create-game-assets/SKILL.md). For Unreal candidate
inspection and integration, follow [asset intake](../../../docs/agents/asset-intake.md).
