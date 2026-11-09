# On-screen acceptance pass: #143 camera and lighting (PR #159), #146 compact HUD, #142 contextual grid

**Evidence only. Never merge this branch.** PNGs go through Git LFS.

## What was tested

| | |
| --- | --- |
| Source | PR #159 head `7a0c6ef3d7fac0bfd8529f8aeb62f6919ea9b338` (`feature/143-camera-lighting`; `gh pr view 159` re-checked just before packaging and again after: unchanged). It contains staging with #146, #142, the action-facts query and the SRD ruleset, plus #143. |
| Package | `Builds/20261109T225039957106Z/Linux/DnDDiceRoll/Binaries/Linux/DnDDiceRoll` (`tools/dev.py --cores 2 package`, under the machine lock; package log: 0 errors, 0 warnings, startup map reached, normal exit) |
| Binary SHA-256 | `91cd6d89b4c55f660913b4f2eb520c2e8157d0ba374ff23240d6373043376d57` |
| Host | PlebDev Ultra, UE 5.8.4, RTX 4080 SUPER, Vulkan SM5 |
| Display | **A nested, headless Hyprland instance** (nothing appeared on the owner's desktop). Client sizes 1920x1080, 1280x720 and 1600x600, exact. |
| Input | **Virtual, not physical.** `wtype` (virtual keyboard; Shift/Alt as real press and release) and a Wayland `zwlr_virtual_pointer_v1` client extended with right/middle drag and wheel (`nested-compositor/ptr2.c`). Both addressed only the nested compositor. |
| Saves | Isolated `-UserDir`, `-ExpeditionSaveRoot` per run under `.local/onscreen/runN`. Fixtures `board-16x11` and `board-18x12` (seed 3, offers 0 and 1) were generated in this checkout and copied into each run's own save folder, then resumed with `O`. The owner's saves were not touched. |
| Runs | 9 launches (see `launch-command.txt`). All closed through a normal window close (`closewindow`), never a signal. |

## Nested compositor: it works, reuse it

`nested-compositor/start.sh` + `hyprland.conf` + `lib.sh`:

1. `env -u WAYLAND_DISPLAY -u HYPRLAND_INSTANCE_SIGNATURE HYPRLAND_HEADLESS_ONLY=1 AQ_BACKENDS=headless LIBSEAT_BACKEND=noop setsid Hyprland -c nested/hyprland.conf`. **`LIBSEAT_BACKEND=noop` is required**; without it `CBackend::create()` aborts.
2. It binds a new socket (`wayland-2` here; the script diffs `/run/user/1000/wayland-*`). Its instance signature is the newest dir in `/run/user/1000/hypr`.
3. In that instance (`HYPRLAND_INSTANCE_SIGNATURE=<nested>`): `hyprctl output create headless`, then `hyprctl keyword monitor HDMI-A-1,disable` and `... DP-2,disable` (the instance also lists the real connectors as 0x0 outputs because `HYPRLAND_HEADLESS_ONLY` does not stop it enumerating DRM; it holds no DRM master so it cannot drive them, but disabling them is needed or `grim` fails with "failed to create buffer"). Set the size with `hyprctl keyword monitor HEADLESS-1,<w>x<h>@60,0x0,1`; with no gaps or borders the tiled game window fills the output exactly (client size == requested size).
4. Everything for the game, `wtype`, the pointer client and `grim` runs with `WAYLAND_DISPLAY=wayland-2` and the nested signature. Focus is the only window, so keys can only reach the game.
5. Gotchas: (a) `wtype` drops the first key unless it is given `-s 250`; (b) `grim` captures the output, so crop is not needed; (c) a pointer-client process that exits releases the pointer, and Unreal then forgets hover, so hover screenshots must be taken while the client is still alive (`ptr ... sh "grim ..."`, `hshot` in `lib.sh`); (d) the pointer client needs a ~1.4 s warm-up (`s 1400`) before its first motion; (e) Unreal needs several wheel notches: zoom is 120 units per notch over 900..6400.
6. **Intermittent startup crash on this rig, not a game defect I can attribute:** 2 of 10 launches (run4 first launch, run9 first launch) died in window creation with `SIGFPE` in SDL `handle_wl_output_done` (SDL_waylandvideo.c:1016) because the headless output advertises 0x0 mm physical size. An identical relaunch worked every time. Logs: `run4-first-launch-crash.txt`, `game-run9-first-launch-sigfpe.log`. A real monitor reports a physical size, so I do not expect this on the owner's session, but it should be re-checked on a real output.
7. Stale `/run/user/1000/hypr/<sig>` dirs remain from my failed start attempts (timestamps from 2026-11-09 ~16:46 on). **I also, by mistake, deleted every file matching `~/.cache/hyprland/hyprlandCrashReport*.txt` once while cleaning up my own crash report; that folder was empty afterwards, so any older Hyprland crash reports there are gone.**

## #143 camera and lighting (PR #159)

Pass 7 / Partial 3 / Fail 0, plus one framing observation.

| # | Item | Result | What was seen |
| --- | --- | --- | --- |
| 1 | Q/E orbit at default zoom, board stays clear of party cards, top strip, dock and log. 16x11 and 18x12, 1920x1080, 1280x720 (18x12 also 1600x600) | **Pass** | `sheet-b16-grid-yaw4-7-1920`, `sheet-b16-orbit-grid-1280`, `sheet-b18-grid-A-1920`/`B`, `sheet-b18-orbit-1280`, `sheet-b18-orbit-1600x600`. Eight yaws (45 degree steps) on 16x11 at 1920, the four axis yaws at 1280, 18x12 at all four axis yaws at all three sizes: the full board (grid shown with Alt) is inside the free area every time. 16x11 `Stream trail`, 18x12 `Watchtower road`. |
| 2 | Orbit by right-drag | **Partial** | `sheet-rd3-1920`, `139b-rd-none-1920`: right-drag orbits (about 0.35 deg/px) on 16x11 at 1920. The first drag in one sequence had no effect (`sheet-orbit-pan-1920` frames 2 and 3 are identical to the start) while the second and later drags worked; not reproduced further. Right-drag was **not** exercised at 1280x720 or on 18x12. |
| 3 | Board stays clear while the camera eases | **Partial** | `sheet-b16-orbitE-mid-1920`: four frames about 0.12 s after a Q/E press, on 16x11 at 1920: the board is contained mid-ease. I took the matching mid-ease frames at 1280x720 on both boards but did not inspect them, so I do not claim them. |
| 4 | Wheel zoom (three stops, about 35/45/60 degrees) | **Pass** | `sheet-zoom-stops-1920`, `sheet-zoom-1920`, 1280 series: zoom is continuous (120 units per notch), pitch follows it: the near end is a low angle with figures seen from the front (`120-near-1920`), default is about 45, the far end is a steep overview (`121-far-1920`) and does not zoom out past the clamp. Angles were not measured numerically; judged by eye. |
| 5 | A deliberate zoom survives an orbit | **Pass** | `sheet-zoom-survives-orbit-1920`: zoomed in, 2xE, 2xQ: frames 1 and 3 match, the zoom is kept through the orbit. |
| 6 | Shift+WASD pan, bounded | **Pass** | `133..135`, `sheet-orbit-pan-1920`, `sheet-b16-pan-series-1280`: Shift+D, Shift+W+D and Shift+S (3 s holds) move the board and stop at a bound; held longer they do not leave the world. See defect 4 about how it looks at the bound. |
| 7 | Middle-drag pan, bounded | **Pass** | `sheet-middledrag-1920`: drag left and up pans; again bounded. |
| 8 | Plain A still selects Attack while Shift pans | **Pass** | `sheet-A-attack-key-1920`: F selects Quick Exchange, then plain `A` returns to Attack / Move; camera unchanged (image diff 0.5% of pixels, the highlight only). |
| 9 | Pan re-centres on the active unit at turn change | **Pass** | `143-turn-changed-recentered-1920` (before: panned; after End Turn: the active hero is in the centre again). |
| 10 | Root boards at the home yaw: no popping of clipped scenery as the camera moves, shadows present | **Partial** | `sheet-b16-pan-series-1280` (6 frames over a 2.6 s pan), `sheet-b16-panW-series-1280`, `sheet-b16-zoomin-series-1280`, `sheet-b16-zoomout-series-1280` (8 frames each, 0.25 to 0.45 s apart): scenery fades in and out smoothly, no frame shows a pop or a missing prop, shadows of walls, towers and braziers are always present. These are stills at about 3 to 4 fps, so a one-frame flicker would not show. |
| 11 | Framing impression on 18x12 at yaw 0 and 180 (`200-b18-home-yaw-1920`, `203-b18-yaw4-E-grid-1920`, `sheet-b18-grid-A-1920`) | **Observation** | Yes, a bit far. At 1920x1080 a figure is about 50 px tall and the party reads as a cluster of small dots; HP bars and chips are legible but figures are not. At 1280x720 and 1600x600 (`500-b18-home-1280`, `600-b18-home-1600x600`) a figure is about 30 px tall. At yaw 90 and 270 it is worse (the board is rotated to fit the free area): the 18x12 board fills the screen but each unit is a 25 to 35 px speck (`502-b18-yaw1-1280`, `sheet-b18-orbit-1600x600`). A nearer default for the active unit is worth a look in the framing call. |

Also seen: the `Watchtower road` floor renders as an extremely saturated blue (`014-watchtower-1920`); that is art, not this PR, but it is a strong first impression.

## #146 compact HUD (PR #155)

Pass 9 / Partial 3 / Fail 0, 1 not tested.

| # | Item | Result | What was seen |
| --- | --- | --- | --- |
| 1 | Start-screen builder drawers, every class including 10-subclass classes | **Pass** (1920, 1280) / not tested at 1600x600 | `010-class01..12-1920`, `sheet-class-1-4/5-8/9-12`: all 13 classes (Fighter, Rogue, Cleric, Barbarian, Monk, Paladin, Ranger, Sorcerer, Warlock, Wizard, Druid, Bard, Artificer) at 1920, the lists in two columns, the left drawer never runs past the screen. 1280x720: `sheet-builder-1280`, `402-wizard-1280`: Cleric (10), Wizard (10), Bard (4), Warlock (5), all inside the 720 px drawer, the last row about 20 px from the bottom. Wizard and Cleric are the tightest. |
| 2 | Party cards; acting hero's details by keyboard and by click | **Partial** | Hover on a card shows its full status (`057-card-hover-ivo-1920`, `705-arden-hover-1280`). Pressing `2` toggles the acting hero's details open and closed (`sheet-key2-toggle-1920`). Clicking the acting hero's card opens its details (`sheet-card-click-1280`, third frame). Partial because after **clicking a non-acting hero's card** (`706-arden-click-1280`) Arden's hover card disappears while the pointer is still over it, and the previously opened Nix card stays; I could not tell if a click on a non-acting card is meant to toggle. |
| 3 | Menu panel: I, Tab, arrows, Enter, Esc | **Pass** | `sheet-menu-nav-1920`: I opens it with Save focused; Tab moves to Resume; Down to Records; Esc closes. Enter on an item runs it (`045`). Mouse: the top-right button opens it and a click on Records opens Records (`417`, `418`, `sheet-menu-click-item-1280`). |
| 4 | Each hotkey with the panel closed | **Pass** | `sheet-hotkeys-closed-1920`: H opens Records, `?` Help, T changes speed (panel shows `T Speed 2x`), K saves (log line). Esc closes each. |
| 5 | Each hotkey with the panel open | **Pass** | `sheet-hotkeys-open-1920`: K, T, H and `?` each close the panel and act; gameplay key `2` leaves it open; Enter on an item closes it. |
| 6 | Focus restored on close | **Partial** | After Esc the highlight is back on the Attack / Move button (`046-after-esc-1920`) but I did not diff it against the pre-open focus. |
| 7 | Combat log fade and corner tab | **Pass** | `sheet-log-fade-1920`: the log fades about 4.6 s after the last entry and leaves a small `Log` tab bottom left; the pointer on the tab brings the log back (`056-log-tab-hover4-1920`). The pointer only holds if the pointer client is still alive when you screenshot (rig note 5c). |
| 8 | Action dock and Move / Action / Bonus chips | **Partial** | The dock draws at all sizes, 6 buttons with key and cost line, the three chips in the top bezel, hints (`Sneak Attack: with adjacent ally`) inside the bezel. After moving 4 cells the chip reads `Move 0` and goes grey, the log says `Nix moves 4 cells (0 left)` (`613-after-kbd-move2-1600x600`). **Spending an Action or a Bonus was not tested.** |
| 9 | Nothing clipped or overlapping with 7 combatants (Watchtower road) | **Pass** | `014-watchtower-1920`, `411-watchtower-1280`, `600-b18-home-1600x600`, `610...`: the initiative strip, Menu button, party cards, dock and log never overlap; in `411` the full strip `R2 N S R1 I V A` is on the left with the Menu button alone top right. |
| 10 | Earlier defect: top bar label clipped to "ksp Menu" | **Pass (gone)** | The button reads `I Menu` in full in `014`, `411`, `412`, `600`. |
| 11 | Earlier defect: Sneak Attack hint under the combat-log corner | **Pass (gone)** | The hint now sits in the dock's top bezel at every size (`411`, `600`), clear of the log. |
| 12 | Return prompt | **Pass** | `415-return-prompt-1280`: Backspace opens `RETURN TO MENU?` (Enter = return, Esc = keep playing), the HUD dims; Esc closes it and the HUD and chips are restored. |

## #142 contextual grid (PR #160)

Pass 7 / Partial 3 / Fail 0, 1 not tested.

| # | Item | Result | What was seen |
| --- | --- | --- | --- |
| 1 | Grid hidden at rest | **Pass** | `sheet-enemy-1920`: during an enemy turn the board has no lines at all; only figures, rings and the enemy's bar. On the party's turn only the acting hero's reach contour and the faint corner ticks show. |
| 2 | Reach contour | **Pass** | Visible in every combat shot as a solid outline of the reachable area with corner ticks. |
| 3 | Path ribbon, cardinal corners, destination bracket, movement, by keyboard | **Pass** | `072-kbd-cursor-left2-1920`: after two Left presses a ribbon runs to a gold destination bracket, one tick per step, a `2` cost label, and the panel reads `Move 2 squares - 2 left after`. |
| 4 | The same by pointer | **Partial** | `078-pointer-path-2-1920`: ribbon with its corner and the bracket show under the pointer. But the **cost number is covered** by the cell panel and chips when the target is to the lower right of the pointer, and after any arrow key the pointer hover does nothing until a click (defect 1). |
| 5 | Targeting: reach, target bracket, forecast chip by pointer | **Pass** | `082-target-hover-1920`, `411-watchtower-1280`: enemy under the pointer gets the target bracket and a name-led chip (`Raider 2: 70% | 4-9 | crit drop`), the reach is outlined. |
| 6 | Targeting by keyboard | **Not tested** | I used the keyboard cursor for movement only. |
| 7 | Footprint rings | **Pass** | Hollow rings lie flat under every figure, selected one brighter (`100-b16-default-yaw-1920`, `120-near-1920`). |
| 8 | Hold-for-full-grid key (Alt) | **Pass** | `sheet-alt-1920`, `103-b16-yaw4..7-grid-1920`, `201`, `311-*`: holding Alt (a real key press) shows every cell line; releasing hides it again; focus stayed in the window; Alt+Enter was not tried. |
| 9 | Path-cost number readable | **Partial** | Readable when not covered (`072`); see 4. At 1280x720 it is about 9 px high and fine. |
| 10 | `-ExpeditionPrimitives` look | **Pass** | `sheet-primitives-1280`: the primitive board (green grass, brown cell tiles, blue reach fill with dots, base discs) is unchanged and unaffected by the grid work; the HUD above it is the compact one. |
| 11 | Sizes | **Partial** | 1920x1080 and 1280x720 throughout; 1600x600 only for the 18x12 board (no path ribbon or targeting looked at there). |

## Defects, most severe first

1. **Pointer hover is ignored after an arrow key until you click, and the click then acts.** `073-pointer-path-1920`, `074`, `075`, `076` (`073` to `076`). Steps: Watchtower road, Nix to act; press Left twice (the keyboard cursor and ribbon appear); move the pointer to another reachable cell: the keyboard ribbon stays, nothing follows the pointer; Esc and `A` do not clear it. Only a click clears it, and that click acts on the cell under the pointer. 1920x1080 (the logic is in `ExpeditionPlayerController.cpp`, `bKeyboardCursor` is only cleared on click).
2. **Path-cost label can be hidden by the cell panel and chips** with pointer hover. `078-pointer-path-2-1920` (cost under the `Nix - Rogue` panel). 1920x1080, hover a reachable cell to the lower right of the active hero.
3. **Clicking a non-acting hero's party card makes its hover card disappear.** `705-arden-hover-1280` then `706-arden-click-1280`: with the pointer still over Arden, the card is gone; Nix's earlier card stays. 1280x720.
4. **Pan bound lets the board slide into a corner with half the screen black.** `sheet-b16-pan-series-1280` (frames 4 to 6), `139b-rd-none-1920`. Shift+S held 2.6 s at 1280x720 on 16x11, home yaw: the board ends up in the upper third with a black void below. It is bounded as specified; the margin looks generous.
5. **Right-drag: the first drag of a sequence did nothing once** (`sheet-orbit-pan-1920`, frames 2 and 3 equal frame 1) while later drags worked. Not reproduced; may be my pointer client.
6. **The legend line** (`Outline Reach / x Blocked sight / Ring Legal target / AREA Victim`) floats unbacked in the middle top of the board and sits under the unit panel at 1920 (`014`, `046`). Cosmetic.
7. **18x12 default framing is far** (observation 11 above).
8. Rig only: the SIGFPE in SDL on the headless output (above).

## Logs

Nine game logs are in this folder (`game-run1..9.log`, plus the two first-launch crash logs). Across all nine completed runs: 0 `Error:`, 0 ensures, 0 `LogMaterial`, 0 `Fatal`; the only `Warning:` in each is `LogLinuxWindow: Warning: Received SDL_EVENT_QUIT, requesting engine exit.` at the window close. Every run exited after a normal window close, no crash folder was created (`Saved/Crashes` is absent).

## Not tested

Audio, physical keyboard/mouse, long-play, frame time, Mac/Windows, the real route table and start screen camera behaviour for #143 ("lose no geometry", I saw the start screen and a route screen only for a glance: `001-title-1920`, `410-route-1280`), Alt+Enter, keyboard targeting, Action and Bonus chip spending, builder at 1600x600, right-drag at 1280x720 and on 18x12, mid-ease frames at 1280x720.

## Files

`sheet-*` are contact sheets made from the numbered raw shots; the individual numbered shots here are the ones cited. `notes.md` is the running log. `nested-compositor/` is the rig. `SHA256SUMS` lists every file.
