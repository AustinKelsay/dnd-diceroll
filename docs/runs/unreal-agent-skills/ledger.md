# Unreal agent skills and recovery

- Issue/spec and approved acceptance: [#10](https://github.com/AustinKelsay/dnd-diceroll/issues/10).
- Owner: this full Codex implementation session; human owner Austin. Branch `feature/unreal-agent-skills`, fixed baseline `a6aeeb47ffa50e97dfce969fd34c911d2f30739a` (`origin/staging`).
- Authorization: owner approved the preceding three-part recommendation and asked to investigate adding the optional recovery proxy. This covers scoped implementation, verification, review, commits, and a staging PR. It does not transfer the active game editor or authorize an engine upgrade, new transport, or main promotion.
- Skill setup: repo contribution workflow already exists; no global skill changes. Initial implementation used Feature Dev and writing-for-agents with the approved scope above. One bounded ticket; tests exercise the new verifier's refusal/readback boundary and the actual engine skill registry.
- Workspace: isolated existing contribution checkout, clean before branch creation. Root checkout and its gameplay worker remain untouched. No asset reservations or binary changes.
- Editor ownership: the game worker retains its interactive editor. This run may build/run a closed-project headless commandlet in its own checkout, with MCP auto-start disabled; no interactive launch or interruption test.

## Findings and decisions

- Owner update on 2026-10-31: use Matt Pocock's skills directly and remove the personal Feature Dev launcher from this repository's workflow. `AGENTS.md`, onboarding, and the workflow now state that policy. Earlier launcher references record historical execution only; new and resumed work follows the current repo contract. The owner is deprecating the global skill separately; this patch changes no global skill files.

- Epic plugin inspected at `0e27bde2b55c5d3f6948532f4296aaa3654d8f4b`, version 3.1.2, MIT. Existing MCP server/toolsets already supply its engine-side capabilities.
- Pilot uses the installed `toolset_registry.agent_skill` decorator and a text-backed Python class. This keeps one reviewable Markdown instruction body and avoids a binary skill asset. Startup imports it alongside the existing status toolset.
- Official proxy absent from the pinned installation's documented `Extras/Proxy` location; authenticated `EpicGames/UnrealEngine` access returned 404. No compatible official binary/source was available. Preserve direct HTTP and document its fresh-session recovery procedure plus the proxy follow-up's acceptance checks. Proxy-enabled checks/transport migration are deferred until an official compatible distribution is obtainable.
- Optional Claude install remains contributor-selected. `CLAUDE.md` imports the shared contract; project compatibility takes precedence over generic upstream defaults.

## Verification and review

- Red: two verifier tests fail because the verifier does not exist yet; tests cover open-editor refusal and missing/wrong project, engine, or instruction-digest readback.
- Green: 37/37 helper tests, including both new verifier tests; `doctor` and the staged repository hygiene check pass.
- Pinned Mac Development editor target built successfully in 92 seconds. No C++ or module changes.
- Real engine readback: startup imported the pilot; reflected `ListSkills` found `/Game/Python/unreal_starter/skills_PY.ProjectAuthoringSkill`; `GetSkills` returned the Markdown verbatim. Engine `5.8.4-58902052+++UE5+Release-5.8`; instruction SHA-256 `11a7c9f4eac15226ffbcf9e087bdad4eed23a1e9a1c4f4a36f86575289b26a16`.
- The first engine probe exposed absent ordinary Python glue for the AICallable functions. The verifier now invokes the actual reflected functions through `call_method`; it does not bypass discovery/readback.
- Real negative control: temporarily removing the startup import made verification fail with `Project skill was not imported by editor startup` and produced no verified report. Restoring it passed again. Final positive evidence is `.local/skills/20261031T153559741787Z/`; negative evidence is `.local/skills/20261031T153527991898Z/`. Local paths are reproduction aids; selected results are repeated above for reviewers.
- Commandlets generated an unrelated Android settings block in this previously clean isolated checkout's `Config/DefaultEngine.ini`. That commandlet-generated change was restored; no generated settings/tokens are in the patch.
- Candidate implementation commit: `ad5230d8bca820d16069fc5fc83d2da2064d2956`.
- Independent Standards review: PASS, with one factual wording nit corrected in the startup docstring and editor recipes. No code changes requested.
- Independent Spec review: APPROVE, no findings. Reviewer independently passed all 24 tests in `test_dev.py` and inspected the actual positive/negative engine logs. Neither reviewer operated Unreal/MCP.
- Local CodeRabbit 0.9.2 review completed across all 13 changed files with 0 issues (`.local/coderabbit-unreal-agent-skills.txt`). Independent review plus this additional check satisfy this run's review gate; CodeRabbit is optional under the repo workflow.
- Final follow-up only corrects startup wording and records evidence/reviews; it changes no tested behavior. Repository hygiene and whitespace checks pass on the final staged patch.

## Handoff

- Delivery: commit/push the reviewed branch and open a non-draft staging PR referencing #10. The issue/PR carry the live integration state; a maintainer closes #10 after staging integration. Main promotion remains separate.
- Known limits: no official proxy installed, no Claude installation/client UI trial, and no live interactive MCP restart test while the game editor remains owned by another thread.
