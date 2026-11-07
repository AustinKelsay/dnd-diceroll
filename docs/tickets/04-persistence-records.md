# Resume a run and record its ending

## Parent and dependency

[Expedition V1](https://github.com/AustinKelsay/dnd-diceroll/issues/1). Blocked by: Complete a seeded expedition.

## Outcome

A player can save and resume the current expedition, including active combat, without rerolling outcomes or reversing deaths, and inspect history after winning or losing.

## Acceptance criteria

- Versioned, validated persistence includes run phase/identity, party stable IDs, deaths, health/resources, inventory, route history, encounter/board state, turn/action state and RNG.
- Restoring and submitting the same next commands produces the same observable results as uninterrupted play.
- Invalid, incompatible or truncated saves fail clearly. A failed save must preserve the prior valid file; loading cannot partially mutate the live run.
- UI offers save/resume, fresh run and run records with understandable feedback. Ending records are idempotent and grant no combat power.
- Tests exercise real serialization round trips and continued commands, invalid snapshots, write failure behavior, irreversible losses and record/reset semantics.
- Finish helper tests, scoped Unreal automation, bootstrap validation, current-project identity, real rendered PIE interaction and captures, Development packaging on the validated host (Mac at the time; PlebDev Ultra/Linux since #125), and visible packaged play/save/resume observation. Record exact checks and limitations.
- Run independent Standards/Spec and integrity review, resolve findings, and commit the reviewed slice. Root handles final CodeRabbit and the non-draft PR to staging.

## Scope

Local single-player saves only. No accounts/cloud sync, production distribution, multiplayer, automatic main merge or guarantees beyond the documented file adapter contract.

Tracker: https://github.com/AustinKelsay/dnd-diceroll/issues/5
