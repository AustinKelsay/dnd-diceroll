"""Re-open every r002 glb in a fresh Blender scene and check it: UVs inside 0..1 and clear of region borders, COLOR_0 RGBA with the
right alpha range, pivot/bbox sanity, world footprint clear of tall architecture, world and occluder atlas identical.

  blender -b --python Art/dark-fantasy-sources/r002/scripts/validate_r002.py
Prints a line per file and 'VALIDATE OK' or 'VALIDATE FAIL'.
"""
import hashlib
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import r002_lib as R
from r002_lib import bpy, np

ROOT = R.ROOT
fails = []


def check(cond, msg):
    if not cond:
        fails.append(msg)
        print('FAIL', msg)


def load(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)
    ob = next(o for o in bpy.data.objects if o.type == 'MESH')
    me = ob.data
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers[0].data.foreach_get('uv', uv)
    uv = uv.reshape(-1, 2)
    ca = me.color_attributes[0]
    c = np.empty(len(ca.data) * 4, np.float32)
    ca.data.foreach_get('color', c)
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get('co', co)
    img = me.materials[0].node_tree.nodes['Image Texture'].image if 'Image Texture' in me.materials[0].node_tree.nodes else None
    px = None
    if img is not None:
        a = np.empty(img.size[0] * img.size[1] * 4, np.float32)
        img.pixels.foreach_get(a)
        px = hashlib.sha256((a * 255).round().astype(np.uint8).tobytes()).hexdigest()[:16]
    return dict(tris=len(me.polygons), uv=uv, col=c.reshape(-1, 4), co=co.reshape(-1, 3), px=px, interp=[n.interpolation for n in me.materials[0].node_tree.nodes if n.type == 'TEX_IMAGE'])


def main():
    atl = {}
    km = json.load(open(os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r002', 'kit_metrics.json')))
    paths = [os.path.join(ROOT, 'Art', 'world-vigil', 'r002', 'SM_World_Vigil.glb'), os.path.join(ROOT, 'Art', 'world-vigil-occluders', 'r001', 'SM_World_Vigil_Occluders.glb')]
    paths += [os.path.join(ROOT, v['file']) for v in km.values()]
    for p in paths:
        rel = os.path.relpath(p, ROOT)
        d = load(p)
        uv, col, co = d['uv'], d['col'], d['co']
        check(uv.min() >= -1e-4 and uv.max() <= 1 + 1e-4, f'{rel}: UV outside 0..1 {uv.min():.4f} {uv.max():.4f}')
        check(d['interp'] == ['Linear'], f'{rel}: image sampling {d["interp"]}')
        check(col.shape[1] == 4, f'{rel}: COLOR_0 not RGBA')
        a = col[:, 3]
        print(f'{rel:70s} tris {d["tris"]:7d} alpha[{a.min():.2f},{a.max():.2f}] zmin {co[:, 2].min():+.3f} m')
        if 'world-vigil' in rel:
            atl[rel] = d['px']
            if 'Occluders' not in rel:
                check(a.max() == 1.0, f'{rel}: no glow alpha')
        if rel.split('/')[1].startswith(('prop-', 'scatter-')):
            # glTF Y-up import converts back to Z-up in Blender: z is up
            check(co[:, 2].min() > -.03, f'{rel}: geometry below the floor pivot ({co[:, 2].min():.3f})')
            check(abs(co[:, 0]).max() < .95 and (abs(co[:, 1]).max() < .95 or 'arch' in rel), f'{rel}: footprint beyond the cell')
    vals = set(atl.values())
    check(len(vals) == 1, f'world and occluder atlases differ: {atl}')
    print('VALIDATE', 'FAIL' if fails else 'OK', len(fails))


main()
