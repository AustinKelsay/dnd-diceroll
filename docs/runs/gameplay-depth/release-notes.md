# Release notes: #112 release (in progress)

Player-facing changes in the #112 compact gameplay-depth release, collected as they integrate on `staging`. Started by [#224](https://github.com/AustinKelsay/dnd-diceroll/issues/224); each later delivery of the release adds its own entry. The run records under `docs/runs/gameplay-depth/` hold the evidence.

## New

- **Abandon run** ([#224](https://github.com/AustinKelsay/dnd-diceroll/issues/224)). The start screen offers Abandon run while an unfinished expedition is saved. It asks first and defaults to keeping the run, and Resume stays the focused choice; keyboard (Tab, Enter, Esc) and pointer both work. After a confirmed abandon, Begin starts a fresh expedition; the records of finished runs are kept and the abandoned run is not recorded. If the game cannot write the abandon, nothing changes and the start screen says so. Rules, random numbers, balance and the save schema are unchanged.

## Known limits

- **A hero sealed in by bodies** ([#217](https://github.com/AustinKelsay/dnd-diceroll/issues/217)). Bodies keep their cells, so bodies plus an obstacle or the board edge can seal in a hero that the last foe cannot reach, and that fight never ends. Revision 9's layouts avoid the pockets that fewer bodies could close, and the balance sweep found none in 3,168 runs, but it can still happen at a board edge. The way out: Return to menu, then Abandon run. A rules fix is planned after the release.
