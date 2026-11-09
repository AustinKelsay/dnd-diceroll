# Slice 2 child contract: action clarity and onboarding (#58 + follow-up)

Status: **approved in principle by the coordinator (2026-11-08); the two open decisions are settled in section 10.** Implementation starts with PR A. Part of [#112](https://github.com/AustinKelsay/dnd-diceroll/issues/112) slice 2, under that issue's delegated authority. Fixed review baseline for all work below: staging `d9eac886688dfb487d07a0dd68b9a146e689ffcd` (#137 merged). Written 2026-11-08 against that tree plus the open PRs [#155](https://github.com/AustinKelsay/dnd-diceroll/pull/155) (compact HUD, #146) and [#156](https://github.com/AustinKelsay/dnd-diceroll/pull/156) (SRD rules PR 1, #95), both read only.

## 1. Narrowing of #58

#58 is Landon's SRD 5.1 proposal ("cost pill and usage pill per button", Reaction budget chip, concentration and rest usage tags). Compact has no Reaction, concentration, short/long rests or free object interaction, and #112 forbids advertising them. This contract delivers #58's real value on what Compact has:

| #58 proposal | Compact contract |
| --- | --- |
| Cost: Action, Bonus Action, Reaction, Free, Movement | **Action, Bonus Action, Movement, and "no cost" (End Turn).** No Reaction chip. |
| Usage: Passive, Once per turn, Per short/long rest, Per camp, Consumable, Concentration | **At will** (Attack, Move), **spends a named resource** (Kit, Slot, Wind, Maneuver, Rage use, Potion bottle) with cost, count held and cap, and the camp recovery taken from the catalog's own recovery values; **needs / forbids a stance** (Rage). Passives have no button, so no pill. |
| Replace duplicated cost strings with one fact query | Yes, see section 2 and the PR order in section 6. |
| Rendered capture shows pills and tooltip | Yes, section 7. |

The facts are shaped so SRD mode can add Reaction and its blockers later without changing Compact output (section 3).

Out of scope, by design: new actions, balance, rules, RNG, save schema, catalog revision, and any change to resolution or legality.

## 2. The fact query (rules side, headless)

One const query, in new files so it never conflicts with in-flight edits to `CombatBattle.{h,cpp}`:

- `Combat/ActionFacts.h` and `Private/Combat/ActionFacts.cpp`: `FActionFacts FActionFactsQuery::For(const FCombatBattle&, int32 ActorId, ECombatCommandType Action)`, using only public queries (`Snapshot`, `Find`, `LegalTargets`, `ActionReach`, `FBuildCatalog::Abilities` and `Find`). It consumes no RNG and changes nothing, and it does not depend on controller selection, hover or playback state.
- `FActionFacts` carries, with no display strings:
  - `Command`, `Name`; `CostKind` (`Action | BonusAction | Movement | None`);
  - resource: `ResourceId` (Kit, Slot, Wind, Maneuver, Rage, Potion, none), `Cost`, `Held`, `Cap`, `CampRecovery` (from the build's `*Recovery`; Potions recover 0);
  - stance: `RequiresRage` / `ForbidsRage`;
  - target facts: `TargetKind` (enemy, ally, wounded ally, downed ally, self), `Range`, `bNeedsSight`;
  - `bAvailable`;
  - `Blockers`: an ordered array of `{Reason, Value, Needed}`.
- Blocker reasons for Compact, in fixed order: `BattleInactive`, `NotYourTurn`, `ActorDown`, `ActionSpent`, `BonusSpent`, `NoMovementLeft`, `ResourceShort` (carries held and needed, so a spell that needs both a Kit and a Slot reports each short one), `StanceRequired`, `StanceBlocks`, `NoWoundedTarget`, `NoDownedTarget`, `OutOfRange` (carries range), `SightBlocked`, `NoEligibleTarget`. Target reasons are classified from `ActionReach` (budget independent), so they are reported even while a budget is spent. Every unavailable action has at least one blocker, and an available one has none.
- `bAvailable` is defined by agreement with the rules, not re-derived: **`bAvailable` is true exactly when `LegalTargets` is non-empty (Attack/Move: when an attack or a move exists), and exactly when `Submit` with a legal target would accept.** The reason ladder mirrors `LegalTargets`' gates and is held to it by the property tests in section 4.
- `Resolve`, `LegalTargets` and the `Result.Feedback` strings are **not touched** in this slice (#156 rewrites `Resolve`). When a command is rejected, the view shows the first blocker's text from the facts formatter; the engine's generic rejection string stays as is. A later slice can route the rules' own rejections through the same facts once #156 has landed.

Presentation helpers, in `ExpeditionStyle`, read facts only: `CostPillText`, `UsagePillText`, `BlockerText`, a tooltip headline, and one name table for `Move / Action / Bonus action` shared with the turn-budget chips. Each uses an exhaustive `switch` over the enums, and a test walks every enumerator to prove it has text.

## 3. Fit with #156 (SRD ruleset), read-only

I read the PR head `5a3fb1c`. It adds `ERuleset { Compact, SRD }` on the encounter, `FStandardActionRules::Cost(...)` (Action vs Bonus), `FOpportunityRules`, per-unit `bReactionAvailable`, `bConcentrating`, `bProne`, `GrapplerId`, new commands `Grapple`, `Shove`, `Escape`, and SRD-only rejections in `Resolve`. It touches no `Presentation/` or `Party/` file and rewrites about 400 lines of `CombatBattle.cpp`.

- **No dependency.** This work does not build on that branch and does not need it to merge. The facts live in new files and read public API only, so the two sets of changes should merge without textual conflicts. Whichever PR lands second merges the other and reruns the gate. Expected touch points: the `ECombatCommandType` enum, `LegalTargets`, `Resolve`.
- **Extension points for SRD mode.** The enums gain values, nothing else changes: `CostKind::Reaction`; blockers `ReactionSpent`, `Prone`, `Grappled`, `Concentrating`; cost for Dash/Disengage/Hide taken from `FStandardActionRules::Cost`; facts for `Grapple/Shove/Escape`. Compact never emits them, so Compact pills and tests stay identical. The exhaustive-switch/enum-walk test forces SRD's PR to add text for every new value, and the equivalence test forces its new blockers to agree with `LegalTargets`.
- **Authoritative rules stay in C++** (ADR-0001). Presentation formats facts and never decides legality.

## 4. Tests (headless, in the pinned engine, `Project.Expedition.*`)

Rules and facts, PR A:

1. `Combat.ActionFactsMatchLegalTargets`: every catalog build (all 70 kits) x every action it has x a state matrix (fresh turn; Action spent; Bonus spent; resource 0; Rage on/off; wounded ally; downed ally; target out of range; target behind a blocker; not the actor's turn; battle over). Asserts `bAvailable == !LegalTargets.IsEmpty()` and that every blocker array is non-empty exactly when unavailable.
2. `Combat.ActionFactsMatchSubmit`: for each available fact, `Submit` accepts, and the budget it flips and the resource it decrements equal the fact's `CostKind` and `Cost`. For each unavailable fact, `Submit` is rejected and the snapshot and RNG are byte-identical.
3. `Combat.ActionFactsReasons`: one fixture per reason with exact `Value`/`Needed`, ordering, and the multi-blocker cases (for example Action spent and no Slot together).
4. `Combat.ActionFactsReadOnly`: repeated calls are equal, `CompareScriptStruct` shows no state or RNG change, and results do not depend on controller or view state.
5. `Combat.ActionFactsCoverEveryAbility`: every `FAbilityFacts` of every build maps to a fact with the same bonus-vs-action cost, resource name and cost; no ability produces `CostKind::None` unless it is End Turn.
6. Existing tests stay; no fixture is edited without recording the preserved intent.

Presentation, PR B (after #155, section 6):

7. `Presentation.ActionTagsFromFacts`: the HUD exposes the tag strings it asked to draw (same counter-style seam as #137's `TargetChipsRequested`). Every one equals the formatter's output for that fact, for all kits, at the four compact-HUD test sizes.
8. `Presentation.DescriptionsHaveNoCostWords`: no `FAbilityFacts::Description` and no `DescribeAction` text contains a cost, resource or requirement phrase the facts now supply.
9. `Presentation.ActionTagsHiddenUnderOverlays` and `...DuringPlayback`: zero tags requested while Help, Records, the return prompt or playback is up (same `ShowsTargeting()` gate).
10. #155's `CompactHudBudgetAndBounds` still passes: all buttons inside the viewport at 1280x720, 1920x1080, 2560x1080, 1024x576, and persistent coverage <= 18% (currently 15.34%).
11. Keyboard parity: selecting a mode with its key shows the same tooltip (cost, reasons) that hover shows. Today only hover does.

## 5. HUD part: small additions on top of #155

#155 rebuilds the dock (104x42 buttons, 20-unit top bezel, room reserved for #58). PR B is deliberately small and starts from #155's head or from staging after it merges:

- no change to dock geometry, constants, button ids, key order or Tab order;
- each button's sub-line becomes the **usage pill** (`1 Kit · 2/3`, `At will`, `Needs Rage`), and a thin **cost pill** (`Action`, `Bonus`, `Move N`) sits in the button's top edge, inside the existing rectangle, so persistent coverage cannot rise;
- unavailable buttons stay dimmed and add the first blocker as a short reason line in the tooltip, not a new persistent panel;
- tooltip = cost headline (facts) + effect-only catalog text; the "unavailable" suffix and the hand-built cost strings in `DrawActionBar` are deleted;
- the bezel's budget chips use the shared name table; no Reaction chip.

The fact query and its tests (PR A) do not depend on #155 in any way. If #155 changes before merge, only PR B moves.

## 6. Delivery order

- **PR A (#58, headless):** `ActionFacts` query, formatter helpers with enum-walk test, tests 1-5. No HUD or description change, so nothing regresses and it merges independently of #155.
- **PR B (#58, presentation):** after #155. Replaces `Ready()` and the cost strings with facts, adds the pills and tooltips, strips cost wording from `DescribeAction` and catalog descriptions (text only: descriptions are not saved, catalog revision unchanged), updates any test that asserted those strings, adds tests 7-11. The on-screen checks below run on its package.
- **Follow-up ticket, onboarding hints (section 8):** after PR B, so hints can point at the pills.

## 7. On-screen checks (display turn, after PR B)

Package of the exact head, windowed, isolated user and save dirs; keyboard, then pointer; at 1280x720, 1920x1080 and 1600x600 (short height); inspected screenshots linked from an evidence branch.

- Cost and usage pills on every button for Fighter (Battle Master), Cleric, Wizard, Barbarian (stance), Rogue and a two-special kit: no clipping, longest names and the 7-combatant Watchtower fixture included.
- One screenshot per reachable blocker: Action spent, Bonus spent, resource 0, Rage required / already raging, no wounded ally, no downed ally for Rescue, out of range, blocked sight.
- Tooltip by hover and by keyboard mode selection; End Turn has no cost pill.
- Pills and tooltips hidden under Help, Records, the return prompt and playback; no stale tag after a turn change.
- Budget chips agree with the pills after each spend; HUD coverage within the compact budget.

## 8. Follow-up ticket: contextual onboarding hints (draft)

**Title:** Contextual combat hints with skip and reopen (#112 slice 2).
**Blocked by:** PR B of #58 (hints reference the pills). Parent: #112.

**Behaviour.** One short callout at a time, never over the board's play area or the dock, never during playback or an enemy turn, shown on the party's turn at rest. Four hints, each shown at most once per run:

1. **Move, select, confirm:** first party turn of a fresh run: click a glowing tile to move, a marked enemy to attack; arrows + Enter does the same from the keyboard.
2. **Scarce resources:** first time an action button shows a spent or short resource, or a resource is spent: what the count means and that camp gives back only what the build's recovery says (wording read from the facts' `CampRecovery`; Potions say they are not refilled).
3. **Downed and Rescue:** first time a party member is Downed: Rescue (R) from an adjacent ally within the shown turns or they die (`RescueTurnsLeft` from the snapshot).
4. **Ending a turn:** first time the active hero has nothing available but End Turn: Space ends it.

**State and saves.** Hint state is presentation-only and per process: shown flags and a "tips off" switch live in memory, reset by New Run, never in the checkpoint. **Resume does not arm hints** (a returning player is not a first-time player); they can be re-armed from Help. No schema, catalog or RNG change. **Decided: "tips off" lasts for the session only.** No settings file is added.

**Controls.** "Got it" and "Skip tips" buttons on the callout; Help gains "Replay tips" (re-arms and closes Help). Keyboard: **D dismisses the visible hint** ("Dismiss"). D is unbound (the camera uses Q/E and right-drag) and the bound keys were checked by grep of `EKeys::` in `Source/` on 2026-11-08. It is shown in Help and on the callout. Hints take no focus and add no Tab stop (combat holds no menu focus by design).

**Modal-input rules.** Hints are non-modal, so play is never blocked; nothing leaks either way:
- while Help, Records or the return prompt is open (`OverlayOpen()`), hints are hidden, do not advance, and are not dismissed by overlay keys;
- clicks on a hint's own buttons are swallowed by the HUD (`CoversBoard`) and never move or attack;
- the dismiss key is a no-op when no hint is visible, and never reaches gameplay when one is;
- skip and reopen change no game state.

**Tests.** `Presentation.HintsFollowFirstRun` (each trigger fires once, in order, from real controller input; no hint on a resumed run; New Run re-arms), `Presentation.HintsSkipAndReplay`, `Presentation.HintsNeverLeakInput` (click on a hint button and the dismiss key leave the run, checkpoint bytes and selection unchanged; hidden under every overlay and during playback), HUD budget and bounds still pass with a hint up at the four sizes.
**On-screen:** first-run sequence at 1280x720 and 1920x1080, Skip tips, Replay tips from Help, Resume shows none, hint hidden under Help and the return prompt, no overlap with dock, log or chips, veteran flow (Skip on the first hint, then no further prompts).

## 9. Shared acceptance

Rules authoritative in C++; queries consume no RNG; rejected commands and failed checkpoint writes leave state and bytes unchanged; saves, catalog revision and Compact seeds identical (the seeded full-run fingerprint test must pass unchanged); pinned UE 5.8.4 build with zero warnings in a fresh checkout; full `Project.` zero warnings/failed/not-run; helpers and `contribute.py check`; independent Standards and Spec review of each PR; rendered evidence for PR B and the hints ticket.

## 10. Open decisions (recommendations)

- **Decided:** "tips off" is session-only (see section 8).
- **Decided:** the hint dismiss key is **D** (section 8). If a later slice binds D, move the hint key and Help text together.
- Open: Landon: tell him #58 is narrowed to Compact facts, that SRD cost and blocker values extend `ActionFacts` additively, and that #156 needs no change for this to land.
