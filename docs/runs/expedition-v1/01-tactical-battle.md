# Ticket #2 — tactical battle session

Worker is the sole implementation writer/editor owner on `feature/expedition-v1`.
Fixed review baseline: `bca18be64b76ab7c0cac1cae71915ddccad489b2`.

## Plan and seams

Approved seam: `FCombatBattle` Start / Submit / Snapshot, with legal-move and legal-target queries and one bounded enemy-turn operation. Tests use public encounter definitions and deterministic seeds. Reflected value state retains IDs, initiative, budgets, turn and random progression; presentation references never enter the state. Actor/controller/HUD adapters create the board at runtime on the unchanged sandbox. Four roles have basic attacks here; role abilities, rescue/death, expedition routing and save adapters are later tickets.

Use cardinal movement and Manhattan attack range on an 8x8 arena, occupied cells blocking travel, blocked-cell line of sight, one attack per turn, movement before/after attacking. These are labeled tactical adaptations. Initiative is d20 + modifier with stable ID tie-break. Natural 1 misses, natural 20 hits and doubles damage dice. Enemy planning is finite breadth-first search followed by commands through the same validation.

## Work and evidence

- Read full issue #2, required guidance and skills; doctor passed on pinned UE 5.8.4 / CL 58902052. Initial status: no associated project process or MCP listener.
- First build exposed missing module include roots; moved headers to the conventional Public and implementation to Private directories.
- Encounter start and movement/atomic rejection each had a compiled failing automation run followed by passing behavior. Additional attack, sight, turn, terminal and deterministic AI tests pass. Scoped suite: 6/6, zero warnings, `.local/automation/20261031T013113163929Z`. Full `Project.` prefix: 8/8 including map PIE and preserved starter fixture, zero warnings, `.local/automation/20261031T013212996844Z`.
- All 22 offline helper tests pass. Bootstrap validated and preserved the existing map, zero errors/warnings: `.local/bootstrap/20261031T013201416408Z.log`.
- Rendered inspection found and corrected Retina HUD scaling, board framing and runtime static-lighting warnings. Runtime meshes/lights use movable presentation in the PIE copy; saved map stays intact.
- Real native mouse attack observed: Sera hit Briar raider for 5 damage, HP 16→11; `.local/captures/after-desktop-q-before.png`. Initial window input attempts were no-ops; native driver foreground/desktop recovery finally delivered the click. Keyboard delivery is under verification; changed Canvas-only controller from Game+UI to game-only input focus. Temporary targeted input logs removed before candidate review.
- One immediate editor relaunch failed MCP bind despite a free preflight port; recover through normal exact-PID quit, then closed-editor validation before launch retry. No forced quit or binary asset edits.
- Standards and Spec reviews: pending, exactly two independent read-only reviews after candidate implementation.
- Rendered interaction and ownership handback: pending.
