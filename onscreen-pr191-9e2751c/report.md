# PR #191 (#190 roll card placement) on-screen pass at `9e2751c`

Head `9e2751cb90c2e6f4713c77157e4b111a8831c6fb` (unchanged when packaged; the coordinator says a fix round will move it). Binary SHA-256 `495ccc85abbc3fc23309d62fd6e63fbf7b5c755db0892ffe357f021e2d1ff367`; package 0 errors, 0 warnings. Nested headless Hyprland, `follow_mouse = 1`, virtual input. Locked session 09:23Z to about 09:45Z, my game alone. Three launches (1920x1080, 1600x600, 1280x720), Watchtower road, fights played by my key script (`rig/play.py`); rolls captured as frame sequences at about 6 fps (`rig/fast.sh`). 0 Error / ensure / Fatal. **Not covered: all four yaws, spells and healing rolls, 1280x720 beyond the zoomed/panned view, a controlled pass with the party at three board positions.**

## The three findings you named
| | Seen? | What |
| --- | --- | --- |
| (a) hint returning under the still-visible roll card | **Yes, confirmed** | At 1920x1080, after Sera's attack resolved (`s/HINT-vs-card.jpg`, three frames from the same roll): the "Your turn" panel (title, text, "D Got it" and "Skip tips") is drawn again at the same spot and shows through the roll card as faint ghost text while the card is still up. The card is drawn on top but is translucent enough to read the hint under it, and the dice/HIT line collide with it visually. Same at 1600x600 (`s/R1600-card-crops.jpg`, hint visible in the frames just before the card appears). |
| (b) hero 4 details overlapping the card | **Touching, not overlapping, for a Wizard** | With Ivo selected (key `4`, twice), his 3-line details panel sits directly right of card 4 and ends about 8 px above the roll card's top edge (`s/HERO4-details-vs-card-rect.png`, left: the details panel and a roll card up together). A taller panel (more lines, for a Barbarian or a kit with longer text) would overlap. My pointer hover over card 4 did not open the panel (virtual-pointer quirk), so I used the keyboard. |
| (c) the d20 rising above the card into party card 4 | **Yes, confirmed at 1920x1080 and 1600x600** | At the start of a roll the tumbling d20 first appears with its top edge over the card's top border, partly over card 4's bottom row and over the title text "Sera attacks Raider 2" (`s/ROLL-d20-rise.jpg`, six consecutive frames at 1920x1080; `s/R1600-card-crops.jpg` frame 4 at 1600x600, the 17 sits on the card's top edge). It settles inside the card about 3 frames (0.5 s) later. |

## Checklist
| # | Item | Result | What was seen |
| --- | --- | --- | --- |
| 1 | Attack: card under the party cards, dice land inside, verdict, readable arithmetic line, fades | **Pass with the flaws above** | Card sits in the left margin directly under the party cards at 1920x1080, 1600x600 and 1280x720; the d20 and damage die land inside, then "HIT" (or "NATURAL 1 — MISS", "MISS") and the arithmetic line ("d20 10 + 5 = 15 vs AC 13 · damage 5 + 3 = 8") read fully and fit; the card fades after the next action. Fade time not measured. It never covered a unit or the cells between attacker and target in any frame I looked at (figures are at the right of the card in all sizes). |
| 2 | All four yaws | **Not tested** | Only the default yaw. |
| 3 | Spells and healing | **Not tested** | Only basic attacks (Cure Wounds fired in earlier passes but was not looked at here). |
| 4 | Hover over party cards with a card up | **Partial** | See (b). Pointer hover itself was not delivered in my rig. |
| 5 | Chips and the dock | **Pass** | Target chips and Skip button positions unchanged; "Skip >> Tab" replaces the dock during the roll (`s/R1280-zoomed-left.jpg`, bottom row). |
| 6 | Zoomed in and panned left | **Pass, acceptable** | 1280x720, near zoom, Shift+A held 2.5 s (`s/400-zoomed-panned-left.png`, `R1280-zoomed-left.jpg`): the board runs under the card, so floor tiles show around and behind it, but no unit stood under it in these frames and the card's opaque dark panel keeps the text readable. Mild: it hides some floor at the left margin. |

Combat log expanded: not tested. The roll card and the log are far apart in every frame I saw (log at the bottom-left, card below the party cards).

Counts: **Pass 3 / Partial 2 / Not tested 3; two of your three suspected defects confirmed (a, c), one conditional (b).**
