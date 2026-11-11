# The Vigil stained-glass rose at full resolution (issue #149)

The Watchtower board's edge has a large rose window. It is in the occluder mesh (`SM_World_Vigil_Occluders` and its LODs), and its texture was a
**128 x 128** region of the shared 2048 atlas: about 40 texels/m where the rest of the Vigil is 256. The lead's finding on #197 (the cause is
texture density, not LOD) is the premise here: this directory is the fix, and only that.

The lower 60 % of the Vigil atlas is empty, so a **1024 x 1024** rose region (about 320 texels/m, with 16 px of wrapped padding, mip-safe to mip 4)
goes there, and only the vertices that sampled the old region are re-pointed. Everything else is untouched: positions, normals, `COLOR_0`, indices, every
other UV and every other atlas pixel are byte-identical. `SM_World_Vigil` (the world mesh) has no vertex in the rose region (the patch asserts it), so it and its
atlas are not touched at all.

```sh
blender -b --python Art/dark-fantasy-sources/r002-rose/scripts/patch_rose_vigil.py       # Art/world-vigil-occluders/r001/*.glb -> r002/*.glb (LOD0..LOD2), T_World_Vigil_Atlas_rose.png
blender -b --python Art/dark-fantasy-sources/r002-rose/scripts/validate_rose_vigil.py    # exits non-zero on any failure; writes rose_metrics.json
```

Under the machine lock on the shared host. `rose_lib.py` is shared (glb and PNG read and write, the old atlas layout replayed from `r002/world_metrics.json`, the full-resolution rose).
The new rose is the same design as `r001/scripts/glass.py`'s `rose_window` (twelve petals, five palettes, lead rings, the same sector colours) sampled at 1024 x 1024 and
supersampled 2 x 2; the 128 px shimmer terms are rescaled to the design, not the pixel grid, and the corners outside the disc are lead, not black.

`validate_rose_vigil.py` checks, for each of the three exports against its r001 source: the JSON structure, positions / normals / `COLOR_0` / indices byte-identical, every
non-rose UV byte-identical, rose UVs map back exactly and stay inside the new region, the atlas differs only inside the new region, the sampler is unchanged, the texel density of the
rose triangles before and after (UV area in texels over world area in m^2), and an appearance check (the same points on the same triangles read from the old and the new atlas).

Outputs: `world-vigil-occluders` r002 (a reimport at the same package), `T_World_Vigil_Atlas_rose.png`, `patch_report.json`, `rose_metrics.json`.
