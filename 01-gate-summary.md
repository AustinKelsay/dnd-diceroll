# Merge gate evidence for PR #166 (#58 PR A: action facts)

- **Head tested:** `39b5606c` on `feature/58-action-clarity`, which already contains `staging` `b02b5c3`.
- **Host:** PlebDev Ultra, Linux x86_64, Unreal Engine 5.8.4, clang 20.1.8, `--cores 2`, one job at a time under the machine lock.
- **Checkout:** fresh worktree that had never been built, so every project file compiled.
- **Date:** 2026-11-09 (UTC).

| Step | Command | Result |
| --- | --- | --- |
| Machine check | `python3 tools/dev.py doctor` | all checks OK |
| Helper tests | `python3 -m unittest discover -s tools/tests` | pass |
| Hygiene | `python3 tools/contribute.py check` | PASS |
| Build | `python3 tools/dev.py --cores 2 build` | succeeded, 0 warnings |
| Automation | `python3 tools/dev.py --cores 2 test --filter Project.` | 142/142 passed, 0 warnings, exit 0 |
| Tracked files | `git status --short` after the runs | clean |

Files here: `02-automation-verified.json` is the helper's verdict, `03-automation-tests.json` lists every test with its state, and `04-build-log-tail.txt` is the end of the build log.

Not exercised: rendered or on-screen play. PR A changes no presentation code.
