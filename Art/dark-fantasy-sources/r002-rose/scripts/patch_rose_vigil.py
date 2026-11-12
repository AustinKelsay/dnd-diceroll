"""Vigil stained-glass rose at full resolution (issue #149, the Vigil window finding on #197).

  blender -b --python-exit-code 1 --python Art/dark-fantasy-sources/r002-rose/scripts/patch_rose_vigil.py       (shared host: under the machine lock)

The rose in SM_World_Vigil / SM_World_Vigil_Occluders sat in a 128 x 128 atlas region, about 40 texels/m against 256 elsewhere. The lower
60 % of the 2048 atlas is empty, so this adds a 1024 x 1024 rose region there (about 320 texels/m), rewrites only the TEXCOORD of the
vertices that sampled the old region, and embeds the extended atlas. Nothing else moves: positions, normals, COLOR_0, indices, every other
UV and every other atlas pixel are byte-identical (validate_rose_vigil.py proves it). The rose lives in the occluder mesh only (the world
mesh has no vertex in the region, asserted below), so the three occluder exports (LOD0 to LOD2) come out as world-vigil-occluders r002; the r001
files are kept and the world exports are unchanged.
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rose_lib as R

METRICS = os.path.join(R.SRC, 'r002', 'world_metrics.json')


def main():
    place = R.old_layout(METRICS)
    ox, oy, ow, oh = place['rose']
    assert (ow, oh) == (128, 128), place['rose']
    scale = R.ROSE_PX / ow
    first_js, first_bin = R.read_glb(os.path.join(R.ROOT, R.SOURCES[0][0]))
    old_atlas = R.png_decode(bytes(R.view_bytes(first_js, first_bin, first_js['images'][0]['bufferView'])))
    assert old_atlas.shape == (R.ATLAS, R.ATLAS, 4)
    # the old region really is the r001 rose (design check, 8-bit)
    import glass
    ref = R.to8(glass.rose_window(128)[..., :3])
    diff = np.abs(old_atlas[oy:oy + oh, ox:ox + ow, :3].astype(int) - ref.astype(int)).max()
    assert diff <= 1, f'old rose region does not match glass.rose_window ({diff})'
    # new atlas, 4096 x 2048: the old pixels untouched in the left half (same pixel positions), the new region and its wrapped padding in the right half
    atlas = np.empty((R.ATLAS_H, R.ATLAS_W, 4), np.uint8)
    atlas[:, :R.ATLAS] = old_atlas
    atlas[:, R.ATLAS:, :3] = old_atlas[-1, -1, :3]
    atlas[:, R.ATLAS:, 3] = 255
    nx, ny, n = R.NEW_X0, R.NEW_Y0, R.ROSE_PX
    block = R.to8(R.rose_hires())
    P_ = R.NEW_PAD
    atlas[ny - P_:ny + n + P_, nx - P_:nx + n + P_, :3] = np.pad(block, ((P_, P_), (P_, P_), (0, 0)), mode='wrap')
    assert ny + n + P_ <= R.ATLAS_H and nx + n + P_ <= R.ATLAS_W
    atlas_png = R.png_bytes(atlas)
    os.makedirs(R.OUT_SRC, exist_ok=True)
    open(os.path.join(R.OUT_SRC, 'T_World_Vigil_Atlas_rose.png'), 'wb').write(atlas_png)

    report = dict(old_region_px=[int(v) for v in (ox, oy, ow, oh)], new_region_px=[nx, ny, n, n], new_region_padding_px=R.NEW_PAD, atlas_px=[R.ATLAS_W, R.ATLAS_H], files={})
    for src in R.UNTOUCHED:        # the world meshes must have nothing in the old rose region
        wjs, wbin = R.read_glb(os.path.join(R.ROOT, src))
        for prim in wjs['meshes'][0]['primitives']:
            uv = R.accessor_array(wjs, wbin, prim['attributes']['TEXCOORD_0']).astype(np.float64) * R.ATLAS
            hit = (uv[:, 0] >= ox) & (uv[:, 0] <= ox + ow) & (uv[:, 1] >= oy) & (uv[:, 1] <= oy + oh)
            assert not hit.any(), f'{src} has {int(hit.sum())} vertices in the rose region'
    for src, dst in R.SOURCES:
        js, binary = R.read_glb(os.path.join(R.ROOT, src))
        assert len(js['meshes']) == 1 and len(js['images']) == 1
        total = moved = 0
        tris_rose = 0
        for prim in js['meshes'][0]['primitives']:
            ia = prim['attributes']['TEXCOORD_0']
            uv = R.accessor_array(js, binary, ia).astype(np.float64)
            px, py = uv[:, 0] * R.ATLAS, uv[:, 1] * R.ATLAS           # glTF UV is top-down: (u, v) = (x, y) / size (old 2048 atlas)
            eps = 0.01
            inside = (px >= ox - eps) & (px <= ox + ow + eps) & (py >= oy - eps) & (py <= oy + oh + eps)
            new = uv.copy()
            new[:, 0] = px / R.ATLAS_W                                # the old pixels sit in the left half: the same texels, u halved
            new[inside, 0] = (nx + (px[inside] - ox) * scale) / R.ATLAS_W
            new[inside, 1] = (ny + (py[inside] - oy) * scale) / R.ATLAS_H
            R.write_accessor(js, binary, ia, new.astype(np.float32))
            total += len(uv)
            moved += int(inside.sum())
            idx = R.accessor_array(js, binary, prim['indices']).reshape(-1, 3)
            tris_rose += int(inside[idx].all(1).sum())
        assert moved > 0, f'no rose vertices in {src}'
        # swap the embedded image, rebuilding the buffer so later views keep their data
        img_view = js['images'][0]['bufferView']
        views = [bytes(R.view_bytes(js, binary, i)) for i in range(len(js['bufferViews']))]
        views[img_view] = atlas_png
        out, offsets = bytearray(), []
        for v in views:
            out += b'\0' * (-len(out) % 4)
            offsets.append(len(out))
            out += v
        for i, bv in enumerate(js['bufferViews']):
            bv['byteOffset'] = offsets[i]
            bv['byteLength'] = len(views[i])
        js['buffers'][0]['byteLength'] = len(out)
        R.write_glb(os.path.join(R.ROOT, dst), js, out)
        report['files'][dst] = dict(source=src, vertices=total, rose_vertices_moved=moved, rose_triangles=tris_rose, bytes=os.path.getsize(os.path.join(R.ROOT, dst)))
        print(dst, report['files'][dst])
    json.dump(report, open(os.path.join(R.OUT_SRC, 'patch_report.json'), 'w'), indent=1)



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
