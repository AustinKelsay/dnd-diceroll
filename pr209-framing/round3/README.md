# #215 round 3 evidence (source head `322936a6`, docs `1698bde6`)

- `d1-yaw90-180deg-from-home_before-left_after-right_<res>.jpg`: the on-screen D1 frame (far cap, 180 degrees from home, yaw 90) before (`34c4b506`) and after, 1920x1080 and 1280x720: the black band is gone; the camera-side wall dithers.
- `vault-far-cap_24-yaws_<res>.jpg`: the far cap at every 15 degrees of yaw at 1920x1080, 2560x1080 and 1280x720.
- `black-band-results.txt`: `tools/black_band.py` over the 144 captures (none), and yaw 90 at the previous head (49 rows at 1080p, 32 at 720p).
- `tests-round3.md`: what the new rendered tests cover and what they report.
- `vault-contrast-spot-check.md`, `perf-report.md`: the Vault contrast spot check and the perf gate on the package of the head (`Builds/20261113T172128961623Z`).
