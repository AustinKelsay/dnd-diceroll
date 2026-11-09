# On-screen acceptance pass: PR #167 (#58 action dock cost and usage tags) at `b12981f`

**Evidence only. Never merge this branch.** PNGs go through Git LFS.

| | |
| --- | --- |
| Source | PR #167 head `b12981f35057f2a2a509381dd6745da58440e41a` (`feature/58-action-clarity-hud`); `gh pr view 167` checked before packaging and again after: unchanged. |
| Package | `tools/dev.py --cores 2 package` under the machine lock; 0 errors, 0 warnings, startup map reached. Binary SHA-256 `f6837ee1294ce09ade883f0d1aca2518ace70193f6849b68cff26bf2d932bcc8`. |
| Host / display | PlebDev Ultra, UE 5.8.4, RTX 4080 SUPER, Vulkan SM5. Nested headless Hyprland (see `rig/`; steps are in the earlier `evidence/onscreen-7a0c6ef` report). Sizes 1280x720, 1920x1080, 1600x600. |
| Input | **Virtual, not physical**: `wtype` and a Wayland virtual-pointer client (`rig/ptr2.c`). |
| Saves | Isolated `-UserDir` / `-ExpeditionSaveRoot` per launch. Fixtures `kit-barbarian-battle`, `kit-barbarian-raging`, `kit-wizard-all-spent`, `sight-blocked-battle`, `board-18x12` copied from this checkout's `Saved/CaptureFixtures`; fresh runs for the rest. |
| Lock | The package and the first game sessions ran before the new rule; sessions from 23:34Z on were under the machine lock (`rig/lib167.sh`: `getlock`/`droplock`). My games at 23:21 to 23:28Z (before the rule) overlapped other agents' Unreal jobs; the 23:27Z renderer crash the coordinator mentioned fell inside that window. |
| Runs | 11 launches, all closed through a normal window close. 1 more launch died at startup (SDL `wl_output.done` SIGFPE on the headless output, same rig race as the previous pass; log `logs/game-h1-first-launch-sigfpe.log`); the identical relaunch worked. |
| Declined image read | `pr167/shots/sheet-421-docks-1600x600.png` (a contact sheet of six 1600x600 docks on the Watchtower run) was declined; I did not retry it and judged from the individual crops instead (frame 3 showed Arden's Battle Master dock). The same applies to my first read of `sheet-070-072` (turn-change frames), which I replaced by the two single crops. |

## PR checklist ("Pending on-screen checks")

Pass 5 / Partial 3 / Fail 0 / Not tested 1 (the SRD dock).

| # | Item | Result | What was seen |
| --- | --- | --- | --- |
| 1 | Pointer: hover every dock button incl. unavailable; tooltip with reasons; goes away when the pointer leaves; click on a disabled button does nothing; click on an available one selects the mode | **Pass** at 1280x720 | `021`..`024` (Attack with `Out of range (1)`, Rescue `No downed ally`, Potion `No wounded target`; End Turn shows no tooltip), `020`, `051`..`057`. Pointer left: `032` tooltip gone. Disabled click: `030` before, `031` after clicking Rescue (no mode change, no log line, tooltip stays the hover one), `032`. Available click: `018` (Rage accepted: tooltip gone, Bonus chip greyed, Rage 1/2), `056` (click Attack clears the Magic Missile tooltip). 1920x1080 and 1600x600 hover: `601`, `301`, `411`. **Caveat:** after a fresh launch, hover is ignored until the first click in the window (see Quirk 1). |
| 2 | Keyboard A, F, M, R, P show the tooltip incl. unavailable, stays until turn change, accepted command, End Turn or pointer mode choice; a rejected command leaves it up and logs `Cannot act: Needs Rage active`; Tab and Space as before | **Partial** | M on Berserker not raging (`010`): tooltip `Frenzy - Bonus action / Needs Rage / Needs Rage active / Out of range (1)`. Enter on a cell (`013`): log line `Cannot act: Needs Rage active` and the tooltip **stays up**. F on Rage (`014`). R with no downed ally (`040`). P then End Turn: tooltip gone at the next hero (`071`, `072`). Also `Cannot act: Sight is blocked` with the tooltip kept (`232`). A pointer click on the enemy in Frenzy mode (`011`) kept the tooltip but I did not see a log line in that frame (the log may have faded, it fades after about 4 s). **Tab not exercised**; keyboard `A` after the Berserker accepted-action case was not separately screenshotted. |
| 3 | Tags on every button: Fighter (Battle Master), Cleric, Wizard, Barbarian, Rogue, two-special kit, longest names, Watchtower 7-combatant at all three sizes; no clipping, legible at 1600x600 | **Pass** (legible but small at 1600x600) | Barbarian `001`, Rogue `220`, Cleric / Wizard / Fighter-Champion / Ranger `sheet-243-246-docks`, `sheet-311-316-docks-1600x600`, Battle Master `Second Wind` / `Precision Strike  Maneuver 2/2` (the longest name) at 1600x600 with 7 combatants (`410`, `421-turn3-1600x600`). Two-resource tags `Kit 2/2 + Slot 2/2` (Magic Missile, Cure Wounds) fit their buttons everywhere. 7-combatant Watchtower docks: `220` (1280), `300` (1600x600), `600` (1920). At 1600x600 tag text is about 7 px tall at 1x; readable, not roomy. |
| 4 | One screenshot per blocker state | **Pass** (11/11, Slot noted) | Action spent `054` / `237`; Bonus spent `020`; Kit at 0 `054`, `258`, `259` (`Need 1 Kit, have 0`); **Slot at 0 `258`** (`Need 1 Kit, have 0` and `Need 1 Slot, have 0` together: every Slot ability also costs a Kit, and the Kit runs out first, so Slot alone at 0 is not reachable in these kits); Rage required `010`; already raging `061`; no wounded ally `023`, `301`; no downed ally `022`, `040`; out of range `010`, `237`; blocked sight `232` (`Sight is blocked`); no movement left `068` (`No movement left`). |
| 5 | Build picker: Berserker, Evocation and others, readable and not overlapping the action buttons at 1280x720 and 1600x600 | **Pass** | 1280: Berserker `201`, Evocation, Cleric Life, Rogue Thief, Artificer Alchemist and Armorer, Paladin Watchers, Sorcerer Draconic, Warlock Archfey (`sheet-picker-1280`). 1600x600: Evocation `402`, Battle Master `401` and Berserker `403` (`sheet-picker-1600x600`). The picker lives in the right-hand panel and never touches the dock (the dock does not exist on the start screen). Cost lines read `Action | 1 Kit - 2/2 - Camp restores 1 | 1 Slot - 2/2 - Camp restores 1`; stance `Needs Rage`, `Not while raging`. Minor at 1600x600: the first party slot box sits tight under the "Four heroes" caption (`402`). |
| 6 | Tags and tooltips hidden under Help, Records, the return question and playback; no stale tag after a turn change | **Pass** | `041` Help, `043` Records, `045` return: dock and tooltip are gone. `042`, `044`, `046`: closed again, the dock returns with current tags and the Rescue tooltip that was chosen before the overlay is back (the mode is still chosen, as the PR says). Playback: `263` (0.15 s after a move click) the dock is replaced by `Skip >> Tab`, `264` back with `Move 1`. Turn change: `071`/`072` no tooltip, tags of the new hero current. |
| 7 | Budget chips agree with the tags after each spend | **Pass** | Move 4 to 2 to 1 to 0 (`065`, `068`, `264`; chip and the `Action - Move N` tag agree); Bonus chip greyed after Rage (`018`, `020`); Action chip greyed after a cast (`237`); Kit and Slot go 2/2, 1/2, 0/2 with both casts (`237`, `258`). HUD coverage was not measured on screen (it is a headless test). |
| 8 | SRD run: the dock draws and nothing errors | **Not tested** | No UI or flag starts an SRD run. I made an SRD copy of the Wizard fixture (Ruleset 1 in both Progress and Encounter) in an isolated save folder; the game **refused it**: `Cannot act: Invalid saved expedition: phase, party, routes or history disagree.` (`500-srd-resumed-1280`; no error line in the log). So no SRD dock was seen. |
| 9 | Contract section 7: tooltip by hover and keyboard; End Turn has no cost tag | **Pass** | `024`, every dock shot: End Turn shows only `Space` and no tag. |

## Coordinator's follow-up items

1. **#143 confirmations on the `7a0c6ef` package** (18x12 at 1280x720): right-drag orbit worked on the second drag (`101`, `102`; the first one after launch did nothing, as before); frame 0.12 s after E mid-ease contained (`103`); both clear of HUD and log. One line posted on PR #159.
2. **Hover tooltips not appearing until the first click, in a fresh launch with no fixture: yes, reproduced** (`221-fresh-hover-noclick-1280`: pointer on Quick Exchange, no tooltip; `222-fresh-hover-after-click-1280`: after one click on empty board the tooltip shows). It is **not the same mechanism as defect 1 of the previous pass** (that one was `bKeyboardCursor` holding after an arrow key); here no key was pressed. It looks like the first pointer motion after launch is not delivered or not used until a click gives the window mouse focus, which may be a property of the virtual pointer on the headless compositor, so I would not call it a game defect until it is seen with a physical mouse.
3. **Move contour with all actions spent**: in `050` the chip reads `Move 0` and the Action and Bonus chips are grey, and a wide contour is still drawn. At that point no movement was left. In `068` (`Move 0`, Action still available) only the 1-cell attack reach is outlined. So the large contour in `050` is the Wizard's Attack range (reach 4) drawn for an Attack that is already spent, not a movement area. Minor.

## Defects and rough edges (most severe first)

1. No real #167 defect found. All four reviewer-named risks behaved as specified (disabled click, Berserker rejection with the tooltip kept, overlay restore, two-resource and longest names at 1600x600).
2. Attack reach contour stays drawn for a spent Action (`050`). Cosmetic.
3. At 1600x600 the dock's tags are about 7 px (legible, not generous).
4. Hover not delivered until the first click after launch (Quirk 2 above; probably rig).
5. A pointer click on an out-of-reach enemy in Frenzy mode showed no log line in the frame I took (`011`); not confirmed either way.

## Logs

Eleven game logs in `logs/`: 0 `Error:`, 0 ensures, 0 `LogMaterial`, 0 `Fatal`; the only `Warning:` in each is `Received SDL_EVENT_QUIT` at window close. One extra launch crashed at startup with the SDL SIGFPE (rig). No `Saved/Crashes` folder appeared. Clean shutdown each time.

## Not tested

SRD dock; Tab key; frame-by-frame comparison with the offscreen gallery on `evidence/pr167-b12981f` (the dock layout looked consistent with it but I did not diff); HUD coverage on screen; physical input; 1920x1080 for the Berserker/Wizard docks and the blocker states (all blocker shots are at 1280x720); Slot alone at 0.

`rig/` holds the nested-compositor scripts and pointer client. `SHA256SUMS` lists every file.
