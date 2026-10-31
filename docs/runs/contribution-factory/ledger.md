# Contribution factory — delivery ledger

- Spec/implementation issue: https://github.com/AustinKelsay/expedition-rpg/issues/7
- Owner: AustinKelsay; implementation session gpt-6.1-sol.
- Branch: feature/contribution-factory; base staging at ea2a3c0047064668a018e5479546d7884dc12b83.
- Authority: owner approved the council plan and requested end-to-end full send, including routine test/implementation decisions and reversible merge of this feature into staging when review/check gates pass. V1 PR #6 and main promotion remain outside scope.
- Execution: this dedicated session implements one coherent ticket in an isolated clone; independent review uses fresh reviewers. No gameplay edits, assets, editor/MCP mutation, or changes in the game thread's checkout.
- Approved slices: contribution contract + portable pipeline; read-only checks with tests; templates/CI + fresh-clone validation.
- Record ownership: GitHub issue is canonical scope/progress; this ledger preserves review/evidence/handoff.

## Evidence in progress

- Initial clone exposed Python 3.9 selected by its shell; explicit Homebrew Python passed doctor. Guide now selects Python 3.11+ before running checks.
- New contribution tests first failed importing the absent public check module; implementation then passed 9/9 tests against disposable Git repositories, including byte-preserved index/worktree on dirty and interrupted-state refusal.
- Existing editor helper is unchanged; no Unreal build is needed for these Python/docs/workflow changes. The first-friend/second-machine user study remains a follow-up, distinct from automated fresh-clone proof.

## Review and delivery

Pending final local checks, fresh-clone exercise, independent Standards/Spec and Sol review, CodeRabbit, CI, PR, and authorized integration.
