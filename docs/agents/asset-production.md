# Asset production and revision

Use this recipe to generate or revise a game asset. Start with the approved issue
and [contribution workflow](workflow.md); [development](../development.md) owns
Unreal lifecycle, exact identity and editor ownership. This recipe works with an
agent and local tools; a generation service is optional.

## Establish the asset

Resolve the existing asset ID, records and Unreal destination before creating
anything. For a new asset, choose an unused ID and destination with the issue
owner. Claim existing binary paths before edits. Capture the desired player-facing
change, preserved behavior, target scale/material style, acceptance checks and
reviewer in the issue. Reuse approved decisions; ask only for unresolved intent
outside existing authority.

Keep a small approved reference set with the editable source. Describe palette,
silhouette, proportions and materials, plus what changes from that baseline.
Generate one trial before a batch. Record the selected model/version where known,
parameters, reference hashes, job ID when available, quote and actual reported
cost in the recipe. An unknown model revision stays explicitly unknown.

Before paid work, check that the later stages are available, quote the actual
payload, and establish a spending/attempt bound within the user's grant. At the
bound or repeated failure, report the best candidate and remaining mismatch;
reassess rather than silently extending the experiment. A seed is useful metadata;
preserved source/output bytes are the recovery point.

Completion: the issue identifies one asset, an approved reference/brief, permitted
tools and bounds, and observable checks. It is clear whether this is new content
or a revision of accepted content.

## Produce and inspect a candidate

Preserve the original generation and work in a new revision directory under `Art/`.
In Blender or another suitable tool, inspect scale, facing, pivots, topology,
normals/UVs, materials and collision requirements. Bake/retopologize only when the
asset needs it. For characters also inspect skeleton compatibility, deformation,
root-motion policy and transitions in motion. Retain an editable source and the
export settings/tool versions. Export, re-open the export and compare it with the
source; record losses or unsupported features. Check the actual installed tool
version before using an upstream script.

Use [asset manifests](asset-manifests.md) to record/hash the reference, source and
export. Check the record and plan against the prior record before import. A
`metadata_only` plan means no export/import-policy change was detected; it does
not prove the live Unreal asset is current. If the prior import is unverified,
inspect it before deciding whether to import.

Through the verified editor owner, import or reimport at the recorded destination
under `/Game/DnDDiceRoll`. Preserve the prior accepted content until the candidate
passes. Inspect returned import/compile/save results, full dirty-package state,
saved/reloaded references, and the asset in the actual game view. Exercise relevant
interaction and camera/collision clearance. Image scoring cannot verify meshes,
rigs, collisions or game feel. Record provenance in [assets](../assets.md), plus
accessible selected evidence on the issue/PR. Follow the existing independent
review and staging integration gates before treating a revision as accepted.

Completion: source/export integrity and applicable runtime checks are recorded;
unobserved behavior remains pending. No provider output is accepted solely because
it is labelled game-ready.

## Optional Scenario setup

Reviewed upstream snapshot: [scenario-labs/skills at 7f4b801c7248d51b093ec365aceb114bd06139e6](https://github.com/scenario-labs/skills/tree/7f4b801c7248d51b093ec365aceb114bd06139e6).
Its skill library is [MIT licensed](https://github.com/scenario-labs/skills/blob/7f4b801c7248d51b093ec365aceb114bd06139e6/LICENSE).
Generated assets retain their own applicable service/model/input terms; the skill
license does not establish their provenance.

The project skill above is available without installing upstream. For optional
Scenario setup, follow its [pinned installation guide](https://github.com/scenario-labs/skills/blob/7f4b801c7248d51b093ec365aceb114bd06139e6/README.md#install)
and inspect the selected files before use. From your owned checkout, this exact
installer/source pin can first list the available skills without installing them:

```sh
npx skills@1.7.4 add scenario-labs/skills#7f4b801c7248d51b093ec365aceb114bd06139e6 --list
```

Inspect the existing project skill paths before installation and resolve any naming
collision without overwriting another contributor's work. For Codex, install the
selected cloud skills into the current project (other agents use the installer's
documented `--agent` value):

```sh
npx skills@1.7.4 add scenario-labs/skills#7f4b801c7248d51b093ec365aceb114bd06139e6 --skill scenario scenario-consistency scenario-3d --agent codex
```

An agent-run installer may select non-interactively; do not rely on a confirmation
prompt to protect existing files. Inspect the resulting diff and retain any lock
metadata it writes. Record the selected paths/revision in the issue. Additional
model/image skills are loaded only when the chosen job needs them. Connect Scenario
MCP through the chosen client's setup using the upstream `scenario` instructions;
use its supported authentication flow and local secret storage. Keep credentials
and signed download URLs out of Git. Paid generations need the account, appropriate
plan and agreed budget; local production and record validation do not.

For local Blender production, start with the
[Blender expert family](https://github.com/scenario-labs/skills/blob/7f4b801c7248d51b093ec365aceb114bd06139e6/skills/dcc/blender/README.md).
The lead and export specialist are the minimal useful pair; add retopology, UV
baking and texturing specialists for actual cleanup needs. Specialists import the
lead's scripts, so retain their sibling layout. The same pinned command accepts
`--skill scenario-blender-expert scenario-blender-export` for that pair. Upstream
reports headless tests on Blender 5.2.1; this repo has not validated those scripts
on the contributor's installed Blender.

The [Unreal family explicitly lacks live engine verification](https://github.com/scenario-labs/skills/blob/7f4b801c7248d51b093ec365aceb114bd06139e6/skills/game-engines/unreal/README.md).
Use its import-manifest ideas as references; do not replace `tools/dev.py` or
execute its unverified import/build scripts as part of this setup. Keep the pinned
engine, controlled MCP startup, existing endpoint and sequential editor calls.
Upgrading optional skills is a separate reviewed revision: inspect upstream changes,
update the pin deliberately and repeat affected checks; a blanket skills update
does not preserve the reviewed state.

## Static-prop pilot

Use one approved prop, such as a camp chest, to validate this workflow on the
chosen account and host. Record a complete first revision, import/inspect/save/reload
it, then make one visible change from its approved source/reference. Preserve r001,
produce r002, and validate both records with `--previous`. Reimport deliberately
at the same Unreal package. Check that existing placed references still resolve,
the change is visible, collision/interaction still work, and there are no duplicate
packages from the rerun. Compare the source/export and in-game views, record actual
spend, and attach evidence for both versions.

This PR's offline fixtures prove record validation and revision planning only.
They do not complete the paid generation, Blender, Unreal or visual pilot. Mark
each of those stages pending until observed; lack of a service connection leaves
local preparation and validation available.
