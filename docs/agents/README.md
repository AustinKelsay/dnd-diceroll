# Agent task index

Find the document that owns your task, then follow its links. Each row names one owner; the linked rules stay authoritative, and this index restates none of them. Codex-style agents discover `.agents/skills/`, and Claude Code discovers the same skills through the `.claude/skills` link. Other agents open the listed `SKILL.md` directly. Repository docs win when a skill and a doc disagree.

| Task | Read first | Then use |
| --- | --- | --- |
| Onboard, sync, claim a ticket, open a PR, or resume | [CONTRIBUTING](../../CONTRIBUTING.md), [workflow](workflow.md) | Matt Pocock skills named in the workflow; [feature run](../templates/feature-run.md) for multi-ticket features |
| Gameplay rules or C++/Blueprint code | [workflow](workflow.md) step 1, [game brief](../game-brief.md), [V1 spec](../specs/expedition-v1.md), [architecture](../architecture.md) | [domain docs](domain.md): [CONTEXT](../../CONTEXT.md), [GLOSSARY](../../GLOSSARY.md), [ADRs](../adr/) |
| File, triage, or label issues | [issue tracker](issue-tracker.md) | [triage labels](triage-labels.md) |
| Build, launch, close, test, or call MCP | [development](../development.md) | [Unreal agent setup](unreal-tools.md) for Epic's plugin, the project skill, and connection recovery |
| Editor, presentation, or input task | [editor contributions](editor-contributions.md) | [task brief](../templates/editor-task.md), [completion report](../templates/editor-report.md), [editor recipes](../editor-recipes.md) |
| Import a new or replacement asset | [asset intake](asset-intake.md) | [asset provenance](../assets.md) |
| Generate or revise an asset with retained source | [asset production](asset-production.md) | [asset records](asset-manifests.md), [record template](../templates/asset-record.json), `tools/asset_manifest.py`, [asset-production skill](../../.agents/skills/asset-production/SKILL.md) |
| Art direction, asset families, raster QA | [create-game-assets skill](../../.agents/skills/create-game-assets/SKILL.md) | its family roster, which links to revision records rather than replacing them |
| HUD, menus, focus, feedback | [game-ui-ux skill](../../.agents/skills/game-ui-ux/SKILL.md) | [editor contributions](editor-contributions.md) for the brief and report |
| Encounter blockouts and pacing | [level-design skill](../../.agents/skills/level-design/SKILL.md) | [editor contributions](editor-contributions.md) for the brief and report |
| Measured performance work | [performance-optimization skill](../../.agents/skills/performance-optimization/SKILL.md) | |

## Agent wiring

Claude Code reads `CLAUDE.md`, which imports `AGENTS.md`, discovers the project skills through `.claude/skills`, connects to Unreal through `.mcp.json`, and runs the shared `.claude/settings.json` hook that blocks destructive Git commands. Codex reads `AGENTS.md` and `.agents/skills` natively and uses `.codex/config.toml` for Unreal. Cursor reads `AGENTS.md` natively. Any other agent works from this repository by reading `AGENTS.md` first and opening the listed `SKILL.md` files directly; for Gemini CLI, set `context.fileName` to `AGENTS.md` in a local `.gemini/settings.json`. Agents without the hook rely on the workflow's Git rules.

Inside the editor, `ProjectAuthoringSkill` serves [editor authoring](editor-authoring.md), a condensed checklist for the same editor and asset work. Editor startup reads that file by path, so moving or renaming it breaks the skill.
