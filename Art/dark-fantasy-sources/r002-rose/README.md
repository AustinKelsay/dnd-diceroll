# The Vigil stained-glass rose at full resolution (issue #149)

The Watchtower board's edge has a large rose window. It is in the occluder mesh (`SM_World_Vigil_Occluders` and its LODs), and its texture was a **128 x 128** region of the shared 2048 atlas.
The window is about **9 m across** (the rose triangles total 63 m^2 of window, 126 m^2 counting the doubled back faces), so that region was **14.2 texels/m** where the rest of the Vigil is 256. (The first
estimate on #197 was about 40 texels/m; the measurement below is the number.) The lead's finding on #197, that the cause is texture density and not LOD, is the premise here: this directory is the fix, and only that.

**What shipped.** The atlas becomes **4096 x 2048**: the old 2048 x 2048 image stays pixel for pixel in the left half, so every other vertex keeps its texel; only its **U coordinate is rescaled** (u' = u x 2048 / 4096), V is unchanged.
A **2000 x 2000 rose region** goes at atlas pixel **(2072, 24)** in the right half, with **24 px** of wrapped padding (mip-safe past mip 4); only the vertices that sampled the old region (262 in LOD0, 196 in each LOD) are re-pointed to it.
**14.2 -> 222.2 texels/m** (15.6x), 87 % of the paving's 256 (a 256 rose needs 2304 px and will not fit 2048). Positions, normals, `COLOR_0`, indices and every other atlas pixel are unchanged. `SM_World_Vigil` (the world mesh)
has no vertex in the rose region (the patch asserts it), so it and its atlas are not touched at all. The first attempt, a 1024 x 1024 region inside the 2048 atlas (113 texels/m), was abandoned: it did not reach the target.

```sh
blender -b --python-exit-code 1 --python Art/dark-fantasy-sources/r002-rose/scripts/patch_rose_vigil.py       # Art/world-vigil-occluders/r001/*.glb -> r002/*.glb (LOD0..LOD2), T_World_Vigil_Atlas_rose.png
blender -b --python-exit-code 1 --python Art/dark-fantasy-sources/r002-rose/scripts/validate_rose_vigil.py    # exits non-zero on any failure; writes rose_metrics.json
```

Under the machine lock on the shared host. **Exit codes:** Blender exits 0 when a script raises, so every command line here carries `--python-exit-code 1` (an error before the script's own guard, such as a failed import,
then exits 1) and every entry point runs through `run_guarded`, which prints an unexpected exception and exits 1 (a check that fails exits 1 as well). A validator that did not run to its end never reports success.
`rose_lib.py` is shared (glb and PNG read and write, the old atlas layout replayed from `r002/world_metrics.json`, the full-resolution rose).
The new rose is the same design as `r001/scripts/glass.py`'s `rose_window` (twelve petals, five palettes, lead rings, the same sector colours) sampled at 2000 x 2000; the 128 px shimmer terms are rescaled to the design, not the pixel grid, and the corners outside the disc are lead, not black.

`validate_rose_vigil.py` checks, for each of the three exports against its r001 source: the JSON structure, positions / normals / `COLOR_0` / indices byte-identical, every non-rose UV on its old texel (u halved), rose UVs map back
exactly and stay inside the new region, the left half of the new atlas equals the old atlas and the right half is background outside the new region, the sampler is unchanged, the texel density of the rose triangles before and after
(UV area in texels over world area in m^2; the gate is 215, 85 % of 256), and an appearance check (the same points on the same triangles read from the old and the new atlas).

Outputs: `world-vigil-occluders` r002 (a reimport at the same package), `T_World_Vigil_Atlas_rose.png`, `patch_report.json`, `rose_metrics.json`.
