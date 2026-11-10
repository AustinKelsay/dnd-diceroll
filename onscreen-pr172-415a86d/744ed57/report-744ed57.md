# PR #172 pass on head `744ed57`

Head `744ed57bddb2fe310bc80c4aff4ce5e5ff9bc09f` (unchanged when packaged). Binary SHA-256 `9651770c4f49ab20ce0848596e65d1f49a3a069f0f840b3017e2d94c022c81e5`. Nested headless Hyprland, `follow_mouse = 1`, **virtual** input, 1920x1080, board-18x12 fixture (Watchtower road, Nix to act). Lock 04:14Z to about 04:25Z, my game the only Unreal process. 3 launches, 0 Error / ensure / LogMaterial / Fatal, clean closes. Same rig limits as before (clicks cannot be timed exactly against keys; capture slows the game).

| # | Item | Result | What was seen |
| --- | --- | --- | --- |
| 1 | 4b: preview resting, then Q/E, click while the camera eases | **Pass for Q at +0.10 s, E at +0.25 s, Q at +0.50 s** | Move stayed 4 after each (`chips-410-412`, `chips-420-430`; mid-ease frame `411-Q-d0.10-mid` shows the camera moving). No commit while it eased. **Pan and wheel zoom: not validly exercised**: my Shift+D hold / wheel attempts either did not register or the frames showed no motion (`sheet-440-443-invalid...`, `sheet-450-453-pan-no-camera-motion`); one earlier pan did move the camera but the click was not timed inside it. |
| 1b | A click after it settles and a fresh preview shows commits on that cell | **Pass** | `c472` (preview "Move 4 squares · 0 left after" on the new cell under the pointer) then one click: Move 0 (`chips-473`). |
| 2a | Settled camera: hover then click commits first time | **Pass** | `c472`/`chips-473`. |
| 2b | One Enter with a visible, unchanged keyboard cursor commits | **Pass** | `c491` ribbon, then Enter: Ivo moved (`sheet-492`). |
| 2c | New hero's turn: after the camera settles, hover and click commits (no ignored click) | **Pass** | Arden's turn: `c481` preview "Move 3 squares", one click: Move 1 left (`chips-482`). |
| 3 | 4a: arrow+Enter, then arrow+Enter again quickly | **Pass (consistent)** | `sheet-492-4a-744ed57`: first walk plays in frames 6-8, then the cursor/preview returns, and only then the second Enter commits one more square (Move 2 left of 4). The second move did not start during the first walk. |
| 4 | Still pointer while orbiting: the preview follows the cell under the pointer, no click | **Pass (limited)** | `sheet-411-412`: after Q, both the mid-ease and settled frames show a path preview for the cell under the pointer on the rotated board ("Move 3 squares"). I did not check pan or zoom, nor that each cell identity is correct frame by frame. |

Counts: **Pass 7 / Partial 0 / Not validly tested 1 (pan and wheel variants of 4b)**. No defect found on this head.
