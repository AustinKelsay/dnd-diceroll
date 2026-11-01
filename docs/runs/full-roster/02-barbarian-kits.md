# Native Barbarian contract (#40)

Original compact adaptations, available at level 1, not reproductions of tabletop subclass rules. Public test seams are catalog queries, combat Start/Submit/Forecast/Snapshot and run/session checkpoint/Resume.

All four builds: 34 HP, AC 14, +5 attack, d8+3 melee, speed 4, initiative +1. Levels 1–5 add only +0/1/2/3/4 basic damage; no healing or refill. Two Rage uses, camp restores one capped at two to conscious survivors. Rage (slot 1) costs a Bonus Action and one use, self only, requires inactive stance. It grants +2 basic damage and reduces incoming damage by 1 (minimum zero). It lasts through the activation turn and the following own turn: decrement at each owner's End Turn. No refresh while active. Downing/death and terminal battle cleanup clear stance and protection; transitions inherit cleared state. Resume never ticks or refills.

Slot 2 requires active Rage, costs no resource, and uses the stated existing budget. All target distances are Manhattan with blocked-cell sight; only conscious targets. No effect heals or revives.

| Build ID | Slot 2 | Contract |
| --- | --- | --- |
| barbarian.berserker | Frenzy | Bonus Action, enemy range 1, guaranteed 4 damage; permits an additional attack after a basic Action on the following rage turn. |
| barbarian.totem-warrior | Shelter | Action, another ally range 3; reduces each incoming damage by 3 until the protector's next turn starts. One protected ally per owner; strongest protection wins, no stacking. Clears if either participant is downed/dead or Rage expires. |
| barbarian.beast | Long Claw | Action, enemy range 2, guaranteed 6 damage. Reach attacks across an open cell; no displacement. |
| barbarian.wild-magic | Wild Bolt | Action, enemy range 3, committed seeded d2 outcome gives 4 or 8 guaranteed damage. LastSurge stores 1/2 until next bolt or encounter end; result reports outcome and damage. Preview states both possibilities and never rolls. |

Shelter and Rage forecasts report actual protection/buff descriptions with zero damage/healing, not fictitious healing. Generic special slots resolve catalog facts (name, description, budget, target kind, range, effect, resource availability). Ordered committed per-target facts report damage/protection/stance and actual cells, without presentation recomputation. No secondary targets or displacement in this unit.

Omissions: exhaustion, totem animal selection, creature forms/inventories, random tabletop surge tables, spellcasting, damage types, resistance fractions, reactions, extra initiative, advantage, scaling subclass unlocks, companions and resurrection. Long Claw and guaranteed compact attacks are deliberate arena adaptations. Old five kits retain their numeric rules and random call order.

Persistence uses reflected bounded value fields RageUses, RageTurns, GuardTargetId and LastSurge; catalog revision 2 rejects prior interpretations of unpublished schema 4. Invalid ownership, duration, pool caps, missing/dead/friendly relationships, terminal statuses and unsupported profiles reject atomically. LastSurge may remain after rage expires during combat but clears at terminal cleanup. Shelter requires live Rage and a conscious same-side other target; separation after casting does not cancel it.
