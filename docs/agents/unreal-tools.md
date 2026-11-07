# Unreal agent setup and recovery

[The contribution workflow](workflow.md) governs planning and delivery. [Development](../development.md) governs Unreal operations. Epic's optional skills add editor expertise; these project contracts remain authoritative when generic instructions differ.

## Optional Claude Code plugin

The editor-side plugins and direct MCP configurations are already committed. A Claude contributor can install Epic's instructions from within Claude Code:

```text
/plugin install unreal-engine-skills-for-claude-code@claude-plugins-official
```

If the official marketplace is absent, follow [Epic's installation instructions](https://github.com/EpicGames/unreal-engine-skills-for-claude-code-plugin#installation). Inspect `/plugin` to confirm installation and record the installed version in your run notes. Keep the existing project MCP configuration. Inspect `/mcp` for the `unreal` connection; Epic's examples call theirs `unreal-mcp`. This repository does not install global skills or automatically enable a team plugin. Contributors using another agent follow the same project docs without installing it.

`CLAUDE.md` imports `AGENTS.md`, keeping the shared rules in one file. Confirm it appears in Claude's memory/context view; [Claude documents file imports](https://code.claude.com/docs/en/memory#import-additional-files).

## Compatibility rules

| Upstream example or default | This project's contract |
| --- | --- |
| Live Coding for C++ | Use the close/build/editor lifecycle in development; this engine's Live Coding path is Windows-only and unsupported on Linux. |
| MCP auto-start in editor preferences | Keep auto-start disabled. The helper starts MCP only for the owned interactive editor; headless commandlets remain isolated. |
| Default endpoint on port 8000 | Use the committed endpoint from `tools/versions.json` and existing client configuration. Regenerating defaults can introduce another connection or the wrong port. |
| Independent concurrent calls or multiple editors | One editor owner, sequential calls, with exact path/PID/engine verification before mutation. |
| Save before bulk changes | Coordinate asset paths, preserve existing work, save deliberately through Unreal, and check individual results and complete dirty state. |
| Plugin support on several operating systems | Local Unreal validation remains limited to the host/engine combination in development. |

Plugin installation alone proves neither connection nor editor identity. From the owning checkout, run `python3 tools/dev.py status`, then `python3 tools/dev.py mcp`. The latter opens a fresh session, independently verifies runtime identity, and lists tools. Continue with discovery and the schemas actually returned by that editor.

## Project-skill pilot

The editor imports `unreal_starter.skills.ProjectAuthoringSkill` at startup. Its short description is discoverable through Epic's AgentSkillToolset; the instruction body comes directly from [editor authoring](editor-authoring.md). It covers project-specific asset and presentation work, complementing bundled material/Blueprint skills. It creates no assets and changes no editor settings.

On unfamiliar editor work, discover the registered AgentSkillToolset using the normal list/describe/call sequence. Read its schema, call `ListSkills`, then `GetSkills` with the returned pilot path. Read the instructions before writing. Do not assume a Python or toolset name is the wire identifier. The pilot is defined in source; edit its Markdown and restart through the safe lifecycle to refresh it, rather than updating the transient class through asset tools.

With this checkout's editor closed, `python3 tools/dev.py verify-skills` runs a headless pinned-engine discovery/readback check. Build the editor target first on a fresh clone. Evidence appears under `.local/skills/`. This proves engine registration and content readback; live MCP/client loading requires a separate owner-controlled check. The engine's AndroidFileServer plugin is disabled in `DnDDiceRoll.uproject` because its editor settings wrote a `SecurityToken` section into `Config/DefaultEngine.ini` on every run. Keep it disabled; `doctor` checks this, and the helper fails any run that rewrites tracked config.

## Recover after an editor restart

1. Record the last operation and whether it completed. A timed-out mutation has an unknown outcome: inspect the resulting assets/state before deciding what to do next.
2. Coordinate with the editor owner. Use the normal save/close/build/editor sequence when required; preserve the configured port and existing work.
3. Run `python3 tools/dev.py status` from the owning checkout. An occupied endpoint answering for another checkout is a stop. A closed editor is expected between build steps.
4. Once that editor is ready, run `python3 tools/dev.py mcp` to prove a fresh direct session and exact live identity. If the agent still has stale tools, use its reconnect/reload action; otherwise restart the agent session and resume from the issue/run ledger.
5. Discover toolsets and relevant project skills again. Reconcile any uncertain operation before continuing; recovery never authorizes replay of mutations.

## Official proxy investigation

Epic documents an optional STDIO proxy that retains the client connection across editor shutdowns. A connected proxy or cached tool catalog does not prove a live editor is reachable. [Official setup](https://github.com/EpicGames/unreal-engine-skills-for-claude-code-plugin/blob/0e27bde2b55c5d3f6948532f4296aaa3654d8f4b/skills/unreal-mcp/references/setup.md), [recovery semantics](https://github.com/EpicGames/unreal-engine-skills-for-claude-code-plugin/blob/0e27bde2b55c5d3f6948532f4296aaa3654d8f4b/skills/unreal-mcp/references/operations.md).

On 2026-10-31 the pinned UE 5.8.4 CL 58902052 installation lacked `Engine/Plugins/Experimental/ModelContextProtocol/Extras/Proxy`, including `Bin/Mac/unreal_mcp_proxy`; the Linux install checked on 2026-11-07 also lacks `Bin/Linux/unreal_mcp_proxy`. The authenticated account also received HTTP 404 for `EpicGames/UnrealEngine`; the official source was unavailable through that account. Direct HTTP remains the supported connection. `doctor` reports the optional binary's local availability without treating absence as an environment failure.

Enable the proxy only after obtaining an official compatible distribution and reviewing its bundled README. Do this in an isolated test checkout with a coordinated editor owner. Preserve the direct configuration, use the actual endpoint, and configure one agent connection. Epic's installer rewrites `.mcp.json`; the current repository checks deliberately require direct HTTP, and the Codex configuration also needs a validated adapter. Changing transport therefore needs a reviewed update to those checks/configurations, not a local installer run against this checkout.

Acceptance for that follow-up: verify exact engine/project identity through the proxy; stop/restart the owned editor normally; prove the same client session recovers and refreshes its catalog; refuse a wrong project; confirm that failed mutations are not replayed; and restore direct HTTP successfully. Do not replace the pinned engine or introduce an untested custom transport to satisfy an optional dependency.

## Provenance

Inspected Epic's MIT-licensed [plugin at commit 0e27bde2b55c5d3f6948532f4296aaa3654d8f4b](https://github.com/EpicGames/unreal-engine-skills-for-claude-code-plugin/tree/0e27bde2b55c5d3f6948532f4296aaa3654d8f4b), plugin version 3.1.2, on 2026-10-31. References: `skills/unreal-mcp`, `skills/unreal-skill`, and the bundled UE 5.8.4 `toolset_registry.agent_skill` implementation. No upstream code/skill files are vendored. The marketplace command installs the available release, not necessarily this inspected revision; evaluate changed guidance against the compatibility rules before adopting it.
