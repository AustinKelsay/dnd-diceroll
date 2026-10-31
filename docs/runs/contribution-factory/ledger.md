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

- Candidate `214801a`: 33/33 tests; index hygiene and real preflight passed. Fresh disposable clone proved missing-LFS refusal, download recovery, doctor and 33 tests, plus preservation of untracked work/index. Reports are ignored under `.local/contribution-proof/`.
- Independent Standards review: PASS, no findings.
- Sol round 1 (Spec/correctness): S1 fixed. A real hydrated-LFS regression reproduced `git status` installing a missing hook; status now uses a command-local null hooks path, while later inspection retains the real configured path. The test verifies hooks/index/worktree preservation. S2 fixed. Noninteractive `gh issue view --comments` omitted the issue body; the workflow now requests body and comments explicitly with JSON fields. Root reproduced both findings before fixing.
- Local CodeRabbit: completed, one hook-path finding declined. `git rev-parse --git-path hooks/pre-push` already respects relative and absolute `core.hooksPath`; direct Git experiments and a regression covering both active custom paths establish this. No production change was needed for that finding.
- Pending Sol recheck, hosted CI, PR feedback, and authorized integration. The real friend/second-machine pilot remains a follow-up; no Unreal build, rendering, or package behavior is changed or newly claimed.

## Local acceptance

Sol round 2 approved candidate `ec7b2ec` (tree `489af3a6ca204184684ba0d7975c9fdfa5e0d79e`): S1/S2 resolved, no new findings; reviewer independently passed all 35 tests, index hygiene, and whitespace checks, including a failing negative control for the original hook bug. Root's actual Mac preflight passes. Standards review passed separately. Local CodeRabbit completed; its sole custom-hook-path report is disproved and documented above. Full feature diff grew from 650 to 691 added lines to include the fixes, regression coverage, and evidence record. No review dispute remains.

Delivery plan: non-draft staging PR, Linux repository CI and PR review feedback, then merge under the owner's explicit full-send authority if all gates pass. Use a merge commit to preserve recorded implementation/review SHAs; leave main and V1 PR #6 untouched. The PR/issue carry the eventual integration result to avoid another evidence-only branch change after CI.
