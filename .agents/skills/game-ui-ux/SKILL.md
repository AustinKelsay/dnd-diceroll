---
name: game-ui-ux
description: Improve this Unreal project's HUD, menus, responsive layout, focus navigation, or UI state feedback. Use for UI readability, resolution scaling, screen flow, and input accessibility work.
license: Apache-2.0
---

# Game UI and UX

Adapted from the pinned upstream skill; source and file hashes are in [UPSTREAM.json](UPSTREAM.json).

## Project contract

Use the [contribution workflow](../../../docs/agents/workflow.md) for the approved scope and review. Read [development](../../../docs/development.md) before editor or C++ work and [architecture](../../../docs/architecture.md) for the C++/Blueprint boundary. Before gameplay UI, read the [game brief](../../../docs/game-brief.md) and any available `CONTEXT.md`, `GLOSSARY.md`, relevant ADRs and feature spec. Treat unresolved product requirements as design input.

## Workflow

1. Inspect the existing HUD, input/controller ownership, UI technology, screenshots and public game-state queries. List the player decisions this screen supports and the accepted device/resolution targets. Preserve the current implementation unless the approved slice needs a migration.
2. Define a reference resolution, responsive placement and a policy for extra width/height. Prioritize readable actions, turn/selection, resources and results over decorative art. Use text/icons as well as color for meaningful state.
3. For UMG work, use anchors, layout panels, size constraints and the project's DPI scaling. Match the authoring reference size across Widget Blueprints; reserve render transforms for temporary visual effects. If the current HUD uses Canvas/Slate, implement equivalent layout behavior there within scope rather than replacing the UI framework automatically.
4. Give each screen a clear input owner, initial focus and visible focus state. Define navigation and Back behavior for the approved devices; restore focus after a modal closes. Modal input must not also submit a game action behind the UI. A simple screen can remain simple; use a stack when nested overlays need it.
5. Route player intent through the existing validated command interface. Render const queries/snapshots or accepted results; widget state, animation and selection never decide rules or write authoritative state. Refresh on meaningful state changes where the existing seam supports it; pair delegate binding with owned unbinding. Avoid introducing a global event bus for one screen.
6. Check narrow/wide windows, the target resolution and larger text. Exercise real pointer/keyboard navigation and controller navigation when included in scope. Test disabled/invalid actions, repeated input, opening/closing overlays and long labels. Include the spec's own states, such as loss conditions, scarce resources and save/resume feedback, when the screen exposes them.
7. Report the exact resolutions/devices exercised and provide inspected rendered evidence. A widget compile, headless test or successful command response alone does not prove readable layout or usable input. Label devices and paths that remain untested.

## Reference

Read [layout and screen flow](references/layout-and-flow.md) for Unreal implementation choices and a concrete review checklist. Asset family production belongs to `create-game-assets` when that skill is available; this skill owns layout and interaction.
