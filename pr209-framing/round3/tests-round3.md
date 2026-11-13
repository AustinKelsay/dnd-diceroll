# Round 3 automation (head `322936a6` source; full `Project.` 345 of 345, 0 warnings)

Rendered through the real eased camera at 30 Hz unless noted.

- `VaultNeverShowsPastTheSet`: 25,980 frames; three aspects, every 15 degrees of yaw, wheel to the stop, panned to each stop, orbits from the far cap in both directions with wheel notches during the easing; the footprint stays inside the 27 m set and no row of the frame is mostly opaque camera-side wall.
- `VaultEdgeUnitsStayInViewAtTheFarCap`: 4 board sizes (16 to 18 by 11 to 12), the acting hero on each board corner, 1080p and 720p, every 15 degrees of yaw, wheeled out to the cap: 768 settled poses, 107,520 frames. Each frame: the unit's tile and head are in the safe rect, a locked axis is centred apart from the unit hold, the footprint is inside the set unless `ShowsPastSet()`. 334 of the 768 poses report `ShowsPastSet()`: even the default zoom shows past the set at that focus, the pose stays at the default, and the focus is not moved.
- `VaultShakeStaysInsideTheSet`: 7,200 frames under a constant `Shake(1.0)` through capped poses, an orbit and the focus easing, with wheel notches, units on the corners: every viewport corner stays inside the set (the shake amplitude is scaled down, to nothing at worst) and the unit stays in the safe rect.
- `VaultKeyboardPanStaysInsideTheSet`, `VigilKeyboardPanKeepsTheBoundAndTheRoseFaded`: Shift+W/A/S/D through the controller (1,952 and 3,276 frames; the keys moved the camera).
- `VaultNearWallFadesLikeTheOtherOccluders`, `VigilRoseFadesBehindTheDockAtEveryZoom` (256 rose points in the bottom third over 8 zooms), `VigilNearSideGeometryFades`, `CameraRigKeepsTheViewInsideTheSet` (1,000 focus and yaw cases: 215 capped below the far pose, 785 report that even the default does not fit at that focus).
- Mutation: with the Vault's camera-side fade disabled, `VaultNeverShowsPastTheSet` fails with 112 band errors (yaws 90, 105, 75, 0, 15, 45, 60, 120 and others).
