"""Build the three rigged characters (LOD0 with all clips, LOD1, LOD2) -> Art/<asset-id>/r001/*.glb + source .blend.

Run from the repository root (one Blender process at a time):
    blender -b --python Art/rig-pilot/r001/scripts/build.py [-- fighter wizard raider] [--noclips]
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

import characters as C
import clips as CL
import rigkit as K

ASSET_IDS = {'fighter': 'sk-hero-fighter', 'wizard': 'sk-hero-wizard', 'raider': 'sk-foe-raider'}
args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
NOCLIPS = '--noclips' in args
which = [a for a in args if a in C.CHARACTERS] or list(C.CHARACTERS)
METRICS = os.path.join(ROOT, 'Art', 'rig-pilot', 'r001', 'metrics')
os.makedirs(METRICS, exist_ok=True)


def build_one(key):
    builder, name, seed = C.CHARACTERS[key]
    out_dir = os.path.join(ROOT, 'Art', ASSET_IDS[key], 'r001')
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.fps = K.FPS
    ao = K.make_armature(sc)
    rig = K.Rig(ao)
    M0 = rig.fk(*CL.idle_at(0))
    spec = None
    img = None
    mat = None
    metrics = dict(name=name, asset_id=ASSET_IDS[key], bones=len(K.BONE_ORDER), fps=K.FPS, lods={})
    objs = {}
    for lod in (0, 1, 2):
        K.LOD = lod
        B, spec = builder(rig, M0)
        if img is None:
            img = K.new_image(f'T_{name[3:]}_Atlas', K.make_atlas(spec, seed))
            mat = K.make_material(img, f'M_{name[3:]}')
        ob = K.make_mesh_object(sc, ao, B, name if lod == 0 else f'{name}_LOD{lod}', mat)
        objs[lod] = ob
        V = np.array(B.V)
        metrics['lods'][f'LOD{lod}'] = dict(tris=len(ob.data.polygons), verts=len(V),
                                            height_m=round(float(V[:, 2].max() - V[:, 2].min()), 3), zmax=round(float(V[:, 2].max()), 3),
                                            zmin=round(float(V[:, 2].min()), 3), glow_verts=int(sum(B.G)))
        print(f'{name} LOD{lod}: {len(ob.data.polygons)} tris, {len(V)} verts, z {V[:, 2].min():.3f}..{V[:, 2].max():.3f}')
    K.LOD = 0
    metrics['materials'] = 1
    metrics['atlas'] = K.ATLAS
    clips = []
    if not NOCLIPS:
        for cname, (frames, loop, fn) in CL.CLIPS.items():
            K.author(ao, rig, cname, frames, fn, ground=True, loop=loop)
            clips.append(cname)
        metrics['clips'] = {c: dict(frames=[0, CL.CLIPS[c][0]], seconds=round(CL.CLIPS[c][0] / K.FPS, 3), loop=CL.CLIPS[c][1]) for c in clips}
    # hide LODs from each other for export: export selected only
    ao.animation_data_create()
    for lod in (1, 2):
        objs[lod].hide_render = True
    p0 = K.export_glb(os.path.join(out_dir, f'{name}.glb'), [objs[0]], ao, clips)
    metrics['glb_bytes'] = {os.path.basename(p0): os.path.getsize(p0)}
    for lod in (1, 2):
        p = K.export_glb(os.path.join(out_dir, f'{name}_LOD{lod}.glb'), [objs[lod]], ao, None)
        metrics['glb_bytes'][os.path.basename(p)] = os.path.getsize(p)
    # restore actions for the editable source (the exporter clears NLA tracks between files)
    ao.animation_data.action = None
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, f'{name}_source.blend'), compress=True)
    with open(os.path.join(METRICS, f'build_{key}.json'), 'w') as fh:
        json.dump(metrics, fh, indent=1)
    print('METRICS', key, json.dumps(metrics['lods']))


for k in which:
    build_one(k)
