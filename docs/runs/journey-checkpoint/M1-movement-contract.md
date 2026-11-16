# M1 contract: SRD move-through (catalog revision 11)

Status: **child issue [#240](https://github.com/AustinKelsay/dnd-diceroll/issues/240) of spec [#239](https://github.com/AustinKelsay/dnd-diceroll/issues/239); fixes [#217](https://github.com/AustinKelsay/dnd-diceroll/issues/217).** Austin chose the rule on 2026-11-15. Fixed review baseline: staging `5223a0b4`. Balance tuning is M2 ([#241](https://github.com/AustinKelsay/dnd-diceroll/issues/241)), inside the same unreleased revision 11.

## 1. The rule

SRD 5.2.1: a creature may move through a space occupied by an ally, an incapacitated creature or a dead creature, and may never end its move in an occupied space. In this game:

- **Passable:** a cell held by an ally (conscious or Downed), by a Downed hero of either side, or by a body (a Dead unit of either side). Enemies are never Downed: they die at 0 HP.
- **Blocking:** a cell held by a conscious opponent, and blocked terrain, as before.
- **Never a destination:** any occupied cell. Each cell passed costs one movement, the same as an open one.
- **Who:** heroes and enemies alike. Enemies pass through their allies and through bodies, never through a living hero, so front-liners still hold chokepoints.
- **Which runs:** Compact runs created at catalog revision 11, which every new run now is. Revisions 7–10 keep the older rule, under which every unit blocks its cell, for the life of the run. SRD runs keep it too: SRD mode is Landon's opt-in track (#95), and its opportunity attacks can stop a mover mid-path on the cell it is leaving, which would need its own rule for a mover stopped inside an ally's cell.

Nothing else moves. Generation, enemy stats and rewards are revision 10's exactly, and no random number is drawn differently.

## 2. Rules keyed on movement

- **Compact has no opportunity attacks** and no reactions ([rules reference](../../rules-reference.md#tactical-adaptations-and-omissions)), so passing beside or through units provokes nothing.
- **Forced movement is unchanged.** A Push still moves its target only into an open cell (`IsOpen`), so a push never ends on an occupied cell, as SRD requires. Swap exchanges two cells and is not movement through anything. SRD's Shove keeps its own open-cell check.
- **Line of sight is unchanged.** Combatants never obscured sight.

## 3. Where it lives

- `FCombatEncounter::bMoveThrough`, a plain C++ member and **not a `UPROPERTY`**, so the save schema and the checkpoint bytes do not change. The run sets it from its saved ruleset and revision (`FExpeditionRun::MovesThroughUnits`): on the opening encounter, on every layout it generates (`EncounterLayout`, so `Carry` takes it too), and on `Restore`, which ignores whatever the in-memory snapshot holds. A decoded checkpoint therefore plays its own revision's rule.
- `FCombatBattle::IsPassable(Cell, Mover)` is the new travel test that the path search (`Distances`) uses. `IsOpen` stays the ending test. `LegalMoves` and `PathTo` add `IsOpen` on the destination. Under the older rule every reachable cell is already open, so both filters change nothing there.
- **Enemy turns** use the same search. The firing cell must be one the enemy can stop on, and when movement runs short, the enemy walks back along its route to the farthest cell within movement that it can stop on. If that is its own cell, it does not move.
- **Presentation reads the same queries.** The move contour (`LegalMoves`), the path ribbon and the destination bracket (`PathTo`), the move blocker `NoOpenCell` (`FActionFactsQuery`), the path-cost number and the autoplay all go through them. No presentation code changed.

## 4. Balance harness (policy 3)

Under move-through the policy's own distance heuristic (rules 3 and 9 of [contract 07](../gameplay-depth/07-balance-baseline-contract.md#3-party-policy-deterministic-legal-commands-only)) lets the walker pass allies, Downed heroes and bodies, and blocks it only at conscious opponents, as the rules' path search does. `Deadlocked` no longer counts bodies and Downed heroes as walls. Under revisions 7–10 both are exactly policy 2 (section 6, comparison A). `PolicyVersion` is 3.

## 5. Tests

- `Project.Expedition.Combat.MoveThroughPassesAlliesDownedAndDead`: a corridor with an ally, a body and a Downed ally in a row. The legal moves are exactly the open cells beyond them, and the path crosses all three in order. Moves ending on any of them are refused and change nothing. Each cell costs one movement. The older rule allows no move.
- `…Combat.MoveThroughLivingFoeBlocks`: a living foe holds a corridor (for heroes and for enemies), and a dead one does not.
- `…Combat.MoveThroughEnemyTurns`: an enemy passes its ally and a hero's body to attack. It never stops on the body that holds the only firing cell, and when short of movement it stops short of its ally rather than on it. The older rule walls it in.
- `…Combat.MoveThroughFreesTheSealedHero`: #217's trace (rot2-03, seed 20261119, route 10, fight 4, revision 9 before #214's layout fix). The sealed Barbarian walks out over the skirmisher's body and can attack the brute. Under the older rule it has no move.
- `…Run.MoveThroughFromRevision11`: the gate for every supported revision and SRD. The opening encounter and every fight carry the run's rule. The real save round trip has no such field and restores the rule from the revision, even from a snapshot whose flag disagrees.
- `…Presentation.MovePreviewPassesBodies`: on a resumed revision 11 checkpoint with a body in front of the acting hero, the move contour equals `LegalMoves` and offers no occupied cell. The ribbon equals `PathTo` and crosses the body, the destination has its bracket, and hovering the body shows no path and no bracket.
- `Project.Balance.Revision10RunsArePinned`: six seeded revision 10 harness runs hash to digests captured on unmodified staging `5223a0b4`. Each digest covers the three CSV rows plus the final snapshot as the real save encodes it, RNG included. They include #217's stall (rot5-04, seed 20261116, route 7) under `reserve` and `mixed`.
- `Project.Balance.DeadlockedStallIsRecognised` adds two cases: under move-through, #217's position is no longer a deadlock, but terrain alone still walls a hero in.
- Fixture `move-through-battle` (`Project.Tools.CaptureFixtures`) is a revision 11 battle with a body in front of the acting hero, for the gallery's targeting shot.

## 6. Measurement

Harness [07](../gameplay-depth/07-balance-baseline-contract.md), the same 99 parties and 32 seeds (`20261109 + i`, route pattern `i`), so 3,168 complete Compact runs per measurement. Each measurement is one full `Balance.Baseline.` run under the machine lock at `--cores 2`, about 30–35 s of play. All of them ran on Ultra (UE 5.8.4), with `DND_BALANCE_REVISION` and `DND_BALANCE_REWARD` set per measurement.

**A. Revision 10 is unchanged.** Revision 10 was swept under all four reward strategies on unmodified staging `5223a0b4` (policy 2), then again on this branch (policy 3). In all 24 groups, `runs.csv`, `encounters.csv` and `heroes.csv` are byte-identical, and `group.json` differs only in `seconds` and `policy_version`. The #217 deadlock is still there under `reserve` and `mixed` (rot5-04, seed 20261116, route 7), as revision 10 keeps it for life.

**B. Revision 11 against revision 10** (this branch). Paired differences are over (party, seed), with `balance_report.py`'s seed-cluster bootstrap: 10,000 resamples, 95% percentile intervals.

| Strategy | Win rate 10 → 11 | Paired win difference | Deaths per run 10 → 11 (paired) | Rounds per fight 10 → 11 | Stalls (deadlocks) 10 → 11 | Gates | Defeats in fights 3 / 4 / 5, 10 → 11 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| gear | 78.6% → 84.4% | +5.8 pts (+2.9 to +8.9) | 0.947 → 0.677 (−0.27, −0.40 to −0.15) | 5.98 → 5.47 | 0 → 0 | 0 | 44 / 144 / 484 → 25 / 84 / 385 |
| reserve | 78.2% → 85.7% | +7.5 pts (+4.9 to +10.4) | 0.958 → 0.621 (−0.34, −0.45 to −0.23) | 6.14 → 5.61 | 1 (1) → 0 | 0 | 40 / 144 / 501 → 25 / 74 / 353 |
| honed | 75.0% → 83.0% | +8.0 pts (+4.9 to +11.5) | 1.092 → 0.733 (−0.36, −0.50 to −0.23) | 6.20 → 5.65 | 0 → 0 | 0 | 43 / 163 / 582 → 23 / 98 / 416 |
| mixed (default) | 77.8% → 85.4% | +7.6 pts (+5.1 to +10.4) | 0.976 → 0.634 (−0.34, −0.45 to −0.24) | 6.14 → 5.61 | 1 (1) → 0 | 0 | 42 / 146 / 508 → 24 / 79 / 358 |

Gates means policy errors, illegal proposals, passive ends and protection mismatches; every one is zero in every measurement. Rotation win rates under revision 11 are 84.1% (gear), 85.1% (reserve), 82.4% (honed) and 85.1% (mixed), against 78.1%, 77.9%, 74.3% and 77.5% under revision 10.

**Reading it** (this is what one simple policy does, not human difficulty):
- **Zero stalls and zero deadlocks** under revision 11, across 12,672 runs.
- **The party gets clearly stronger:** about +6 to +8 points of wins, about a third fewer deaths per run, and half a round shorter fights. Most of the gain comes in fights 4 and 5. The likely causes were not measured separately: allies no longer wall each other in a lane, a rescuer can reach a Downed hero through the line, and melee heroes reach targets past bodies. Enemies gain the same freedom, but under this policy the party gains more.
- **No tuning here.** M2 ([#241](https://github.com/AustinKelsay/dnd-diceroll/issues/241)) retunes inside revision 11 from this measurement. Its baseline is the four `m1-r11-*` measurements.

**Reproduce:**

```sh
for s in gear reserve honed mixed; do
  DND_BALANCE_REVISION=11 DND_BALANCE_REWARD=$s DND_BALANCE_OUT=.local/balance/m1-r11-$s \
    flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock python3 tools/dev.py --cores 2 test --filter Balance.Baseline.
  python3 tools/balance_report.py .local/balance/m1-r11-$s
done
```

The selected summaries, the paired comparison script and its output, and the gallery shots are on the evidence branch named in the PR.
