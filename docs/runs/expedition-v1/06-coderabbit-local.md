# Local CodeRabbit gate

- Scope: complete branch versus `staging`.
- Command: `coderabbit review --agent --type all --base staging`.
- Reviewed functional head: `83cfb638c98a4278710bc66b585986d4d3cc51cc`.
- Started: 2026-10-31 approximately 04:57 UTC; completed by 05:03 UTC.
- Availability: completed, two major findings; no fallback required.
- Raw generated report: ignored `.local/coderabbit-local-v1.log`.

| Finding | Decision | Resolution |
| --- | --- | --- |
| Startup New Run could replace unfinished saved progress | Valid; fix | Session rejects replacement of unfinished live or on-disk runs. Startup feedback directs the player to O resume. |
| Unreadable/incompatible save permanently blocked new runs | Valid; fix | Explicit New Run at startup preserves the original under a unique sibling `.unreadable-…` name before replacement. Resume stays strict; a live session never archives its checkpoint. Feedback discloses recovery, and a failed replacement retains the preserved original. |

Both public regressions failed against the pre-fix compiled source: `.local/final-startup-red-test.log` and `.local/final-recovery-red-test.log`. Startup input is also checked through the public presentation adapter. Recovery tests cover truncated/future-version documents, exact original-byte preservation, live-run refusal and replacement write failure. Final green build, automation, package and native evidence follow in the root delivery record.

One attempted second red test while the first still owned the helper lock was refused; it was rerun after completion and produced the intended regression failure. No concurrent editor operation occurred. Root resolved these bounded findings directly; no extra implementation agent was started.
