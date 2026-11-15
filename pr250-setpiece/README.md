# Evidence for #250 (Waystone set piece), branch feature/250-waystone-setpiece at e8b2634f

- **Bursts**, gallery `setpiece` mode, fixture `victory`, 1920x1080, one shot every 0.1 s of the 4.6 s timeline plus the settled state:
  - `themed/` and `primitives/` hold every frame as a 960x540 JPEG, the shatter (013) and the settled frame as full PNGs, and the manifest and index;
  - `sheet-themed3.png` and `sheet-prim3.png` are contact sheets of t = 0, 0.8, 1.2, 1.3, 1.5, 1.8, 2.4, 3.2, 4.0, 4.6 and settled.
  - Rendered at code head 496a28f2. The later merge of origin/staging (#247) touches no presentation code.
- **Import:** `import/*/diff.txt` is the SHA-256 diff of Content/ and Config/.
  - Run 1 added exactly 9 files and changed nothing (`THEME_FILES_CHANGED 9`, `THEME_SETPIECE_COMPLETE set piece 3/3 meshes`).
  - The rerun changed nothing (`THEME_FILES_CHANGED 0`, empty diff).
- **Gate on the merged head e8b2634f:**
  - `gate2-build.log`: 0 warnings;
  - `gate2-test.log`: Project. 392/392, 0 warnings.

## Review fix (head 3ac9e191)

- The artifact's flare colour no longer compounds. The flare now scales the shard's colour as built.
- `themed-fix/` and `sheet-themed-fix.png` are the themed burst re-rendered at this head: the artifact keeps its cyan shard colour through the hold and settle (before the fix it washed toward white).
- Gate at 3ac9e191:
  - `gate3-build.log`: 0 warnings;
  - `gate3-test.log`: Project. 392/392, 0 warnings;
  - `fix-test.log`: the Waystone tests, 2/2.
