"""Re-open every Root r002 LOD glb in a fresh Blender process and check it against its LOD0 and lod_metrics.json:
triangle count and 50 % / 25 % targets, UVs inside 0..1, COLOR_0 RGBA with the same alpha range and glow area as LOD0, sampler Linear,
atlas pixels identical to LOD0, bounding box and silhouette (surface distance both ways), appearance error, and for the world and the
occluders the board-area clearance (nothing above 10 cm inside the 27 x 18 m footprint) and footprint-edge height.

  blender -b --python Art/dark-fantasy-sources/r002-root/scripts/validate_lods_root_r002.py
Prints a line per LOD and 'LOD VALIDATE OK' or 'LOD VALIDATE FAIL'.
"""
import hashlib
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lod_r002 as D
from r002_lib import bpy, np, ROOT, glb_stats

fails = []


def check(cond, msg):
    if not cond:
        fails.append(msg)
        print('FAIL', msg)


def imported(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)
    ob = next(o for o in bpy.data.objects if o.type == 'MESH')
    me = ob.data
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers[0].data.foreach_get('uv', uv)
    mat = me.materials[0].node_tree
    img = next(n.image for n in mat.nodes if n.type == 'TEX_IMAGE')
    a = np.empty(img.size[0] * img.size[1] * 4, np.float32)
    img.pixels.foreach_get(a)
    return dict(tris=len(me.polygons), uv=uv, colors=len(me.color_attributes), interp=[n.interpolation for n in mat.nodes if n.type == 'TEX_IMAGE'],
                px=hashlib.sha256((a * 255).round().astype(np.uint8).tobytes()).hexdigest()[:16])


def glow(P, C):
    a = np.linalg.norm(np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]), axis=1) * 0.5
    return float((a * C[..., 3].mean(1)).sum())


def main():
    metrics = json.load(open(os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r002-root', 'lod_metrics.json')))
    for name, m in metrics.items():
        folder = os.path.join(ROOT, 'Art', m['asset_id'], 'r%03d' % m['revision'])
        lod0_path = os.path.join(folder, name + '.glb')
        P0, UV0, C0, png0 = D.read_glb(lod0_path)
        base = imported(lod0_path)
        stats0 = glb_stats(lod0_path)
        px = D.atlas_pixels(bpy.data.images.load(png0))
        big = ('organic' if 'occluders' in m['asset_id'] else True) if m['asset_id'].startswith('world-root') else False
        for row in m['lods']:
            path = os.path.join(ROOT, row['file'])
            tag = f'{name} LOD{row["lod"]}'
            check(hashlib.sha256(open(path, 'rb').read()).hexdigest() == row['sha256'], f'{tag}: file differs from lod_metrics.json')
            P, UV, C, _ = D.read_glb(path)
            im = imported(path)
            target = {1: 0.5, 2: 0.25}[row['lod']]
            check(len(P) == row['triangles'] == im['tris'], f'{tag}: triangle count {len(P)} / {im["tris"]} vs {row["triangles"]}')
            frac = len(P) / len(P0)
            check(abs(frac - 0.5) <= 0.015 if row['lod'] == 1 else 0.24 <= frac <= 0.40, f'{tag}: {len(P)} of {len(P0)} triangles ({frac:.0%}) is not about {target:.0%} (LOD2 may keep up to 40 % to hold the silhouette)')
            check(len(P) < len(P0) * (0.9 if row['lod'] == 1 else 0.5), f'{tag}: not reduced enough')
            check(UV.min() >= -1e-4 and UV.max() <= 1 + 1e-4, f'{tag}: UV outside 0..1')
            check(im['colors'] >= 1 and C.shape[-1] == 4, f'{tag}: COLOR_0 is not RGBA')
            check(abs(C[..., 3].max() - C0[..., 3].max()) < 1e-3 and abs(C[..., 3].min() - C0[..., 3].min()) < 1e-3, f'{tag}: alpha range changed')
            g0, g1 = glow(P0, C0), glow(P, C)
            check(g0 == 0 or abs(g1 - g0) <= (0.05 * g0 if big else max(0.20 * g0, 0.04)), f'{tag}: glow area {g1:.4f} vs {g0:.4f} m2')        # kit fungus caps are tiny accents: 20 % or 0.04 m2
            check(im['interp'] == ['Linear'], f'{tag}: sampling {im["interp"]}')
            check(im['px'] == base['px'], f'{tag}: atlas pixels differ from LOD0')
            st = glb_stats(path)
            tol = D.silhouette_limits(big, row['lod'])[1]
            check(max(abs(a - b) for a, b in zip(st['bbox_min_m'] + st['bbox_max_m'], stats0['bbox_min_m'] + stats0['bbox_max_m'])) <= tol, f'{tag}: bounding box moved {st["bbox_min_m"]} {st["bbox_max_m"]} vs {stats0["bbox_min_m"]} {stats0["bbox_max_m"]}')
            check(st['color0'] == stats0['color0'] and st['materials'] == 1 and st['image_px'] == stats0['image_px'], f'{tag}: attribute layout differs from LOD0')
            fwd = D.deviation(P0, P, samples=20000)            # LOD surface -> source
            back = D.deviation(P, P0, samples=20000)           # source surface -> LOD (catches lost silhouette)
            limit = D.silhouette_limits(big, row['lod'])[0]
            check(fwd[2] <= limit and back[2] <= limit, f'{tag}: surface distance max {fwd[2]:.3f} / {back[2]:.3f} m over {limit} m')
            app = D.appearance_error(P0, UV0, C0, P, UV, C, px)
            check(app['rgb_mean'] <= 0.04 and app['rgb_p95'] <= 0.2, f'{tag}: appearance error {app}')
            if big:
                # board-area clearance: nothing above 10 cm inside the footprint; footprint edge and floor height stay put
                pts = P.reshape(-1, 3)
                inside = (np.abs(pts[:, 0]) < 13.5) & (np.abs(pts[:, 1]) < 9.0)
                check(not (pts[inside, 2] > 0.10).any(), f'{tag}: geometry above 10 cm inside the board footprint (max {pts[inside, 2].max() if inside.any() else 0:.3f})')
                if m['asset_id'] == 'world-root':
                    sel = np.nonzero(((np.abs(P[:, :, 0]) < 14.5) & (np.abs(P[:, :, 1]) < 10.0)).any(1))[0]
                    edge = D.deviation(P0, P[sel], samples=60000)
                    check(edge[2] <= 0.03, f'{tag}: footprint-edge region deviates {edge[2]:.3f} m')
                    print(f'  footprint region (footprint + 1 m): {len(sel)} triangles, max deviation {edge[2]:.4f} m')
            print(f'{tag:34s} {len(P0):7d} -> {len(P):7d} tris, dev {fwd[2]:.3f}/{back[2]:.3f} m, appearance {app["rgb_mean"]:.3f}/{app["rgb_p95"]:.3f}, glow {g1:.3f}/{g0:.3f} m2')
    print('LOD VALIDATE', 'FAIL' if fails else 'OK', len(fails))


main()
