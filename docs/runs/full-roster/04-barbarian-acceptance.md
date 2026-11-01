# Slice 2 acceptance and review (#40)

Root review baseline: `16f7c1ac2a596788ecf9ded19caf3119b1862692`; initial source head: `c983a4589e95024902f680bc3db943cd2f662e47`. Two fresh independent reviewers inspected that immutable four-commit range and full #37/#40 discussion. Neither reviewer changed files or reran builds/tests.

Standards axis: no additional documented source-standard breach. It identified the ordinary dice wording regression and invalid-party coverage gap. Possible duplicated protection queries remain a nonblocking heuristic.

Spec axis: three P2 presentation defects: ordinary dice forecasts incorrectly advertised discrete endpoint outcomes; Rage/Shelter overhead markers were hostile despite friendly board markers; protected enemy hits displayed raw dice equaling reduced damage without explaining mitigation. Opposite builder arrows both advancing was a nonblocking usability observation; this matches the current controls contract.

Root addressed each finding through shared presentation queries and committed combat facts. Forecasts explicitly identify discrete outcomes; the formatter uses ranges for ordinary dice. Board/overhead targeting share catalog-derived friendliness. Attack results retain damage prevented and the dice tray explains protection. Invalid-party scenarios now use separate unstarted runs, while valid-party replacement separately exercises active-run protection.

Public regression `Project.Expedition.Presentation.AbilityFeedback` reproduced five expected failures before the fixes: healing range, attack range, Rage marker, Shelter marker, and protection arithmetic. Report: `.local/automation/20261101T222308687510Z/index.json`. The synthetic protected hit specifies an independent worked example: five rolled damage, three prevented, two committed. Native Barbarian tests also verify the actual committed prevention and Wild Bolt's forecast semantics. The first green compilation exposed a missing catalog include; it was corrected before the final build. Its attempted test run used the prior regression binary and is not green evidence.

Fresh pinned build/full automation, package, rendered acceptance and review recheck are recorded in the ledger as completed. This document does not claim those pending steps have passed. All generated reports/captures stay ignored. No binary assets, original saves/editor, endpoints, engine pin, hooks or remotes changed.
