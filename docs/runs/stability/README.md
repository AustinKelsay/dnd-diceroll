# Stability investigations

Delivery records for the crash issues that are parked as known, reported risks. These remain accepted, tracked risks. #157 has only been observed after failing tests; #32 and #35 affect Mac gameplay and startup. The detail behind each is paraphrased here; the engine-source notes that support them are private (see each page).

| Issue | Host | Status | Next action |
| --- | --- | --- | --- |
| [#157](157-vt-shutdown.md) Virtual-texture crash at shutdown of a failed automation run | Linux (Ultra) | Trigger confirmed (13 of 13 failing runs crashed, 0 of 15 passing), cause identified; known, reported, low impact. Open. | None active |
| [#35](35-llm-startup.md) Packaged startup crash in the LLM tracker | Mac | Cause explained from source and evidence; not validated. Open. | Checks on the next Mac package: Shipping, then `-LLM`, then direct exec vs `open` |
| [#32](32-renderer.md) Scene-culling assertion after Resume | Mac | Unresolved; one exact capture; a 7,000-cycle Linux soak did not reproduce. Open. | None active |

A passing Linux run says nothing about the two Mac issues, and a Mac result says nothing about Linux. `tools/dev.py test` now reports a shutdown crash as a failed lifecycle even when tests also fail ([PR #161](https://github.com/AustinKelsay/dnd-diceroll/pull/161)).
