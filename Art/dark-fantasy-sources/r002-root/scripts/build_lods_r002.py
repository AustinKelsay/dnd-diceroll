"""Author the explicit LODs of the r002 meshes (issue #145 phase 2; policy: explicit LODs for meshes over ~1k triangles).

  blender -b --python Art/dark-fantasy-sources/r002/scripts/build_lods_r002.py        (Blender 5.2.3 LTS, as phase 1)

For every mesh in LOD_SET writes <name>_LOD1.glb (about 50 % of the triangles) and <name>_LOD2.glb (about 25 %) beside its LOD0
glb, and Art/dark-fantasy-sources/r002/lod_metrics.json.  The LOD0 glbs are only read.  Decimation is Blender's collapse
decimator on the welded mesh with UV-island borders kept as seams; UVs, 16-bit vertex colour (including the glow/opacity alpha)
and the atlas come through unchanged.  Run validate_lods_r002.py afterwards in a fresh process; then rerun write_records_r002.py.
Cards (mist, shafts) and scatter under ~1,000 triangles are exempt.
"""
import hashlib
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lod_r002 as D
from r002_lib import bpy, np, ROOT

SRC = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r002')
FRACTIONS = (0.5, 0.25)

# (asset id, revision, export name): the world, its occluders, the nine obstacle variants, the arch, and the one scatter prop over 1,000 triangles.
LOD_SET = [
    ('world-vigil', 2, 'SM_World_Vigil'), ('world-vigil-occluders', 1, 'SM_World_Vigil_Occluders'),
    ('prop-pillar', 2, 'SM_Prop_Pillar'), ('prop-pillar-b', 1, 'SM_Prop_Pillar_B'), ('prop-pillar-c', 1, 'SM_Prop_Pillar_C'),
    ('prop-wall', 2, 'SM_Prop_Wall'), ('prop-wall-b', 1, 'SM_Prop_Wall_B'), ('prop-wall-c', 1, 'SM_Prop_Wall_C'),
    ('prop-stones', 2, 'SM_Prop_Stones'), ('prop-stones-b', 1, 'SM_Prop_Stones_B'), ('prop-stones-c', 1, 'SM_Prop_Stones_C'),
    ('prop-arch-2cell', 1, 'SM_Prop_Arch2Cell'), ('scatter-roots', 1, 'SM_Scatter_Roots'),
]


def export_dir(asset_id, rev):
    return os.path.join(ROOT, 'Art', asset_id, 'r%03d' % rev)


def glow_area(P, C):
    """Area (m2) weighted by the corner-mean alpha."""
    a = np.linalg.norm(np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]), axis=1) * 0.5
    return float((a * C[..., 3].mean(1)).sum())


def main():
    metrics = {}
    for asset_id, rev, name in LOD_SET:
        folder = export_dir(asset_id, rev)
        lod0 = os.path.join(folder, name + '.glb')
        P, UV, C, png = D.read_glb(lod0)
        img = bpy.data.images.load(png)
        px = D.atlas_pixels(img)
        entry = dict(asset_id=asset_id, revision=rev, lod0_triangles=len(P), lod0_glow_area_m2=round(glow_area(P, C), 5), lods=[])
        big = asset_id.startswith('world-vigil')
        for index, fraction in enumerate(FRACTIONS, start=1):
            # Aim for the fraction; if the silhouette gate fails (small pieces lose thin features at 25 %), keep more triangles in 10 % steps.
            wanted = fraction
            while True:
                P2, UV2, C2 = D.decimate(f'{name}_LOD{index}', P, UV, C, img, int(round(len(P) * wanted)))
                path = D.write_lod(f'{name}_LOD{index}', P2, UV2, C2, img, os.path.join(folder, f'{name}_LOD{index}.glb'))
                # What the exporter actually wrote (it drops degenerate triangles) is what counts: read the file back.
                P2, UV2, C2, _ = D.read_glb(path)
                ok, fwd, back, move = D.silhouette_ok(P, P2, big, index)
                if ok or wanted >= 0.40:
                    break
                print(f'  {name} LOD{index} at {len(P2)} triangles fails the silhouette gate (distance {fwd:.3f}/{back:.3f} m, box {move:.3f} m); keeping more')
                wanted = min(0.40, wanted * 1.1)
            if not ok:
                raise RuntimeError(f'{name} LOD{index}: no acceptable decimation up to 40 % of the triangles')
            dev = D.deviation(P, P2, samples=20000)
            app = D.appearance_error(P, UV, C, P2, UV2, C2, px)
            row = dict(lod=index, triangles=len(P2), fraction=round(len(P2) / len(P), 4), file=os.path.relpath(path, ROOT).replace(os.sep, '/'),
                       sha256=hashlib.sha256(open(path, 'rb').read()).hexdigest(), bytes=os.path.getsize(path),
                       deviation_m_p50_p95_max=[round(v, 4) for v in dev], appearance=dict((k, round(v, 4)) for k, v in app.items()),
                       glow_area_m2=round(glow_area(P2, C2), 5))
            entry['lods'].append(row)
            print(f'LOD {name} LOD{index}: {len(P)} -> {len(P2)} tris ({row["fraction"]:.2f}), deviation p50/p95/max {row["deviation_m_p50_p95_max"]} m, appearance {row["appearance"]}')
        metrics[name] = entry
    with open(os.path.join(SRC, 'lod_metrics.json'), 'w') as handle:
        json.dump(metrics, handle, indent=2)
        handle.write('\n')
    print('LODS DONE', len(metrics))


main()
