# Editor task brief: <outcome>

Fill this into the existing issue or approved ticket; link to decisions already recorded there. The agent drafts it from the contributor's description. This is a refinement of the approved scope, not a second approval gate. Follow [editor contributions](../agents/editor-contributions.md).

- **Issue and authority:** <issue, approved scope or delegated decisions; unresolved product choices>
- **Owner and workspace:** <writer, branch, checkout, fixed baseline SHA; editor owner or handoff needed>
- **Outcome:** <one change the player or contributor can observe>
- **Preserve:** <accepted rules, input, camera, saves, layout or other behavior this task must retain>
- **Paths:** <allowed source/content paths; existing binary paths reserved in the claim; expected new assets>
- **Required approach:** <only constraints material to acceptance, such as a real PCG graph or an existing skeleton; otherwise let the agent choose>
- **Done when:** <repeatable action and expected result for each acceptance check, including a relevant failure/ending/restart case>
- **Evidence:** <applicable automation, save/reload, interaction, inspected capture or recording; who performs any manual check>
- **Recovery point and experiment bound:** <owned commit or preserved starting work; time/attempt limit for an uncertain step>

## Example: replace one prop

Outcome: replace the placeholder camp prop with an approved static mesh.
Preserve the camp's position, interaction, collision clearance and game rules.
Reserve the exact existing map/asset paths before editing and add the candidate
under the issue's agreed folder within `/Game/DnDDiceRoll`.
Done means the candidate survives save/reload, the camp interaction still works,
and the prop does not obscure the agreed camera views. Evidence includes provenance,
an inspected in-game capture and the interaction result. If the candidate cannot
meet the agreed scale or collision requirements, report the mismatch before
replacing the accepted prop.
