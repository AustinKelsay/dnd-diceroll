# Evidence for PR #214 (#188), head d249faf8

- `balance/before-r8`: revision 8 on this build (DND_BALANCE_REVISION=8). Raw runs/encounters/heroes CSVs.
- `balance/step4`: the first head's kept step (e0311a1), raw CSVs; it had 2 deadlocked stalls on the sunken causeway.
- `balance/final2`: the head (d249faf8), raw CSVs; byte-identical to `step10` (kept). 0 stalls, 0 deadlocks.
- `balance/<step>/summary.*`: every tried step (see report 12's steps table).
- `gate/project-337-e0311a1`, `gate/project-338-d249faf8`: full `Project.` automation reports (index.json, verified.json, automation log) for the first head and this head; `gate/sweep-d249faf8`: the `Balance.Baseline.` sweep at this head.
- `traces/`: the soft lock of the first head (rot2-03:20261119:10, fight 4) and step 8's policy-loop stall (rot2-14:20261132:23, fight 3).

Victories with a death: rotation 16.0% (before) -> 11.8% (first head) -> 11.3% (this head); all parties 16.1% -> 11.7% -> 11.1%.

Integrated head f9dab473 (staging cbda465 merged): gate/project-342-f9dab473 (342/342) and gate/sweep-f9dab473 (CSVs byte-identical to final2/step10).
