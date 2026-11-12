"""Batch 2 build (#150): barbarian, monk, paladin, ranger, and the r002 re-export of the five earlier characters.
build.py (batch 1) is not edited so batch 1 asset records that hash it stay valid; this is its batch 2 counterpart.

Build the cast characters (LOD0 with all ten clips, LOD1, LOD2) -> Art/<asset-id>/r001/*.glb + source .blend.

Run from the repository root (one Blender process at a time):
    blender -b --python-exit-code 1 --python Art/rig-pilot/cast/scripts/build2.py [-- rogue cleric barbarian monk paladin ranger]
    blender -b --python-exit-code 1 --python Art/rig-pilot/cast/scripts/build2.py -- --r002 fighter wizard raider rogue cleric

Every command takes `--python-exit-code 1` (Blender then exits 1 when the script raises; otherwise an uncaught exception still exits 0 and a pipeline would read it as success).

Uses the r001 skeleton, clips and exporter unchanged (Art/rig-pilot/r001/scripts); only the recipes are new. The tenth clip, Brace
(brace.py, batch 2), is registered when cast_batch2 is imported. `--r002` re-exports the five earlier characters with the ten clips
into Art/<asset-id>/r002 (asset records are immutable per revision); only the LOD0 glb and the source blend differ from r001, and
the LOD glbs are identical (they carry no clips), so r002 records reference r001's for those.
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
import cast_batch2 as B2      # registers the batch 2 classes and the Brace clip
import clips as CL
import overrides as OV
import rigkit as K

args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
REV = 'r002' if '--r002' in args else 'r001'
LEGACY = dict(C.R1.CHARACTERS)                                                  # fighter, wizard, raider: the r001 recipes, unchanged
LEGACY_IDS = {'fighter': 'sk-hero-fighter', 'wizard': 'sk-hero-wizard', 'raider': 'sk-foe-raider'}
ALL = {**C.CHARACTERS, **LEGACY}
IDS = {**C.ASSET_IDS, **LEGACY_IDS}
which = [a for a in args if a in ALL] or list(B2.BATCH2)
METRICS = os.path.join(ROOT, 'Art', 'rig-pilot', 'cast', 'metrics')
os.makedirs(METRICS, exist_ok=True)


def build_one(key):
    builder, name, seed = ALL[key]
    out_dir = os.path.join(ROOT, 'Art', IDS[key], REV)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.fps = K.FPS
    ao = K.make_armature(sc)
    rig = K.Rig(ao)
    M0 = rig.fk(*CL.idle_at(0))
    img = None
    mat = None
    metrics = dict(name=name, asset_id=IDS[key], bones=len(K.BONE_ORDER), fps=K.FPS, lods={})
    objs = {}
    for lod in (0, 1, 2):
        K.LOD = lod
        B, spec = builder(rig, M0)
        if img is None:
            img = K.new_image(f'T_{name[3:]}_Atlas', B2.make_atlas(key, spec, seed))
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
    if key in OV.OVERRIDES and REV == 'r001':          # a per-class clip override (overrides.py), exported beside the shared clips
        shared, action, frames = OV.OVERRIDES[key]
        if key == 'ranger':
            pose_at, scale_at, state = OV.ranger_pose_fn(rig)
        else:
            g0 = OV.grip(rig, M0, 'R')
            pose_at, state = OV.barbarian_pose_fn(rig, g0 - OV.AX_IDLE * OV.HAFT_FROM_GRIP)
            scale_at = None
        act = K.author(ao, rig, action, frames, pose_at, ground=True, loop=False)
        if scale_at:                                    # the short blade vanishes from the hand while the bow is out
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
        if REV != 'r001':
            continue                                                            # the LOD glbs carry no clips: r002 reuses r001's
        p = K.export_glb(os.path.join(out_dir, f'{name}_LOD{lod}.glb'), [objs[lod]], ao, None)
        metrics['glb_bytes'][os.path.basename(p)] = os.path.getsize(p)
    ao.animation_data.action = None
    blend = os.path.join(out_dir, f'{name}_source.blend')
    bpy.ops.wm.save_as_mainfile(filepath=blend, compress=True)
    if os.path.exists(blend + '1'):
        os.remove(blend + '1')                                                 # Blender's backup of the previous save: never committed
    with open(os.path.join(METRICS, f'build_{key}.json' if REV == 'r001' else f'build_{key}_r002.json'), 'w') as fh:
        json.dump(metrics, fh, indent=1)
    print('METRICS', key, json.dumps(metrics['lods']))


for k in which:
    build_one(k)
