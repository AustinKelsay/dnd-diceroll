# PR #173 session 2 (head `3396280`, same package, binary `a6cc651e...b47c7`)

One locked session, 05:19Z to about 05:30Z (my game alone), 1920x1080, virtual input, nested compositor `follow_mouse = 1`. Fixtures `downed-ally-battle` and `end-turn-only-battle` generated in this checkout with `Project.Tools.CaptureFixtures`. Logs: 0 Error / ensure / LogMaterial / Fatal, clean closes.

| # | Item | Result | What was seen |
| --- | --- | --- | --- |
| 3 | Hint 3 (Downed and Rescue) shows with the right turns left | **Pass** | Resume (no hint), Help, Replay tips: queue shows hint 1 first, D, then "Arden is Downed. An adjacent ally can Rescue (R) within 2 turns, or they die." The party card says "DOWNED · 2 turns to rescue" (`c401`, `sheet-402-403`). |
| 4 | Hint 4 (only End Turn left) names Space | **Pass** | `end-turn-only-battle`, Replay tips: queue is hint 1, "Limited resources: Kit 0/2 ...", then "Ending a turn: Nothing is left but End Turn. Press Space to end it." (`sheet-421-423`). |
| 4b | Resource hint wording true for the kit | **Pass** | "Kit 0/2 is what you hold of the most you carry" matches Hero 1's Kit 0/2 (`sheet-421-423`). |
| 5c | Skip tips, then return and start a New Run: hint 1 shows again | **Pass** | On a fresh New Run (`sheet-500-502`) hint 1 appeared; the Skip tips click (aimed from a full-size shot at (231, 610)) removed it (`sheet-503-504`). Backspace, Enter returned to the start screen, where New Run is refused while a checkpoint exists (`c510`: "A saved expedition is unfinished"); I moved my own isolated checkpoint file aside to start the New Run in the same session (`c511`), and hint 1 came back on its first party turn (`c512`). Note the moved-aside file is the only deviation from real play. |
| 2c | Two Potions: resource hint names the Potion | **Not tested** | Every hero in these fixtures has "Potion 1": no two-Potion hero was quickly reachable. Covered by the automated keyboard test, per the coordinator. |

Combined with session 1: **Pass 12 / Partial 2 / Not tested 1 (two-Potion trigger).** No defect found.
