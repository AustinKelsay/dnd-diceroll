# November 10 incremental release evidence

Candidate: `0f3042ebc1e7622b9f8559e9dfdd988667e486ca`, tree identical to staging `15e1677b9541166ec15ccfda5c4f5e36d101112e`. Main history was merged without file changes. Release PR #195, tracking #194.

Host: PlebDev Ultra/Linux x86_64, NVIDIA RTX 4080 SUPER, pinned UE 5.8.4 CL 58902052. Every Unreal job acquired the shared machine lock and used `--cores 2`.

Fresh commands: `python3 tools/dev.py doctor`; `python3 tools/contribute.py preflight`; `python3 -m unittest discover -s tools/tests`; `python3 tools/contribute.py check`; `git lfs fsck --pointers`; `python3 tools/dev.py --cores 2 build`; `python3 tools/dev.py --cores 2 test --filter Project.`; `python3 tools/dev.py --cores 2 package`.

Results: build zero warnings; Project. 287/287, zero warnings and clean lifecycle; 170/170 helpers; package zero errors/warnings and normal exit logged (helper-requested exit 143). Package manifest records candidate commit and clean source. Autoplay/performance report will be added after all three passes.

Independent review/native slice provenance: merged PRs #137, #154–#156, #159–#164, #166–#167, #172–#173, #175–#176, #178, #180–#183, #186, #191–#192. The release adds no gameplay or assets. #138 gate 2 was accepted on the integrated slice; #191/#192 subsequently fix the two most visible findings (roll card and hit bloom). Existing on-screen evidence is on `evidence/onscreen-slice-871185a`, `evidence/onscreen-pr172-415a86d` and `evidence/onscreen-pr186-94c9754`.

This incremental release does not close the unfinished #138/#152 graphics overhaul or #112 gameplay program. Open PRs #177/#185/#193 are excluded. Linux success does not resolve Mac #32/#35. Linux failed-automation shutdown crash #157 remains tracked; editor capture compilation race #174 is mitigated, not proved eliminated. Physical-input, pan/zoom timing and human difficulty/fun limits from prior reports remain explicit.

Final packaged passes: warm-up, measured fight and fresh-process Resume all exit 0, no retries. Measured p95 frame 3.23 ms, GPU 2.71 ms, game thread 1.19 ms, peak RSS 1236.9 MiB. Passes both helper budgets and tighter post-#148 budgets (4/3.5/1.5 ms; 1.5 GiB). Worst route load frame 59.5 ms; Resume 56.9 ms. On-screen Wayland 1920×1080, sound enabled, VSync off; automatic public-input policy, not a physical player. No appearance or human balance claim inferred from performance.
