# Editor authoring recipes

Before project asset or presentation work, load the project authoring pilot through AgentSkillToolset; [Unreal agent setup](agents/unreal-tools.md) describes discovery and compatibility. Its source is [editor authoring](agents/editor-authoring.md), also readable without an editor connection.

Start with `python3 tools/dev.py status`. One owner makes sequential calls, discovers `list_toolsets`, describes each toolset, and dispatches using the returned schema. Names below are authoring capabilities in the pinned Epic installation; retrieve their complete registered toolset names from discovery rather than guessing. Browse Epic's `AgentSkillToolset` for bundled instructions such as `material_basics`, `blueprint_basics`, `default_outdoor_lighting`, and `unreal_skill_best_practices`. [Epic: Unreal MCP](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor)

| Task | Bundled capability | Completion check |
| --- | --- | --- |
| Blockout | `primitive.add_cube`, `add_sphere`, `add_cylinder`, `add_cone` | Inspect transforms, collision, exact labels, and viewport; save map |
| Material | `material.create_material`, expression/connect tools, `recompile` | Check compile errors, save, and inspect appearance |
| Material instance | `material_instance.create`, parameter setters | Verify parent and parameters; save explicitly |
| Blueprint | `blueprint.create`, graph/DSL tools, `compile_blueprint` | Verify parent/type, compile, inspect errors, save |
| Data assets/tables | `data_asset.create`, `data_table` tools | Verify type/schema and values; save |
| Asset persistence | `asset.save_assets`, `is_dirty` | Check individual save outcomes and full status dirty list |
| C++ class | `dev.py new-class actor ExampleActor` (also `component`, `object`) | Refuses overwrite; close/build/relaunch before use |
| Viewport evidence | `dev.py capture` | Inspect the returned PNG under `.local/captures` |

For repeatable recipes, preflight paths and types, create missing assets, validate existing output, and preserve user edits. Record imports and licenses in `docs/assets.md`. If output exists but is incompatible or incomplete, report it for explicit repair. Never silently recreate or overwrite it.

The programmatic tool sandbox is intentionally restricted and cannot import `unreal`. Use registered capabilities or a narrowly scoped project editor script when necessary. `Content/Python/init_unreal.py` registers workspace status tools and loads the project authoring skill; it creates no assets on editor startup.
