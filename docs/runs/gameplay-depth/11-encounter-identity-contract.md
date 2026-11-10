# Slice 4 contract: encounter identity and pacing

Status: **child issue [#188](https://github.com/AustinKelsay/dnd-diceroll/issues/188); written 2026-11-10 under [#112](https://github.com/AustinKelsay/dnd-diceroll/issues/112)'s delegated authority.**
- Fixed review baseline: staging `ea9166c` (#186 merged).
- Measured baseline: the tuned Compact revision 8 run of [10-balance-tuning-report.md](10-balance-tuning-report.md), with the harness and policy of [07](07-balance-baseline-contract.md) (policy version 1, 99 parties x 32 seeds).
- Absorbs [#187](https://github.com/AustinKelsay/dnd-diceroll/issues/187) item 2 (previews do not convey danger). The other #187 items stay there.
- The before/after evidence will be in [12-encounter-identity-report.md](12-encounter-identity-report.md).

## 1. What the revision 8 baseline says about encounters

- Every fight reuses one of three recipes (split stones, ruined pillars, broken wall) with the same shape: the party in column 4, an enemy line 5–7 cells away, 2 Guarded or 3 Dangerous enemies of two roles (raider, archer). Only HP, armour and damage grow by stage.
- Pacing comes only from those numbers. The defeat share by fight is 0.1%, 0.1%, 0.3%, 5.2% and 7.7%, so fights 1–3 feel alike and fights 4–5 alike.
- The finale is the same fight with bigger numbers and an artifact preview.
- By recipe, the broken wall plays longest (4.3 rounds Dangerous) and the other two alike.
- Previews say "2 scouts" or "3 raiders incl. archer" and nothing about the risk, although a Dangerous fight costs 2.3 to 5.9 times the HP of a Guarded one.

## 2. Encounter identity

Three stages with their own layouts, rosters and preview wording. **Five encounters, two offers per stage, four ordinary rewards and the artifact-only fifth victory stay exactly as they are.**

| Stage | Fights | Layouts (recipe id: name, obstacle kind) | Guarded roster | Dangerous roster |
| --- | --- | --- | --- | --- |
| Approach | 1–2 | 0 split stones (stones), 1 ruined pillars (pillars), 2 broken wall (walls), unchanged | raider, archer | raider, raider, archer |
| Pressure | 3–4 | 3 fallen colonnade (pillars), 4 sunken causeway (walls), 5 cairn field (stones) | skirmisher, raider, archer | brute, skirmisher, raider, archer |
| Finale | 5 | 6 vault sanctum (pillars), front-to-back | warden, raider, archer | warden, brute, raider, archer |

**Layouts** (cells as `(x, y)` on the smallest 16 x 11 board; every one fits 16–18 by 11–12; obstacles are single cells or straight runs).
- **Approach**, unchanged: the three revision 8 recipes.
- **3 fallen colonnade (pillars):** (6,2), (6,8), (8,4), (8,6), (12,2), (12,8), (13,5), (2,1), (2,9), (14,10). Staggered single pillars break sight lines across the middle, which favours closing in over shooting. Enemy column 10, rows 3/5/7/9.
- **4 sunken causeway (walls):** runs (6..10, 1) and (6..10, 9), and the column run (8, 4..6). A walled causeway with a central block splits the approach into two lanes, a chokepoint fight. Enemy column 11, rows 3/5/7/9.
- **5 cairn field (stones):** (6,2), (6,6), (7,4), (7,8), (8,1), (9,6), (12,4), (12,9), (13,2), (14,7), (2,2), (2,8). Scattered cover; ranged duels around the cairns. Enemy column 10, rows 3/5/7/9.
- **6 vault sanctum (pillars), front-to-back:** with `c = W / 2`, pillars at (c−4, 3/5/7), (c+4, 3/5/7), (c−6, 5) and (c+6, 5) line an aisle to the back.
  - The party enters at the front, (c−2..c+1, 1), the camera's home side.
  - The guardians wait at the back: the warden at (c, 8), flankers at (c−3, 7) and (c+3, 7), and a fourth at (c, 6) when there is one.
  - This matches the Vault world's open front and the Waystone on the back wall (#149).

Every layout keeps the playable 5–7 cell opening gap (closest hero to closest enemy), keeps all open floor connected, and puts no spawn on an obstacle. A test checks every layout at every size, route and stage.

**Enemy roles**, saved explicitly in `FCombatantState::Role`. New names keep the words presentation reads today.

| Role | Name | Behaviour (the unchanged enemy AI) | Stats against the revision 8 base for its route and stage |
| --- | --- | --- | --- |
| Raider | "Scout N" (Guarded) / "Raider N" (Dangerous) | melee, speed 4 | the base |
| Archer | "Archer"; "Vault bow" in the finale | range 4, speed 4 | the base |
| Skirmisher | "Skirmisher" | melee, **speed 6**: reaches the back line in one turn | lower HP, higher armour, a smaller die |
| Brute | "Raider brute" | melee, **speed 3**: slow, hits hard | more HP, lower armour, a bigger die |
| Warden | "Vault warden" | melee, speed 4: the finale's anchor | about double HP, higher armour, more damage |

Starting multipliers (tuned with the harness and recorded as built):
- skirmisher: HP x0.6, AC +1, d4;
- brute: HP x1.5, AC −1, d10;
- warden: HP x2, AC +1, die as the base +1 damage.

Enemy behaviour is unchanged: the same deterministic legal `PlayEnemyTurn`. Target choice and the rescue loop stay in #187.

**Previews** (`FExpeditionRoute::Preview`, read by the route card one fact per line). Four short fact lines after the route tag:
- the roster, for example `4 foes: brute, skirmisher, raider, archer`;
- the terrain and its tactical note, for example `sunken causeway: two lanes`;
- the risk, `Risk: low | moderate | high | severe`;
- the rewards on one line, `+1 gear, +2 potions` (finale: `Recover the artifact; no loot`).

The risk is a fixed table by stage and route:

| | Approach | Pressure | Finale |
| --- | --- | --- | --- |
| Guarded | low | moderate | high |
| Dangerous | high | severe | severe |

The report must show that a higher risk word costs more party HP and loses more often, on average, than a lower one. Any pair where that fails is fixed by tuning, not by rewording.

## 3. Targets (design proposals, measured)

| # | Target | Why |
| --- | --- | --- |
| E1 | **Pacing rises by stage:** mean party HP lost per fight Approach < Pressure < Finale for each route; defeat share by fight rises from fight 3 | #112 "approach, mid-run pressure and a distinct vault finale"; baseline fights 1–3 alike |
| E2 | **Layouts play differently:** within a stage, the layouts differ by at least 15% in mean rounds per fight or in the melee share of damage | #112 "tactically different"; baseline: two of three recipes play alike |
| E3 | **Risk words are true:** the ordering rule above holds for every pair of tiers | #187 item 2 |
| E4 | **Slice 3 targets hold:** rotation win rate 55–90%, every representative party at least 35%, at least 15% of victories lose a hero, Dangerous at least 1.5x Guarded HP, the finale has the highest defeat share, melee not dominated (contract 09) | do not undo slice 3 |
| E5 | Gates zero, no stalls, same seed same results | contract 07 |

All are proposals; the report states met or missed against the evidence.

## 4. Boundaries

**Unchanged:**
- five encounters, two route offers per stage, four rewards, one camp, the artifact-only fifth victory;
- grids 16–18 by 11–12 (#151: keep unless the vertical slice shows crowding), and nothing playable outside the board;
- the 70 kits, levels and the party;
- enemy behaviour;
- schema 5;
- SRD generation and numbers;
- `Presentation/`, capture, camera and world code;
- `Config/` and `Content/`.

**Graphics constraints from #138 and #149:**
- off-board space is presentation's (braziers 1.25 cells out, dressing 2–5 cells out, Vault walls 1–2 cells out at the back and sides, front open);
- obstacles are cell-aligned single cells or straight runs;
- presentation hashes (W, H, EncounterSeed, kit kind) and never feeds back;
- `EncounterSeed` stays saved on the route.

**Identity for presentation** (ADR-0001: rules hold no presentation types):
- **Encounter:** a const query `FExpeditionRun::Encounter(Progress, Route)` returns `FEncounterIdentity`. It carries the stage kind (Approach, Pressure, Finale), the obstacle kind (0 stones, 1 pillars, 2 walls), the layout name and whether the layout is front-to-back. It derives everything from saved fields: the route's recipe id and route id, and the run's revision and ruleset.
- Presentation today infers the Vault from the encounter index and the kit from the recipe id (0/1/2). Until the graphics lead switches to the query, recipes 3–6 draw as stones (the kit's fallback), and the finale still gets the Vault theme by index.
- The graphics lead is told so before this merges, and the new roles are listed for the rigged cast (#150).
- **Enemy kind:** saved in `Role`.

**Versioning:** new runs are created under **catalog revision 9**. Runs created under 7 or 8 keep their own generation, enemies and previews for life, exactly like #186. SRD runs keep revision 7 generation at every revision. Revision 9 draws the generation random numbers in the same order as revision 8 (the pressure recipe is the drawn 0–2 plus 3; the finale draws and ignores it), so fights 1–2 keep their boards, seeds and capture fixtures. Unknown revisions are still refused by name and preserved. `NewRun` takes an explicit older supported revision for tests and for the harness's before-run.

**Heads-up on #95** for Landon, as for #186.

## 5. Evidence

- **Before:** revision 8 new runs on this build (the same CSVs as the #186 after-run, which also proves the revision 8 path is untouched).
- **After:** revision 9.
- The harness gains a `melee_damage` column per encounter for E2; the policy is unchanged.
- **Report tables:** the slice 3 target table again; per stage and route; per layout (rounds, HP lost, defeats, melee share); risk tier versus measured cost; every tried tuning step.
- **Checks:** build 0 warnings, full `Project.` 0 warnings, helpers, `contribute.py check`, `Balance.Baseline.` before and after, and data on an evidence branch.
- **On-screen play:** a list of checks for the PR, covering the finale's front-to-back entry, preview wording and the new roles' readability.
