# Unreal AI Boilerplate

Building a game with friends and their agents? Start each agent on [the contributor guide](CONTRIBUTING.md). It covers local setup, safe updates, and reviewed PRs into `staging`.

A reusable, game-neutral Unreal Engine game-development boilerplate for AI agents, with a safe editor lifecycle, native Epic MCP access, reproducible assets, automation, and a Mac packaging path. Gameplay decisions remain open in [the game brief](docs/game-brief.md).

The validated baseline is **Unreal Engine 5.8.4, CL 58902052**, with Epic's bundled **Unreal MCP**, on an Apple Silicon Mac. MCP is experimental and editor-only. Versions stay pinned until an upgrade passes the same checks.

```sh
# Python 3.11+; on the setup Mac, /opt/homebrew/bin/python3
python3 tools/dev.py doctor
python3 -m unittest discover -s tools/tests
python3 tools/dev.py build
python3 tools/dev.py bootstrap
python3 tools/dev.py test
python3 tools/dev.py editor    # waits for project identity and map readiness
python3 tools/dev.py status
python3 tools/dev.py smoke
python3 tools/dev.py capture
python3 tools/dev.py close
python3 tools/dev.py package   # Mac Development build + packaged startup check
```

Run from this directory. Configure an alternate engine install with `UE_ROOT`. Launch an MCP client after the editor is ready; `.codex/config.toml` and `.mcp.json` share its local endpoint. Existing clients may need restarting to load configuration, and Codex requires a trusted project.

Start with [development](docs/development.md), [architecture conventions](docs/architecture.md), and [editor recipes](docs/editor-recipes.md). [AGENTS.md](AGENTS.md) is the agent entry point, [asset provenance](docs/assets.md) records content sources, and [council decisions](docs/council-decisions.md) explains the reviewed scope. [Validation results](docs/validation.md) records the completed checks and limits. The directory, module, project ID, and `/Game/DnDDiceRoll` paths retain their original internal identifiers; they imply no game design.

To start a separate game, follow [reusing the starter](docs/reusing-the-starter.md). `v0.2.0` is the current starter tag; `v0.1.0` was the initial validated baseline. Clone-based reuse preserves Unreal assets stored in Git LFS.
