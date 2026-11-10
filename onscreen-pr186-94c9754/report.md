# PR #186 (#184 Compact balance tuning) play check at `94c9754`

Head `94c9754d5caea2d45197fe878a928d18cc5b88d7` (unchanged when packaged). Staging package from `871185a` (contains `c07e41f`): SHA-256 `d5380893bbc4ab5307dcc7e4877928492bd327dde841f912ef1a65cd66ee917d`. #186 package: `61bff320bdf927c7432f9246d49e6cb1402a467658381f41e3db1a8a72a4bdba`. Both 0 errors / 0 warnings. Nested headless Hyprland, `follow_mouse = 1`, **virtual** input, 1920x1080, normal speed (1x; the only speed key press was toggling back to 1x). Lock held 08:05Z to 08:28Z, my game alone. Familiar party (Arden Fighter/Champion, Nix Rogue/Thief, Sera Cleric/Life, Ivo Wizard/Evocation), seed 20261030.

**How the fights were played:** a small script (`rig/play.py`) pressed the real keys (`wtype`: A/F/R, arrows, Enter, Space) and read the checkpoint the game writes after every accepted action to decide the next key. Policy: attack the nearest conscious enemy (moving toward it first over free cells), Cure Wounds on an ally below half HP, Rescue an adjacent downed ally, otherwise end the turn. It is a simple, fixed policy, not a player: it never used Sanctuary, Sculpted Burst, Second Wind, Potions or Rage and did not focus the archer. Read the rounds and losses below as that policy's, not a human's. A first attempt had a cursor-tracking bug (attacks did not land); its log is kept as `play-logs/play-fight1-VOID-driver-bug.log` and was not used.

## 1. Old save keeps the old enemies: **Pass**
- Staging package (rev 7): New Run (G, N, route Z, Enter), moved one hero into fight 1 (Stream trail), pressed K (checkpoint), quit. Checkpoint: `CatalogRevision 7`; enemies Scout 1 and Vault bow 10/10 HP, AC 11, +2, d4+1 (`before-dump.txt`, SHA-256 `962afc66...2e22138`).
- #186 package on the same save root, Resume (O): the checkpoint file is **byte-identical** before and after the resume (same SHA-256), the board, turn (Sera, Move 3) and every HP match (`after-resume-dump.txt` equals `before-dump.txt`).
- I then played the rest of that fight on the #186 package (`play-logs/play-rev7-fight1.log`): still rev 7, enemies stayed 10 HP, won in 3 further rounds. Next fight (Watchtower road, encounter 2) of the **old** save: Raider 1, Raider 2 and Vault bow at **14 HP, AC 12, attack +2, d6+1** (`old-save-fight2.txt`), i.e. revision 7 numbers (12 + 2s HP), not the new 28 HP / +4 / d8+3.
- Contrast, same fight on a New Run on #186 (`fight2-start.txt`): 28 HP, AC 13, +4, d8+3, `CatalogRevision 8`.

## 2. New difficulty (New Run on #186, same party): the fights, one run

| Fight | Route / enemies | Rounds | Party HP at the end | Notes |
| --- | --- | --- | --- | --- |
| 1 | Guarded, Scout + Archer, 20 HP, +4, d6+3 | **3** | Arden 26/30 (lowest), others full | Easy, as in staging (staging fight 1: 10 HP). |
| 2 | **Dangerous**, 2 Raiders + Archer, 28 HP each, +4, d8+3 | **13** | Arden 20/30, Ivo 22/22, **Sera 1/22, Nix lost** | Nix downed in round 2 and died after two turns; Sera downed five times (rounds 5, 6, 9, 10, 13). Won. |
| 3 | Guarded, 2 foes at 28 HP (camp already forfeited) | **5** | Arden 11/30, Ivo 9/22, Sera 1/22 | Won with a worn party. |
| 4 | Guarded, 2 foes at 32 HP | **4** | all fallen | **Expedition lost, 3/5 battles won.** |

- **Rounds:** 3, 13, 5, 4 against the PR's mean of about 6. Fight 2 (Dangerous) is far longer than the average; the script's lack of focus fire on the archer is a big part of it (the 28-HP archer survived until round 13).
- **Party low points:** within one fight the party dropped to a single conscious hero twice (fights 2 and 4). A hero died in fight 2 and the run ended in fight 4 after about 11 minutes of play.
- **Camp:** offered once, after the first victory ("Once per expedition"); I pressed on (V) and could not camp later ("Recovery is offered only once"), so fights 2 to 4 ran without any recovery. That choice is mine and makes the run look harsher than a player who camps after fight 2 would see.
- **Do specials still matter?** Not testable with this policy (see above). The script's Cure Wounds on Nix at 1 HP in fight 2 (round 2) did not stop Nix dying two turns later.
- **Felt unfair or stuck?** Fight 2 and fight 4 felt punishing for a level-1 to 2 party, since each enemy hits for d8+3 / d6+4 at +4/+5 against AC 13-16 and has 28-32 HP, but no fight stalled; each ended. The sharp step is fight 4: 2 Guarded foes with 32 HP and +5 killed a party that came in at 11/30, 1/22, 9/22 and a dead Rogue, in 4 rounds.

## 3. Rescue loop: **observed, not a hard trap**
In fight 2 the archer (28 HP, range 4) kept shooting the same hero down: Sera was Rescued at 1 HP and downed again in rounds 5, 6, 9, 10 and 13 (`play-logs/play-fight2.log`). The script chose Rescue each time (an Action), so the archer was only attacked by the other heroes in those rounds: it stayed at 28/28 until round 10. Attacking instead was always available (Ivo, Sera and Arden attacked it in the other rounds and it died), so the party was not locked into rescuing; but the 1-HP Rescue returns the hero to a state the archer can undo every round, so a player who keeps rescuing loses actions each round for no progress. I did not test a controlled "attack instead" run side by side.

## 4. Route previews: **Pass**
Previews read the same as on staging and still make sense with the harder enemies (`screens/c102.png`, `c402.png`, `c450.png`): "2 scouts" and "3 raiders incl. archer", terrain, "+1 gear (rank 3 cap)", "+1 potions" / "+2 potions". They give no HP or damage, so nothing on them is wrong; they also do not say that the Dangerous route is now much harder.

## 5. Logs and shutdown
All 8 launches closed through a normal window close. Across the logs: 0 ensures, 0 `LogMaterial`, 0 `Fatal`. Two logs (`r1`, `r2`) show one `LogLoad: Error: Failed to enter /Game/.../L_AgentSandbox: .` each; that is my launch helper passing the run name as an extra command-line argument (the game then loads the default map as normal). It is not related to #186; the other six logs have 0 Error lines. No crash folder. One earlier launch was a plain re-launch for the same helper bug.

## Not done
Frame-accurate feel, a human's play (the policy above is not one), specials and Rage use, fight 5 and the vault, a camp-after-fight-2 run, SRD ruleset, 1280x720 and 1600x600, and a controlled attack-instead run for the Rescue question.
