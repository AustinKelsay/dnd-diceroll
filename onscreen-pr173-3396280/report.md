# PR #173 (#158 combat hints) on-screen pass at `3396280` (partial)

Head `3396280af073ceec65d13215646cca8d57f78b49` (unchanged when packaged). Binary SHA-256 `a6cc651ef8f2d1dd8f970bdf279c98ee84760928944d564d588480aca5db47c7`; package 0 errors, 0 warnings. Nested headless Hyprland, `follow_mouse = 1`, **virtual** input. Lock 05:11Z to about 05:17Z, my game alone. 3 launches: 0 Error / ensure / LogMaterial / Fatal, clean closes; 1 startup SIGFPE (known rig race), relaunch fine. Sizes: 1920x1080 (fresh New Run, Watchtower road), 1920x1080 (all-spent Wizard fixture, Help/Menu/Records/return), 1600x600 (details). 1280x720 not run.

**This pass is partial.** My virtual pointer and key timing made several multi-step setups unreliable (swap/Sanctuary commits, Skip tips click at 1600x600), and I stopped rather than keep guessing. What was seen is below; the rest is stated as not tested.

| # | Item | Result | What was seen |
| --- | --- | --- | --- |
| 0a | Click on the callout panel (body, edge, between buttons) acts on nothing | **Pass** | `c110`: three clicks (body, left edge, gap between buttons): Move stayed 4, no move, hint still up, no preview. |
| 0b | With the keyboard cursor showing, click Got it: only the hint goes; then Enter by #172's rules | **Pass** | `sheet-120-121`: cursor ribbon, click Got it: hint gone, Move 4. `c122` + `chips-123`: first Enter only re-showed the ribbon (Move 4), the second Enter committed (Move 3). |
| 0c | D dismisses only the hint, the next Enter behaves by #172 | **Partial** | `sheet-140-142`: D dismissed only the hint with a pointer bracket resting on a cell (Move 3 unchanged). The D-then-click commit with a valid previewed cell was not validly exercised: both attempts rested the pointer on a non-reachable cell. A plain preview-then-click on a reachable cell (no hint up) committed first time (`c170`, `c171`: Move 0). |
| 0d | Shift+D pans and leaves the hint up | **Pass** | `sheet-221-227` frame 1: Shift+D moved the camera, hint still showing. D alone with no hint was not separately tested. |
| 1 | Fresh run: hint 1 on the first party turn at rest, not over the board or dock | **Pass** | `c100`: "Your turn: Click a glowing tile to move, or a marked enemy to attack. Arrows move the cursor and Enter does the same." with "D Got it" and "Skip tips", under the party cards, clear of the board and dock. Not shown during playback or enemy turns: not specifically checked. |
| 2 | Hint 2 (scarce resources) after spending a Kit; true wording | **Pass** | `c135`: after Quick Exchange, "Limited resources: Kit 1/2 is what you hold of the most you carry. Camp restores 1. Camp gives back only what the build's recovery says." Matches Kit 1/2. The two-Potion trigger was not tested. |
| 3 | Hint 3 (Downed and Rescue) in real play | **Not tested** | No ally went down in this session. |
| 4 | Hint 4 (only End Turn) names Space | **Not tested** | I could not reach a one-button state on a fresh run in time. The resumed all-spent fixture (`c200`) has only End Turn left and correctly shows **no** hint (Resume arms none). |
| 5a | Resume shows none | **Pass** | `c200`: resumed turn with only End Turn left: no hint. |
| 5b | Replay tips in Help re-arms them | **Pass** | `sheet-201-202`: Help lists "D Dismiss the tip on screen" and a "Replay tips" button; Tab, Enter: Help closes and hint 1 returns (`c210`). |
| 5c | Skip tips, then New Run brings them back | **Not tested** | My Skip tips click at 1600x600 missed (`sheet-310-311`: hint still up). |
| 6 | Hidden under Help, Records, the Menu, the return prompt; nothing dismissed while hidden | **Pass** | `sheet-221-227`: the hint is gone while Menu, Records and the return prompt are open and is back after each closes; D pressed in the Menu left it intact (back on close). Help hides it (`sheet-201-202`). |
| 7 | Each hint at most once per run | **Partial** | Hint 1 and 2 each appeared once in the fresh run; the rest were not reached. |
| (d) | 1600x600 with hero details open: callout vs details | **Pass** | `sheet-300-301`: the callout sits under Hero 4's card, the details panel for Hero 2 sits to the right of the cards; they do not overlap; the callout bottom sits just above the log tab. |
| Normal | Hover-then-click on a reachable cell commits first time with no hint | **Pass** | `c170`/`c171`. |

Counts: **Pass 8 / Partial 2 / Not tested 4** (hints 3 and 4, Skip-then-New-Run, two-Potion trigger). No defect found.
