# #209 camera framing: evidence for PR #215 head `34c4b506`

PlebDev Ultra, UE 5.8.4. Galleries: `tools/capture/framing-poses.json` (Vigil), `tools/capture/vault-far-poses.json` (Vault), `tools/capture/palette-poses.json` (contrast). Gate numbers are in the PR comment. The first round's evidence (head `b2d74758`) is kept in `../b2d74758-superseded/`.

## Vigil rose (finding 2, on-screen note 3)

`vigil-rose-yaw-90_close900-close1300-default-far-6400_<res>.jpg`: the near pan stop at yaw -90 (the home yaw), panels left to right and top to bottom: zoom 900, 1300, the default (locked axis, home is the near stop), the far pose, 6400, at 1920x1080 and 1280x720. `vigil-rose-yaw-45-top_yaw-135-bottom_close1300-default-far_<res>.jpg`: the same at the diagonals. No rose and no coloured pool behind the action bar at any of them.

## Vault far cap (finding 3, finding 1)

`vault-far-cap_24-yaws_every15deg_<res>.jpg`: the far cap at every 15 degrees of yaw, 1920x1080, 2560x1080 and 1280x720. `vault-far-cap_panned-to-4-stops_6-yaws_1920x1080.jpg`: panned to each stop at six yaws (the pans barely differ: the cap and the set clamp leave no room). `vault-far-cap-footprints.md`: the viewport's ground per pose, 27.3 to 27.8 m at every pose and aspect (the set is 27 m; the offline port reads about 0.5 m high). The rendered pose during orbits and wheel notches, at 30 Hz, is `Project.Expedition.Presentation.VaultNeverShowsPastTheSet`.

## Gates

`canonical-contrast-palette-gate.md` (and `.json`): `tools/palette_report.py` over the 54 palette fixtures (70 builds x 3 boards x 5 default-zoom shots) at 1920x1080: 70 builds measured, 0 below the gate. `perf-report.md`: the perf gate on the package of the head.
