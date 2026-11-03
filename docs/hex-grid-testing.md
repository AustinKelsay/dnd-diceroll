# Hex grid proposal: testing notes

This is a proposal for issue #111. It does not replace Austin's square-grid game. It lives on its own branch for review.

## What it changes
- Board uses odd-r offset hexes (FHexGrid: distance, line, step).
- Spell areas (sphere, cone, line, cube) are converted to hex shapes and drawn as matte hex overlays.
- Cover rules: walls give cover; creatures give +2 only when at least one size larger than the target. Monsters with INT 6+ seek cover.
- Wall of Fire is a node-shaped hex line (60 ft).

## Build and run
1. Open DnDDiceRoll.uproject in UE 5.8 and build DnDDiceRollEditor (Development Win64).
2. Run tests: UnrealEditor-Cmd.exe DnDDiceRoll.uproject -ExecCmds="Automation RunTests Project; Quit" -unattended -nullrhi
3. Play with -game as usual. Combat uses the hex board.

## Test counts
296 of 296 Project tests pass. New: CoverTacticsMove (AI moves from an exposed hex into cover), WallOfFirePath.

## Known gaps
- Combat levels are capped at 5, so Wall of Fire (4th) and Cone of Cold (5th) can't be cast in play yet. The Wall of Fire test places the wall through a restored snapshot.