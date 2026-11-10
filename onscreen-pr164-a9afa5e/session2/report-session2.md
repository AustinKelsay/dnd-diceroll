# PR #164 session 2 (head `a9afa5e`, same package, binary `f34e36c3...4bad6`)

Same package and rig as session 1. One locked session, 01:13Z to about 01:30Z (my game the only Unreal process). Virtual input. Frames come from a region-grab loop (`rig/rburst.sh`, about 10 to 20 frames per second). 1x speed for items 1 and 2 at 1920x1080. Logs: 0 Error, 0 ensure, 0 LogMaterial, 0 Fatal; only the close SDL_EVENT_QUIT warning; one startup SIGFPE (known rig race), relaunch fine. Head re-checked before starting: unchanged.

## What was asked, and what I saw

| # | Item | Result | What was seen |
| --- | --- | --- | --- |
| 1 | Walk cycle in motion, cell to cell, feet on board, no sliding (fighter and raider at least) | **Pass at 1x** | Raider (rigged, in the open): a stride with the feet on its ring, an axe swing afterwards (`sheet-032-raider-walk-zoom2`, `sheet-030-raider-walk-1x`). Fighter Arden: three-cell walk, about 19 fps, the figure and its ring travel together and stop on the target cell; no sliding past the end cell, no T-pose (`sheet-042-arden-walk-1x`, `sheet-043-arden-walk-zoom`). Wizard Ivo: walks 2 cells smoothly (`sheet-020`, `sheet-021`; under the silhouette fill, so his feet are hard to see). At 1280x720 a 4-cell Ivo walk was captured (`sheet-210-1280-ivo-walk`) but the frames are too small to judge feet; I do not claim it. Fast speed: **not captured** (see 7). |
| 2 | Wizard Fire Bolt adjacent and at range: Cast | **Partial** | At range (2 cells), 1920x1080: Ivo thrusts an arm and a fire flash lights the board, `-2` (`sheet-068-firebolt-ranged-cast-zoom`): Cast, no weapon swing. **Adjacent Fire Bolt**: the attack resolved (Raider 2 went 12/12 to 6/12, `sheet-217`) but my frames missed the animation (it played between grabs), so I did not see the adjacent clip. Not claimed. |
| 3 | Recover when a downed hero is rescued | **Partial** | Arden rescued downed Nix: `BACK ON THEIR FEET`, Nix at 1/22 (`sheet-125-rescue-recover-1280`, `sheet-127-recover-zoom`). **Nix is a static rogue**, so no rigged Recover clip is involved. A rigged hero (fighter or wizard) was not downed in time, so the rigged Recover clip was **not seen**. Also: I let Sera (static cleric) expire by passing turns; this was my mistake, not a defect. |
| 4 | Finish / skip / instant mid-playback converges | **Partial** | Tab (skip) pressed about 1 s into an enemy phase: the frame right after the skip equals the settled frame (`sheet-131-skip-1280`, frames 5 and 6): every visible figure was already in its final place and pose. The Finish command itself was not found separately; Arden's own commit in that run did not execute. |
| 5 | Telekinetic Thrust (Psi Warrior), actor clip isolated | **Partial** | 1280x720, a cursor on Raider 1 about 2 cells away: `Auto | 5 + push if open`, the raider is pushed (`Repositioned`). Region burst on Arden only (`sheet-112`, `sheet-113-thrust-arden-zoom`): Arden extends an arm and hand toward the target while standing; **no sword swing**, no step. That is a Ranged/point gesture, not Melee. It does not look like the staff-raise Cast of the wizard. I cannot read the clip name from pixels, so I say "consistent with Ranged", not "confirmed". Normal and fast speed separately: not done. |
| 6 | Main items again at 1280x720 | **Partial** | Idle and silhouettes: a rigged party silhouette (solid fill) is visible behind a pillar at 1280x720 on the Watchtower road (`200-1280-cursor`, `106-state-1280`, `104-state-1280`); the guarded-route and fallback frames from session 1 were at 1280x720. Melee, Cast and Hit at 1280x720: **not captured** (the attacks resolved but I missed the animation frames). |
| 7 | Items 1 and 2 at normal and fast speed | **Normal only** | Speed cycles 1x, 2x, Instant with `T` (`sheet-speeds`). Items 1 and 2 were captured at 1x; I did not repeat the walk and the casts at 2x or Instant. |

## Combined counts for #164 (session 1 + session 2)

Pass 8 / Partial 8 / Fail 0 / Not tested 2 (the rigged Recover clip, and fast speed). Nothing seen contradicts the PR; no new defect. Items still open: adjacent Fire Bolt clip, rigged Recover, Finish as a command, the Thrust clip name, fast speed, and Melee/Cast/Hit at 1280x720.

## Observations

- At 1x the raider idle is almost still; the walk cycle is clearly distinct.
- A click on a grid cell often does nothing in this rig (the first pointer action after a change is swallowed); I used keyboard navigation for aiming.
