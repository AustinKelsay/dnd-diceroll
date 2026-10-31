# Ticket #4 — Spec review

Fixed baseline `c1be5672308d0d604bb08c2a1f44c688ffa2fc5f`; candidate `645cdd49697d782c9c08f626a29c78a46398fe25`. Command: `git diff c1be5672308d0d604bb08c2a1f44c688ffa2fc5f...645cdd49697d782c9c08f626a29c78a46398fe25`. Exactly one of two independent read-only gpt-6.1-sol reviewers; no editor or writes. Ticket #4 only, excluding disk persistence.

## Report

No code-level findings in the candidate.

The candidate implements ticket #4’s fresh seeded party, retained truthful route opportunities, bounded connected layouts and valid spawns, route history with concrete supply consequences, exact party carry-over, survivor-only rewards, one explicit take/skip recovery decision, three-combat artifact victory, wipe defeat, and fresh reset. Run commands enforce phase ordering and atomic rejection; presentation has const battle access and cannot bypass the run’s route, reward, or recovery decisions. Board rebuilds explicitly remove and destroy prior tile, obstacle, and unit components.

The snapshot retains run seed/identity, generation progression, encounter seeds, selected opportunities/outcomes, camp decisions, and the authoritative battle’s party, turn, and random state. Disk serialization and restore remain appropriately outside this ticket.

One acceptance step remains pending at the reviewed candidate:

- **Native verification pending**, not a code defect. The ticket requires: “Compile and play an actual rendered route → combat → reward/recovery → finale flow with visible state and controls.” The candidate session explicitly records native acceptance as pending. The passing model tests establish domain behavior, but do not establish actual input delivery, camera framing, readable HUD text, or the rendered ending. Complete and inspect that playthrough before declaring ticket acceptance.

Spec: **0 code findings; native acceptance evidence pending.**

## Owner validation

Owner completed the real keyboard-driven route → first victory → single loot → one camp → new layout → second victory/reward → finale → artifact ending → fresh-new-run flow. Repeated loot/camp and later camp inputs were rejected. Seed, exact resources, earned gear and full reset were visible; inspected PNG paths are in [the session](03-seeded-expedition.md). The pending native composition step is complete. Wipe and smaller-party run continuation are automation-verified in this slice; previous party slice retains its native smaller-party evidence. Package/mouse limitations remain explicit. No additional review delegation.
