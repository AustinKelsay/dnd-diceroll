# Unreal layout and screen flow

Project adaptation of the upstream reference. Read this for UMG implementation or a screen review; the skill entrypoint owns scope and project rules.

## Layout

Inspect the existing HUD technology before choosing widgets. In UMG, author Widget Blueprints at a consistent reference resolution. Use Canvas Panel anchors for edge-relative placement and Horizontal/Vertical Boxes or grids for groups. Let text size to content; reserve fixed bounds for elements with a real size contract. Use the project DPI curve and Size/Scale Boxes deliberately instead of permanent render-transform scaling.

Separate decorative art from labels, hit targets and focus styles. Test long action names, empty/full resource values, multiline messages and enlarged text. Keep selected, hovered, disabled, downed and dead states distinguishable without color alone. Preserve a clear view of legal cells and targets beneath the HUD.

## Input and flow

- Establish the PlayerController/UI owner and the active input mode. A modal must consume the intended UI input without forwarding a duplicate command to gameplay.
- Set initial focus on open, navigation between interactive controls and a visible focus indicator. Define behavior at list ends and after a control becomes disabled.
- Restore an appropriate prior focus target after Back/close, including when the previous control was removed. Verify pointer and keyboard coexistence rather than clearing focus on every mouse move.
- Use the project's existing screen flow. Nested pause/settings/confirm overlays can justify a stack; a single HUD need not introduce a screen framework or CommonUI dependency.
- Support only the devices in the accepted slice. Record a controller-only walkthrough when claiming controller support.

## State boundary

UI handlers submit validated game commands and observe accepted results/const state. Bind updates at meaningful transitions where the public seam exists; remove subscriptions with the owner's lifetime. Animated health bars and selected widgets are presentation values. Widget recreation, repeated clicks and menu closure must not duplicate actions, change RNG or restore spent resources.

For Expedition, inspect startup NewRun/Resume decisions, action rejection, downed versus dead, smaller surviving parties, route/reward/recovery screens, and run records only as relevant to the changed screen. Preserve save/resume and permanent-loss semantics from the accepted spec.

## Review evidence

Record the tested resolution, window/aspect policy, text scale and input device. Exercise supported narrow/wide layouts, long labels and focus through modals. Check that HUD hit regions do not hide or activate the board underneath. Save and reload changed Widget Blueprints, then capture and inspect actual rendered interaction. Designer previews and automation supplement that observation.

## Primary documentation

- [Epic: UMG best practices](https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-best-practices-in-unreal-engine)

Use the pinned engine's APIs and installed headers/tool schemas for exact calls. This reference does not assume an optional UI plugin is installed.
