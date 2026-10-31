# Asset records

Use one JSON record per immutable asset revision, stored with the retained source:
`Art/<asset-id>/r001/asset.json`, `r002/asset.json`, and so on. The
[production recipe](asset-production.md) owns generation and runtime validation.
The issue/PR owns acceptance and identifies the currently accepted record;
`validation.state` reports checks, not approval. Drafts stay identifiable as drafts.

If a `create-game-assets` family manifest is present, keep it as the art roster and
brief/visual-acceptance record. Reuse its asset `id` as this record's `asset_id` and
point its `source.prompt_or_source_record` at the current revision JSON. This
record owns source/export hashes and import policy; the roster owns family status
and approval. Link the records rather than duplicating technical metadata. The
optional [create-game-assets skill](../../.agents/skills/create-game-assets/SKILL.md)
is not required for this helper, and the two JSON formats are not interchangeable.

Copy the [record template](../templates/asset-record.json), replace its example
values, and compute SHA-256 from the actual files. The template deliberately has
no matching source files; it is not a passing asset or production import preset.
Keep editable sources, exports and retained reference inputs in revision-specific
paths under `Art/`. Shared unchanged files may be referenced from an earlier
revision. Preserve earlier records and bytes; use Git LFS for covered binary types
and add coverage deliberately for new large binary formats before staging them.

## Contract v1

`tools/asset_manifest.py` validates exactly these top-level fields. Unknown fields
are rejected to catch misspellings; extend the schema version deliberately.

| Field | Meaning |
| --- | --- |
| `schema_version` | Integer `1`. |
| `asset_id`, `revision` | Stable lowercase hyphenated ID and positive integer; changed records advance by exactly one. |
| `unreal_package` | Stable package path under `/Game/DnDDiceRoll`, without object suffix or `.uasset`. A rename is a separate coordinated migration. |
| `issue` | Owning issue reference. |
| `provenance` | Nonempty `author`, `license`, `origin`; record service/model/input terms where applicable. This is a declaration, not automated rights verification. |
| `recipe` | Nonempty object: authored method or provider/model/revision, prompt/settings/seed, job/quote/cost and reference decisions as applicable. Use explicit unknowns instead of invented versions. |
| `tools` | Nonempty mapping of tool names to version strings. |
| `files` | Entries with exactly `role`, `path`, `sha256`; roles are `reference`, `source`, `export`. At least one editable source and export. Paths are unique POSIX paths under `Art/`; symlinks, traversal, missing files and unhydrated LFS pointers fail. Hashes must match current bytes. |
| `import_policy` | Exactly `name`, positive integer `version`, `settings` object. Capture the applicable importer, material, scale, collision and other chosen settings; change the policy version when its contract changes. |
| `validation` | Exactly `state` (`pending`, `passed`, `failed`) and `evidence` (list of references). `passed` needs evidence. Keep checks pending after revisions until repeated on the new state. |

Recipes and settings are provider-specific JSON objects. Keep secret values and
temporary/signed URLs out of records; reference permanent evidence and retained
inputs. Prompts and seeds document how a candidate was made; the stored files
preserve the accepted result even when a provider/model changes.

## Validate and plan

Python 3.11+, standard library only; run from any directory. File paths resolve
against the repository containing the helper, or an explicit `--root` for isolated
fixtures. The command reads manifests and hashes files; it performs no writes,
network access, imports, deletion or acceptance decisions.

```sh
python3 tools/asset_manifest.py Art/camp-chest/r001/asset.json
python3 tools/asset_manifest.py Art/camp-chest/r002/asset.json --previous Art/camp-chest/r001/asset.json
```

First use requires revision 1. Existing revisions require `--previous`; both
records and their retained files are checked. Pass the actual prior accepted or
candidate record identified by the issue, rather than bypassing history with a
new ID. Plan actions are:

- `new`: valid revision 1 without a prior record; inspect the actual Unreal path
  before creating anything, since this tool cannot detect an existing package.
- `unchanged`: the complete records are equal, including JSON value types; no record change is needed.
- `metadata_only`: next revision with identical exported hashes/relative paths and import
  policy. References/source/recipe or evidence may have changed; inspect that work.
- `reimport`: next revision with changed exported hashes/relative paths or import policy;
  reimport intentionally at the same package through the verified editor owner.

Export comparisons ignore only the `Art/<asset-id>/r<positive-integer>/` container
(including zero-padded revision numbers). They preserve every subfolder below it,
so `wood/albedo.png` and `metal/albedo.png` remain distinct. Exports outside that
layout use their full repository path; relocating them conservatively requires
reimport. JSON number/boolean changes in settings count as policy changes.

Every action is conditional on the recorded history and current bytes, not a scan
of Unreal. The tool cannot prove import freshness, dependency completeness, visual
quality, rights, acceptance or that evidence actually supports its claim. Include
all external textures/material files in `files` with role `export`; omission can hide a change.
Check the live asset's source and settings before treating any plan as no work.

Failure exits nonzero with a reason and no plan. Correct the record or restore
the intended preserved files; do not simply refresh hashes to bless an accidental
overwrite. Re-run the helper, then the relevant live production checks.
