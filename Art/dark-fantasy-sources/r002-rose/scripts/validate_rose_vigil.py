"""Proves the Vigil rose patch changed the rose and nothing else (issue #149). Exits non-zero on any failure.

  blender -b --python-exit-code 1 --python Art/dark-fantasy-sources/r002-rose/scripts/validate_rose_vigil.py

For each patched occluder glb against its source: positions, normals, COLOR_0 and indices are byte-identical; every non-rose TEXCOORD keeps its texel (its U is halved because the atlas doubled in width);
rose TEXCOORDs map back exactly; the atlas differs only inside the new region (the old rose region stays); texel density of the rose
triangles before and after; and an appearance check (the same sample points read from the old and the new atlas).
Writes r002-rose/rose_metrics.json.
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rose_lib as R

FAILS = []


def check(cond, msg):
    if not cond:
        FAILS.append(msg)
        print('FAIL', msg)


def bilinear(atlas, px, py):
    h, w = atlas.shape[:2]
    x, y = np.clip(px - .5, 0, w - 1.001), np.clip(py - .5, 0, h - 1.001)
    x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = (x - x0)[:, None], (y - y0)[:, None]
    a = atlas[..., :3].astype(np.float64) / 255
    return (a[y0, x0] * (1 - fx) * (1 - fy) + a[y0, x0 + 1] * fx * (1 - fy) + a[y0 + 1, x0] * (1 - fx) * fy + a[y0 + 1, x0 + 1] * fx * fy)


def prim_data(js, binary, prim):
    at = prim['attributes']
    return {k: R.accessor_array(js, binary, at[k]) for k in at}, R.accessor_array(js, binary, prim['indices']).reshape(-1, 3)


def main():
    place = R.old_layout(os.path.join(R.SRC, 'r002', 'world_metrics.json'))
    ox, oy, ow, oh = place['rose']
    nx, ny, n = R.NEW_X0, R.NEW_Y0, R.ROSE_PX
    out = {'old_region_px': [int(ox), int(oy), ow, oh], 'new_region_px': [nx, ny, n, n], 'files': {}}
    rng = np.random.default_rng(3)
    for src, dst in R.SOURCES:
        sj, sb = R.read_glb(os.path.join(R.ROOT, src))
        dj, db = R.read_glb(os.path.join(R.ROOT, dst))
        # structure: same JSON apart from the buffer layout
        for key in ('nodes', 'meshes', 'materials', 'accessors', 'samplers', 'textures', 'scenes'):
            check(sj.get(key) == dj.get(key), f'{dst}: {key} differs')
        check(len(sj['bufferViews']) == len(dj['bufferViews']), f'{dst}: buffer view count')
        sa, da = R.atlas_of(os.path.join(R.ROOT, src)), R.atlas_of(os.path.join(R.ROOT, dst))
        check(sa.shape == (R.ATLAS, R.ATLAS, 4) and da.shape == (R.ATLAS_H, R.ATLAS_W, 4), f'{dst}: atlas size {da.shape}')
        check(bool((da[:, :R.ATLAS] == sa).all()), f'{dst}: the old atlas pixels changed')
        right = da[:, R.ATLAS:].copy()
        right[ny - R.NEW_PAD - 0:ny + n + R.NEW_PAD, nx - R.ATLAS - R.NEW_PAD:nx - R.ATLAS + n + R.NEW_PAD] = sa[-1, -1]
        check(bool((right[..., :3] == sa[-1, -1, :3]).all()), f'{dst}: new atlas pixels outside the new region')
        old_area = new_area = world_area = 0.0
        diffs = []
        moved = 0
        for ps, pd in zip(sj['meshes'][0]['primitives'], dj['meshes'][0]['primitives']):
            ad, idx_s = prim_data(sj, sb, ps)
            bd, idx_d = prim_data(dj, db, pd)
            for k in ad:
                if k != 'TEXCOORD_0':
                    check(np.array_equal(ad[k], bd[k]) if k != 'TEXCOORD_0' else True, f'{dst}: {k} changed')
            check(np.array_equal(idx_s, idx_d), f'{dst}: indices changed')
            us = ad['TEXCOORD_0'].astype(np.float64) * R.ATLAS
            ud = bd['TEXCOORD_0'].astype(np.float64) * np.array((R.ATLAS_W, R.ATLAS_H))
            rose = (us[:, 0] >= ox - .01) & (us[:, 0] <= ox + ow + .01) & (us[:, 1] >= oy - .01) & (us[:, 1] <= oy + oh + .01)
            check(bool(np.abs(ud[~rose] - us[~rose] * np.array((1.0, 1.0))).max() < 1e-2), f'{dst}: a non-rose UV moved off its texel')
            back = np.stack([ox + (ud[rose, 0] - nx) / (n / ow), oy + (ud[rose, 1] - ny) / (n / oh)], 1)
            check(bool(np.abs(back - us[rose]).max() < 0.02), f'{dst}: rose UVs do not map back')
            check(bool(((ud[rose, 0] >= nx - .01) & (ud[rose, 0] <= nx + n + .01) & (ud[rose, 1] >= ny - .01) & (ud[rose, 1] <= ny + n + .01)).all()), f'{dst}: a rose UV left the new region')
            moved += int(rose.sum())
            tri = rose[idx_s].all(1)
            P = ad['POSITION'].astype(np.float64)[idx_s[tri]]
            world = 0.5 * np.linalg.norm(np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]), axis=1)
            def uv_area(u):
                t = u[idx_s[tri]]
                return 0.5 * np.abs((t[:, 1, 0] - t[:, 0, 0]) * (t[:, 2, 1] - t[:, 0, 1]) - (t[:, 2, 0] - t[:, 0, 0]) * (t[:, 1, 1] - t[:, 0, 1]))
            old_area += float(uv_area(us).sum()); new_area += float(uv_area(ud).sum()); world_area += float(world.sum())
            # appearance: the same points on the same triangles read from both atlases
            ti = np.flatnonzero(tri)
            if len(ti):
                pick = rng.choice(ti, size=min(3000, len(ti) * 8))
                bary = rng.dirichlet((1, 1, 1), size=len(pick))
                a = (us[idx_s[pick]] * bary[..., None]).sum(1)
                b = (ud[idx_s[pick]] * bary[..., None]).sum(1)
                diffs.append(np.abs(bilinear(sa, a[:, 0], a[:, 1]) - bilinear(da, b[:, 0], b[:, 1])).mean(1))
        check(moved > 0, f'{dst}: no rose vertices')
        d = np.concatenate(diffs)
        dens_old, dens_new = (old_area / world_area) ** .5, (new_area / world_area) ** .5
        check(dens_new >= 215, f'{dst}: new rose density {dens_new:.0f} texels/m < 215 (85 % of the 256 of the rest of the Vigil)')
        check(abs(dens_new / dens_old - n / ow) < 0.01, f'{dst}: density ratio {dens_new / dens_old:.3f}')
        check(float(d.mean()) < 0.12, f'{dst}: appearance mean diff {d.mean():.3f}')
        check([s.get('magFilter') for s in dj['samplers']] == [s.get('magFilter') for s in sj['samplers']], f'{dst}: sampler changed')
        out['files'][dst] = dict(rose_vertices=moved, rose_world_area_m2=round(world_area, 3), rose_diameter_m=round(2 * (world_area / 2 / np.pi) ** .5 * 1.0, 2),
                                 texels_per_m_before=round(dens_old, 1), texels_per_m_after=round(dens_new, 1),
                                 appearance_mean_abs_rgb_diff=round(float(d.mean()), 4), appearance_p95=round(float(np.percentile(d, 95)), 4))
        print(dst, out['files'][dst])
    out['result'] = 'FAIL' if FAILS else 'PASS'
    out['failures'] = FAILS
    json.dump(out, open(os.path.join(R.OUT_SRC, 'rose_metrics.json'), 'w'), indent=1)
    if FAILS:
        print(len(FAILS), 'failures')
        sys.exit(1)
    print('rose patch validated')



def run_guarded(entry):
    """Blender exits 0 when a script raises; every way out of these scripts but success must be non-zero, so an unexpected exception is printed and exits 1.
    (Run Blender with --python-exit-code 1 as well: it covers an error before this point, such as a failed import.)"""
    try:
        entry()
    except SystemExit:
        raise
    except BaseException:
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    run_guarded(main)
