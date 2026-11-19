# Journey checkpoint: release notes

For Austin, 2026-11-16. This is the plain-language summary of the journey checkpoint (spec [#239](https://github.com/AustinKelsay/dnd-diceroll/issues/239)), answering your playtest of the last release. It describes the release candidate `staging` `6a1613b7`. The checks behind it are in [integrated-acceptance](integrated-acceptance.md), and the ticket-by-ticket ledger is in the [run record](README.md).

Your playtest found three problems:
- the sound was synthesized tones;
- bodies trapped heroes;
- the little worlds didn't show, and there was no sense of the whole journey.

All three are addressed. So are the finale set piece, a polish bundle and a balance pass. Landon's SRD spells (#253) are not in this release.

Compact is still the default and only playable ruleset.

## What you will see and hear

**Bodies no longer trap anyone** ([#240](https://github.com/AustinKelsay/dnd-diceroll/issues/240), catalog revision 11; fixes [#217](https://github.com/AustinKelsay/dnd-diceroll/issues/217)). Movement now follows SRD 5.2.1:
- You can move through an ally, a Downed hero or a body, but you can't stop on an occupied square.
- A living enemy still blocks, so a front-liner still holds a doorway. Foes follow the same rule.
- Each square passed costs one movement, as before.
- Runs started before this release keep the old rule for life, so an old save plays exactly as it did.

**A new balance for revision 11** ([#241](https://github.com/AustinKelsay/dnd-diceroll/issues/241); [report](M2-balance-report.md)):
- Rescue restores a third of the hero's max HP, so a rescued hero isn't knocked down again at once as often.
- Foes spread their attacks instead of piling onto hero 1.
- Reserve and Honed now refill a hero's special pools, so specials still matter in fight 5.
- Monk and Paladin moved toward the pack.
- Foes hit a little harder, and the dangerous road's later fights have more HP.
- Under the test policy a run is won 76.8–78.4% of the time, depending on which cards it takes, with the finale the hardest fight. That is about where the last release was (75–79%).

**Sound** ([#242](https://github.com/AustinKelsay/dnd-diceroll/issues/242)–[#246](https://github.com/AustinKelsay/dnd-diceroll/issues/246)). Every sound is now a recording from CC0 libraries, cut and mixed per cue. Each source is listed in `docs/assets.md`.
- **Effects:** all 20 old cues re-recorded, many with several takes so repeats don't sound identical. There are also new cues for:
  - spending a Kit use, slot, potion or Rage;
  - protection soaking damage;
  - the start of a round;
  - the finale won;
  - the run lost;
  - the Waystone's flare and shatter.
- **Ambience per world:**
  - the Drowned Vigil: dripping stone and distant wind;
  - the Root of the World: leaves, wind and insects;
  - the Vault: a low hum;
  - the war table: candle crackle and room tone.

  Beds crossfade when the board changes.
- **Music by phase:**
  - a war-table theme;
  - two combat loops that alternate by fight;
  - a Vault finale track;
  - victory and defeat stings.

  Combat sounds briefly duck the beds and music.
- **Settings: volume,** on the start screen and in the in-run Menu (I), with sliders for Master, Effects, Ambience and Music. They work by keyboard and pointer. Volumes are saved for this computer, never in a run's save, and they persist across launches.

**The war-table journey map** ([#247](https://github.com/AustinKelsay/dnd-diceroll/issues/247), [#248](https://github.com/AustinKelsay/dnd-diceroll/issues/248)). Between stages the camera goes to a big map of all five stages on the war table.
- Cleared stages carry their world's emblem and a pennant (`1 Forest won`).
- The two offered worlds' dioramas are raised, lit and centred, each labelled with its route and risk word.
- Stages ahead sit under fog that lifts as you reach them. The Vault stands at the far end as the goal.
- Click a diorama, click a card, or press Z, X or Enter to choose.
- A defeat ends on the journey, showing where the party fell. A victory ends on the Vault board, with the journey one button away.
- The route screen after a fight is no longer blown out.

**The fly-in** ([#249](https://github.com/AustinKelsay/dnd-diceroll/issues/249)). Choosing a route flies the camera down into that diorama, dips to dark, and opens on the fight. It takes about 2 seconds. Any key or click skips it, and a skip can't act by accident in the fight. A resumed fight never replays it.

**The Waystone set piece** ([#250](https://github.com/AustinKelsay/dnd-diceroll/issues/250), for [#104](https://github.com/AustinKelsay/dnd-diceroll/issues/104)). Winning the fifth fight now has its moment before ARTIFACT RECOVERED:
- the Waystone flares;
- the camera turns to the apse;
- the rune seal shatters onto the dais;
- the artifact is lifted.

It is pre-authored, with no physics, and Tab skips it.

**Polish** ([#251](https://github.com/AustinKelsay/dnd-diceroll/issues/251), [#252](https://github.com/AustinKelsay/dnd-diceroll/issues/252)):
- The VICTORY and DEFEAT banners now draw after a fight, for about 2 seconds.
- After the camp is used, the claim line says "Choose the next route" instead of offering the camp again.
- The SLAIN text no longer lingers on the Reward screen.
- The enemy-turn banner no longer dims the hero cards.

## What was decided

- **Movement follows SRD 5.2.1,** shipped as catalog revision 11 together with the balance pass, so the rules change only once. Older runs keep their rules for life.
- **Two balance misses were accepted** (#241):
  - **No class has a clearly best reward card** ([#228](https://github.com/AustinKelsay/dnd-diceroll/issues/228)). The cards are even, but per-class card values need a catalog change. The next step is measuring one hero's card with the rest of the party held fixed.
  - **Quick re-downs after a rescue** fell from 27% to 22%, not to half ([#187](https://github.com/AustinKelsay/dnd-diceroll/issues/187) item 1). Rescue at half HP was rejected because it forced much tougher foes and longer fights.
- **The first click after start-up** ([#171](https://github.com/AustinKelsay/dnd-diceroll/issues/171)) is not a game bug that can be fixed in code. With no pointer motion since launch, the engine reads the cursor at (0,0). A real mouse moves before it clicks, so it may never happen to you; #171 stays open for a physical-mouse check.
- **#104 was rescoped** to the pre-authored Waystone set piece, without waiting for the destructible-terrain work (#99–#103).

## Your call

- **Post-rescue protection?** A short protection after a rescue is the lever that would halve quick re-downs (#187 item 1). It needs a new saved fact per hero, so it is a save schema change. Say whether you want it.

## What is not covered

- **Sound by ear.** Nobody has listened. The checks are measured loudness and peaks, clean imports, and the game's logs: every cue played its recording, the right bed played per world, and the right music played per phase. Your listen is the ear test.
- **The finale won by hand.** The test bot won fights 1–4 by real input twice, but never the revision 11 finale. The set piece was checked frame by frame through its capture mode, and the victory ending from a saved victory. Its two Waystone sounds have only their automated tests. Please win one.
- **A physical mouse and keyboard.** Every on-screen check used scripted virtual input on a headless test display: wheel, drags, hover feel, the first click (#171) and a real Shift key.
- **Human balance and fun,** including the #228 and #187 item 1 misses above.
- **Mac and Windows.** Nothing was run on either.

## How it was checked (at `6a1613b7`)

- **Gate:**
  - build with 0 warnings;
  - 451 of 451 automation tests with 0 warnings;
  - 325 helper tests on Python 3.11;
  - the contribution check passes;
  - a clean Development package.
- **Speed and memory,** two runs:
  - frame time 3.04–3.05 ms, GPU 2.66–2.67 ms, game thread 1.19–1.20 ms;
  - the budgets are 4, 3.5 and 1.5 ms;
  - memory 1286–1292 MiB, against 1.5 GiB.

  That is level with the last release. The load stutter between the map and a fight now falls under the fly-in's dip to dark instead of at your click. The journey map costs about 0.1 ms over the old overview.
- **Contrast:** all 70 builds pass on all three worlds, unchanged from the last release.
- **On screen:**
  - two runs through four fights each by keyboard and pointer, with 25 moves through allies and bodies;
  - the journey map at every fork, and the fly-in;
  - reward cards, camp and banners;
  - a defeat ending on the journey;
  - resume at every kind of stop, Abandon and Return to menu;
  - Settings from both places, persisting;
  - four window sizes;
  - an old revision 10 save keeping the old rule.

## Next

The lead opens the `staging` → `main` release PR. Then come your listen and playtest. After that, the open items are your post-rescue protection call, #228, #187, the #171 mouse check, and Landon's SRD libraries (#177) and spells.
