"""Author the explicit LODs of the Vault r002 meshes (issue #149, same method as #145 phase 2; policy: explicit LODs for meshes over ~1k triangles).

  blender -b --python Art/dark-fantasy-sources/r002-vault/scripts/build_lods_vault_r002.py        (Blender 5.2.3 LTS, as phase 1)

For every mesh in LOD_SET writes <name>_LOD1.glb (about 50 % of the triangles) and <name>_LOD2.glb (about 25 %) beside its LOD0
glb, and Art/dark-fantasy-sources/r002-vault/lod_metrics.json.  The LOD0 glbs are only read.  The mesh, node and material inside a LOD glb are named LOD<n>_<name> (the file stays <name>_LOD<n>.glb), the pattern Unreal's Interchange expects;
relabel_lods_vault_r002.py strips any Blender '.001' duplicate suffix.  Decimation is Blender's collapse
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

SRC = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r002-vault')
FRACTIONS = (0.5, 0.25)
SHARP = 25.0                      # degrees; box-built architecture keeps its sharp-edge vertices (see lod_r002.decimate)
SHARP_SET = ('world-vault-occluders',)
ONE_LOD = {'world-vault-occluders': 1}      # box-built architecture: pinned sharp-edge vertices stop a collapse at about 54 %, so LOD2 would equal LOD1 (see README)

# (asset id, revision, export name): the world, its occluders, the nine obstacle variants and the arch (scatter, mist cards and light shafts are exempt: all under ~600 triangles or translucent cards).
LOD_SET = [
    ('world-vault', 1, 'SM_World_Vault'), ('world-vault-occluders', 1, 'SM_World_Vault_Occluders'),
    ('prop-vault-pillar', 1, 'SM_Prop_Vault_Pillar'), ('prop-vault-pillar-b', 1, 'SM_Prop_Vault_Pillar_B'), ('prop-vault-pillar-c', 1, 'SM_Prop_Vault_Pillar_C'),
    ('prop-vault-wall', 1, 'SM_Prop_Vault_Wall'), ('prop-vault-wall-b', 1, 'SM_Prop_Vault_Wall_B'), ('prop-vault-wall-c', 1, 'SM_Prop_Vault_Wall_C'),
    ('prop-vault-stones', 1, 'SM_Prop_Vault_Stones'), ('prop-vault-stones-b', 1, 'SM_Prop_Vault_Stones_B'), ('prop-vault-stones-c', 1, 'SM_Prop_Vault_Stones_C'),
    ('prop-vault-arch-2cell', 1, 'SM_Prop_Vault_Arch2Cell'),
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
        big = True if asset_id.startswith('world-vault') else False
        for index, fraction in enumerate(FRACTIONS[:ONE_LOD.get(asset_id, 2)], start=1):
            # Aim for the fraction; if the silhouette gate fails (small pieces lose thin features at 25 %), keep more triangles in 10 % steps.
            wanted = fraction
            while True:
                P2, UV2, C2 = D.decimate(f'LOD{index}_{name}', P, UV, C, img, int(round(len(P) * wanted)), pin_sharp=(SHARP if asset_id in SHARP_SET else 0.0), pin_glow=not asset_id.startswith('world-vault'))
                path = D.write_lod(f'LOD{index}_{name}', P2, UV2, C2, img, os.path.join(folder, f'{name}_LOD{index}.glb'))
                # What the exporter actually wrote (it drops degenerate triangles) is what counts: read the file back.
                P2, UV2, C2, _ = D.read_glb(path)
                ok, fwd, back, move = D.silhouette_ok(P, P2, big, index)
                if ok and not (asset_id in ONE_LOD):
                    app_ = D.appearance_error(P, UV, C, P2, UV2, C2, px)
                    g0_, g1_ = glow_area(P, C), glow_area(P2, C2)
                    ok = app_['rgb_mean'] <= 0.04 and app_['rgb_p95'] <= 0.2 and (g0_ == 0 or abs(g1_ - g0_) <= (0.05 * g0_ if big else max(0.20 * g0_, 0.04)))
                if ok and asset_id == 'world-vault':        # the footprint-edge region (footprint + 1 m) must stay within 3 cm of LOD0, as validate_lods_vault_r002.py requires
                    sel = np.nonzero(((np.abs(P2[:, :, 0]) < 14.5) & (np.abs(P2[:, :, 1]) < 10.0)).any(1))[0]
                    ok = D.deviation(P, P2[sel], samples=60000)[2] <= 0.03
                if ok or wanted >= (0.70 if index == 1 else 0.40):
                    break
                print(f'  {name} LOD{index} at {len(P2)} triangles fails the silhouette gate (distance {fwd:.3f}/{back:.3f} m, box {move:.3f} m); keeping more')
                wanted = min(0.70 if index == 1 else 0.40, wanted * 1.1)
            if not ok:
                print(f'  {name} LOD{index} at {len(P2)} triangles: distance {fwd:.3f}/{back:.3f} m, box move {move:.3f} m')
                raise RuntimeError(f'{name} LOD{index}: no acceptable decimation up to 70 % (LOD1) or 40 % (LOD2) of the triangles')
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
