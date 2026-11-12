"""Foe batch build (#150): scout, archer, skirmisher, brute and warden. The earlier build scripts are not edited, so earlier asset records that hash them stay valid.

Build the foes (LOD0 with all ten clips plus the class's own override clip, LOD1, LOD2) -> Art/<asset-id>/r001/*.glb + source .blend.

Run from the repository root (one Blender process at a time, under the machine lock; always with --python-exit-code 1):
    blender -b --python-exit-code 1 --python Art/rig-pilot/cast/scripts/build_foes.py [-- scout archer skirmisher brute warden]

Uses the r001 skeleton, clips and exporter and the batch 2 Brace clip unchanged; only the recipes (cast_foes.py) and the two foe overrides
(overrides_foes.py: the archer's Ranged_Archer and the brute's Melee_Brute) are new.
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
import cast_foes as F         # registers the foe classes (and, through cast_batch2, the Brace clip)
import clips as CL
import overrides as OV
import overrides_foes as OF
import rigkit as K

args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
which = [a for a in args if a in F.CHARACTERS] or list(F.FOES)
METRICS = os.path.join(ROOT, 'Art', 'rig-pilot', 'cast', 'metrics')
os.makedirs(METRICS, exist_ok=True)


def build_one(key):
    builder, name, seed = F.CHARACTERS[key]
    out_dir = os.path.join(ROOT, 'Art', F.ASSET_IDS[key], 'r001')
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.fps = K.FPS
    ao = K.make_armature(sc)
    rig = K.Rig(ao)
    M0 = rig.fk(*CL.idle_at(0))
    img = None
    mat = None
    metrics = dict(name=name, asset_id=F.ASSET_IDS[key], bones=len(K.BONE_ORDER), fps=K.FPS, lods={})
    objs = {}
    for lod in (0, 1, 2):
        K.LOD = lod
        B, spec = builder(rig, M0)
        if img is None:
            img = K.new_image(f'T_{name[3:]}_Atlas', F.make_atlas(key, spec, seed))
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
    if key in OF.OVERRIDES:                                  # a per-class clip override (overrides_foes.py), exported beside the shared clips
        shared, action, frames = OF.OVERRIDES[key]
        if key == 'archer':
            pose_at, scale_at, state = OF.archer_pose_fn(rig)
        else:
            g0 = OV.grip(rig, M0, 'R')
            pose_at, state = OF.brute_pose_fn(rig, g0 - OF.AX_IDLE * OF.HAFT_FROM_GRIP)
            scale_at = None
        act = K.author(ao, rig, action, frames, pose_at, ground=True, loop=False)
        if scale_at:                                         # the hunting knife vanishes from the hand while the bow is out
            ao.animation_data.action = act
            bpy.context.view_layer.objects.active = ao
            bpy.ops.object.mode_set(mode='POSE')
            pb = ao.pose.bones['weapon_socket']
            for f in range(frames + 1):
                s_ = scale_at(f)
                pb.scale = (s_, s_, s_)
                pb.keyframe_insert('scale', frame=f)
            bpy.ops.object.mode_set(mode='OBJECT')
        clips.append(action)
        metrics['override'] = dict(clip=shared, action=action, frames=[0, frames], worst_residual_cm=round(max(state.get('worst', 0), state.get('worstR', 0)) * 100, 2))
        print('OVERRIDE', key, action, metrics['override'])
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
