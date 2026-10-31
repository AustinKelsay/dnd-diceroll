# Expedition V1 — Feature Dev Ledger

## Run

- Run ID: expedition-v1-20261030
- Loop: plebdev-feature-dev (validated v0.4.0)
- Target repo: AustinKelsay/expedition-rpg (private; derived from the unchanged unreal-ai-boilerplate baseline)
- Base: staging, created at ea2a3c0047064668a018e5479546d7884dc12b83
- Feature branch: feature/expedition-v1
- Owner: AustinKelsay; orchestrator owns the branch and editor ownership transfers
- Status: tactical-battle implementation delegated; SRD fact check in parallel
- Skill setup: GitHub issues, canonical labels, single-context domain docs
- Models: implementation/support/review workers use gpt-6.1-sol as explicitly requested; inherited high reasoning

## Goal

Deliver the approved party-based 3D tactical roguelike V1, focusing on reusable, scalable foundations, tests along the way, structured documentation and reviewed PR delivery. The user explicitly asked the orchestrator to lead end to end and surface questions only when needed.

## Decisions and authority

The product interview and V1 brief are approved. The user delegated routine engineering, testing, documentation, ticket granularity, and orchestration decisions. This overrides repeat confirmation prompts for setup, technical test seams, and ticket dependency planning in the launcher defaults. Scope-changing intent, irreversible product choices, and unavailable privileged access still escalate. No further product question is presently blocking.

Use GitHub, staging, one feature branch, per-ticket commits, public-interface tests, two-axis code review, local CodeRabbit and bounded PR review/fallback. No production operations. No merge to main is authorized by this pipeline's completion condition.

## Durable artifacts

- Approved brief: docs/game-brief.md
- Context/glossary: CONTEXT.md and GLOSSARY.md
- Architecture: docs/architecture.md and ADR-0001
- Spec: docs/specs/expedition-v1.md — https://github.com/AustinKelsay/expedition-rpg/issues/1
- Tickets: docs/tickets/ — published with native parent/dependency links
- Session/review evidence: docs/runs/expedition-v1/
- Local machine evidence: ignored .local/ and Builds/
- PR: pending

## Commands

- Host: /opt/homebrew/bin/python3 tools/dev.py doctor
- Helper tests: /opt/homebrew/bin/python3 -m unittest discover -s tools/tests
- Compile: /opt/homebrew/bin/python3 tools/dev.py build (editor closed)
- Asset fixture: /opt/homebrew/bin/python3 tools/dev.py bootstrap
- Engine tests: /opt/homebrew/bin/python3 tools/dev.py test --filter Project.Expedition.
- Editor: status → close → build → editor; one owner, sequential MCP
- Observation: PIE, real interaction, capture and inspected PNG; tools/dev.py smoke remains the starter fixture check
- Packaging: /opt/homebrew/bin/python3 tools/dev.py package plus visible packaged verification
- Review: coderabbit review --agent --type all --base staging, or recorded fresh Sol review fallback

## Ticket ledger

| Slice | Blocked by | Owner/status | Evidence |
| --- | --- | --- | --- |
| Play a tactical battle | none | sole implementation worker; starting from 31a48fb0342013de151461113fc88be727c5d686 | setup doctor and 22 helper tests pass |
| Survive losses and use party roles | battle | pending | pending |
| Complete a seeded expedition | losses/roles | pending | pending |
| Resume a run and record its ending | expedition | pending | pending |

## Editor ownership

Original editor PID 80712 was verified clean and exited normally through the helper before game changes. One implementation worker at a time may build or operate the editor. Read-only support never calls MCP, launches, or closes Unreal. Root confirmed the close; the next implementation worker owns the editor until it reports completion.

## Escalations and parked work

None. Agent-performable local/visual/package checks are AFK. Other platforms and production distribution are out of scope. Aesthetic tuning can follow the verified simple presentation without blocking the first playable.

## Setup evidence

2026-10-30: created the private game repository; baseline main/staging pushed, original remote retained as boilerplate. Published spec #1 and tickets #2–#5 with native parent/dependency relationships. Doctor passed and all 22 helper tests passed. Setup commit: 31a48fb0342013de151461113fc88be727c5d686. No PR yet.

## Tracker

- Play a tactical battle: https://github.com/AustinKelsay/expedition-rpg/issues/2
- Survive losses and use party roles: https://github.com/AustinKelsay/expedition-rpg/issues/3
- Complete a seeded expedition: https://github.com/AustinKelsay/expedition-rpg/issues/4
- Resume a run and record its ending: https://github.com/AustinKelsay/expedition-rpg/issues/5
