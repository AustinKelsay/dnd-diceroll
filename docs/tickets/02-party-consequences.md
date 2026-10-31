# Survive losses and use party roles

## Parent and dependency

[Expedition V1](https://github.com/AustinKelsay/expedition-rpg/issues/1). Blocked by: Play a tactical battle.

## Outcome

The same party becomes tactically distinct and carries the consequences of a fight into another encounter.

## Acceptance criteria

- Fighter, rogue, cleric, and wizard have distinct useful actions with explicit ranges, targets, limited resources, and readable feedback.
- Zero HP produces a distinct downed state and a bounded rescue window. Rescue, actual death, remaining survivors, and full-party wipe have explicit deterministic behavior. Dead members never act or return.
- Health, ability resources, equipment and consumables retain stable ownership across an encounter transition. Loot offers useful equipment/consumable progression without leveling.
- Commands cannot double-spend resources or mutate state when invalid. Document the supported SRD subset and intentional simplifications, with primary-source attribution.
- Behavior tests cover role actions, costs/rejections, rescue, expiry/death, shrinking-party continuation, wipe, and cross-encounter resource persistence.
- Compile and observe rendered role actions, status feedback, and continuation/end-state behavior. Record reviewed evidence and commit the slice.

## Scope

Use a simple explicit transition fixture if route composition is not yet present. Generated expeditions, save/resume, recruitment, resurrection, leveling and permanent power progression remain outside this ticket.

Tracker: https://github.com/AustinKelsay/expedition-rpg/issues/3
