"""Batch 3 build (#150): sorcerer, warlock and druid. build2.py (batch 2) is not edited so batch 2 asset records that hash it stay valid.

Build the cast characters (LOD0 with all ten clips, LOD1, LOD2) -> Art/<asset-id>/r001/*.glb + source .blend.

Run from the repository root (one Blender process at a time, under the machine lock):
    blender -b --python-exit-code 1 --python Art/rig-pilot/cast/scripts/build3.py [-- sorcerer warlock druid]

Uses the r001 skeleton, clips and exporter and the batch 2 Brace clip unchanged; only the recipes (cast_batch3.py) are new.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
sys.path.insert(0, HERE)

import bpy
import numpy as np

bpy.context.preferences.filepaths.save_version = 0

import cast_characters as C   # also puts the r001 scripts on sys.path
import cast_batch3 as B3      # registers the batch 3 classes (and, through cast_batch2, the Brace clip)
import clips as CL
import rigkit as K

args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
which = [a for a in args if a in B3.CHARACTERS] or list(B3.BATCH3)
METRICS = os.path.join(ROOT, 'Art', 'rig-pilot', 'cast', 'metrics')
os.makedirs(METRICS, exist_ok=True)


def build_one(key):
    builder, name, seed = B3.CHARACTERS[key]
    out_dir = os.path.join(ROOT, 'Art', B3.ASSET_IDS[key], 'r001')
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.fps = K.FPS
    ao = K.make_armature(sc)
    rig = K.Rig(ao)
    M0 = rig.fk(*CL.idle_at(0))
    img = None
    mat = None
    metrics = dict(name=name, asset_id=B3.ASSET_IDS[key], bones=len(K.BONE_ORDER), fps=K.FPS, lods={})
    objs = {}
    for lod in (0, 1, 2):
        K.LOD = lod
        B, spec = builder(rig, M0)
        if img is None:
            img = K.new_image(f'T_{name[3:]}_Atlas', B3.make_atlas(key, spec, seed))
            mat = K.make_material(img, f'M_{name[3:]}')
        ob = K.make_mesh_object(sc, ao, B, name if lod == 0 else f'{name}_LOD{lod}', mat)
        objs[lod] = ob
        V = np.array(B.V)
        metrics['lods'][f'LOD{lod}'] = dict(tris=len(ob.data.polygons), verts=len(V), height_m=round(float(V[:, 2].max() - V[:, 2].min()), 3),
                                            zmax=round(float(V[:, 2].max()), 3), zmin=round(float(V[:, 2].min()), 3), glow_verts=int(sum(B.G)))
        print(f'{name} LOD{lod}: {len(ob.data.polygons)} tris, {len(V)} verts, z {V[:, 2].min():.3f}..{V[:, 2].max():.3f}')
    K.LOD = 0
    metrics['materials'] = 1
    metrics['atlas'] = K.ATLAS
    clips = []
    for cname, (frames, loop, fn) in CL.CLIPS.items():
        K.author(ao, rig, cname, frames, fn, ground=True, loop=loop)
        clips.append(cname)
    metrics['clips'] = {c: dict(frames=[0, CL.CLIPS[c][0]], seconds=round(CL.CLIPS[c][0] / K.FPS, 3), loop=CL.CLIPS[c][1]) for c in clips}
    ao.animation_data_create()
    for lod in (1, 2):
        objs[lod].hide_render = True
    p0 = K.export_glb(os.path.join(out_dir, f'{name}.glb'), [objs[0]], ao, clips)
    metrics['glb_bytes'] = {os.path.basename(p0): os.path.getsize(p0)}
    for lod in (1, 2):
        p = K.export_glb(os.path.join(out_dir, f'{name}_LOD{lod}.glb'), [objs[lod]], ao, None)
        metrics['glb_bytes'][os.path.basename(p)] = os.path.getsize(p)
    ao.animation_data.action = None
    blend = os.path.join(out_dir, f'{name}_source.blend')
    bpy.ops.wm.save_as_mainfile(filepath=blend, compress=True)
    if os.path.exists(blend + '1'):
        os.remove(blend + '1')                                                 # Blender's backup of the previous save: never committed
    with open(os.path.join(METRICS, f'build_{key}.json'), 'w') as fh:
        json.dump(metrics, fh, indent=1)
    print('METRICS', key, json.dumps(metrics['lods']))


for k in which:
    build_one(k)
