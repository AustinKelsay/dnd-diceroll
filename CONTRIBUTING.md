# Build dnd-diceroll together

Bring an idea and an agent that can read files and run commands. Ask it to **read AGENTS.md and onboard you**. The agent follows [the contribution workflow](docs/agents/workflow.md), using Matt Pocock's skills when installed; you decide what you want to build, and a maintainer approves changes to the shared game.

To install those skills, use `/plugin install mattpocock-skills` in Claude Code or `npx skills@latest add mattpocock/skills` for other agents, and record the installed revision in your run notes. Do not run `/setup-matt-pocock-skills` in this repository: the tracker, labels, and skill locations are already configured, and the workflow works without the skills installed.

Useful prompts:

- “Set up dnd-diceroll on this machine and explain anything missing.”
- “Check for updates and help me resume my work without losing changes.”
- “Help me turn this encounter idea into a small, approved feature.”
- “Find an unclaimed, unblocked ticket I can work on.”
- “Verify my change, arrange independent review, and prepare its PR.”

Claude Code contributors can optionally add [Epic's Unreal skill](docs/agents/unreal-tools.md). That guide also covers the shared project-skill pilot and connection recovery for any agent.

## Join this game

Accept the collaborator invitation to the public `AustinKelsay/dnd-diceroll` repository; pushing a feature branch needs write access, and the checks expect `origin` to be this repository rather than a fork. Use your own GitHub account; the agent can guide authentication without handling your password. Each person uses their own clone and feature branch. [Reusing the starter](docs/reusing-the-starter.md) creates a different game and is not the joining procedure.

Unreal builds and tests are verified on one machine: **PlebDev Ultra**, a Linux desktop with the exact engine build in [tools/versions.json](tools/versions.json), Git LFS, and Python 3.11+. The Mac is no longer a build or test host. On your own machine you can do docs, planning, source, and review work, and build or play if you like, but Unreal results only count once they are verified on Ultra.

First verify `python3 --version`. Every Python command below assumes 3.11+. See [development](docs/development.md) for prerequisites and the authoritative Unreal commands.

With Git LFS installed, the agent can run:

```sh
gh auth status
git clone --branch staging https://github.com/AustinKelsay/dnd-diceroll.git
cd dnd-diceroll
git remote -v
git lfs install --local
git lfs pull
python3 tools/contribute.py check
python3 -m unittest discover -s tools/tests -v
python3 tools/dev.py doctor
python3 tools/contribute.py preflight
```

Inspect existing hooks before LFS installation; preserve custom hooks and the LFS pre-push invocation. Resolve a hook conflict deliberately. The checks never install or replace hooks. On an unsupported host, `check` remains available; `preflight` reports that editor state cannot be verified.

On Ultra, finish the first Unreal baseline using `build`, `bootstrap`, and `test`, then `editor`, `status`, `smoke`, and an inspected `capture`. Save deliberately and `close` normally. Packaging is needed when the work claims packaged behavior. Several agents share Ultra, so get a native slot first (see "Shared machine" in [development](docs/development.md)); one editor owner at a time.

## The contribution agreement

1. Work starts with an issue in this repository. New ideas go through scope/spec/ticket approval; an already approved ticket uses its existing decisions.
2. Claim the issue with an assignee and a comment naming your branch, scope, dependencies, and any shared binary assets you intend to edit. Resolve competing claims before writing.
3. Use `feature/<issue>-<slug>` from the agreed `origin/staging` baseline. One coherent feature can have several ticket commits; one writer owns its branch and checkout at a time.
4. Inspect and preserve unfinished work before integrating updates. Keep the editor closed for branch switches, merges, and LFS downloads. The agent explains dirty files, conflicts, or unknown ownership; it never silently discards them.
5. Keep LFS hooks active and coordinate shared `.umap`/`.uasset` edits by path. Binary conflicts require an intentional resolution with the asset owners through Unreal.
6. Verify the changed behavior, obtain independent Standards and Spec review, then open a non-draft PR into `staging`. Attach accessible evidence and state limitations.
7. A maintainer integrates the PR. Opening a PR alone does not authorize merging or promotion to `main`; any delegated merge authority must be recorded explicitly.

Keep questions about product intent and access understandable. Let the agent handle routine engineering within approved scope. A larger feature uses [the run template](docs/templates/feature-run.md); a small ticket can resume from its issue and PR.

## Your first contribution

`staging` and `main` are protected: changes arrive through a PR whose repository checks pass and which carries one approving review, and the maintainer merges. For your first PRs, the maintainer acts as the Spec reviewer; your agent still arranges an independent Standards review in a fresh session. Ask for a ticket labeled `good first issue` or `ready-for-agent` with no assignee, or file an idea with the issue template and let the maintainer scope it.

If you build or play on your own machine, that evidence is unvalidated: state your host in the PR and ask the maintainer to run Unreal verification on Ultra before integration. Documentation, planning, source, and review work need no engine. Claude Code users get a repository hook that blocks destructive Git commands; other agents rely on the workflow's rules, so inspect any `reset`, `clean`, or force-push your agent proposes before approving it.
