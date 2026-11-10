# PR #173 evidence at 723938d

- Gallery shots of the four combat hints (themed and primitives, 1920x1080 and 1280x720); each raised by the capture hint pose on the fixture showing its cause. Not an on-screen pointer or keyboard check.
- red-run-before-fixes.txt: the automation run of the two review-fix tests written first, against the code before the fixes. Skip tips surviving a New Run, a Potion going 2 to 1 raising no hint, and the state-machine assertion all FAIL; the other hint tests pass. The fixes are commit 723938d, after which the same 8 hint tests pass.
- The original 158 tests were written together with the implementation, not before it; there is no red run for them.

Gate at 723938d: build 0 warnings; Project. 236/236, 0 warnings; helper tests 85 OK; contribute check PASS. The checkout is not a fresh clone.
