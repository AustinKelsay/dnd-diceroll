# Expedition

A single-player 3D tactical roguelike: four adventurers, generated encounters, route choices, and permanent death. Built from the reusable [Unreal AI Boilerplate](https://github.com/AustinKelsay/unreal-ai-boilerplate).

Read [the approved game brief](docs/game-brief.md), [V1 spec](docs/specs/expedition-v1.md), and [active run ledger](docs/runs/expedition-v1/ledger.md). [Development commands and editor safety](docs/development.md) remain authoritative. This project targets the pinned UE 5.8.4 build on Apple Silicon Mac. Internal Unreal identifiers remain DnDDiceRoll for compatibility.

Development targets `staging` through reviewed feature PRs. The original starter and its v0.1.0 tag remain unchanged.

Play the sandbox `/Game/DnDDiceRoll/Maps/L_AgentSandbox` in PIE to start a seeded three-combat expedition. Choose Z/X for the displayed guarded/dangerous route. After each of the first two wins, select a conscious survivor with 1–4 and press L for the displayed gear/potion reward. After the first reward, C takes the single camp (+10 capped HP and one capped role resource per survivor); V skips it. The third win recovers the artifact. N at either ending starts fresh with a new seed and no inherited power. Current run history is in memory; disk save/resume follows in ticket #5.

Combat: 1–4 inspect/select, A attack/move, F role ability, R rescue, P potion, arrows+Enter target, Space end turn. Q/E orbit and mouse wheel zoom. Mouse targeting is supported; the keyboard path is the established native acceptance path. Rescue downed allies before their three-turn deadline or the finishing blow; actual deaths are permanent. See [supported rules and adaptations](docs/rules-reference.md).
