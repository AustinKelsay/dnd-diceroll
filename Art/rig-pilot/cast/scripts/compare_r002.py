"""Prove the r002 re-exports only add the Brace clip: compares an earlier LOD0 glb with its r002 re-export in a fresh process.

    blender -b --python Art/rig-pilot/cast/scripts/compare_r002.py -- <dir-with-the-old-glbs> <key> [<key> ...]

For each key (fighter wizard raider rogue cleric) the old glb is the r001 file copied into <dir> before the re-export. Checks: the
same vertex count and vertex positions in the bind pose (max difference), the same bones, every old clip present in the new file
with identical frame ranges and identical keyframe values on every fcurve (max absolute difference), and exactly one extra clip,
Brace. Writes Art/rig-pilot/cast/metrics/compare_r002.json.
"""
import json
import os
import sys

import bpy
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
IDS = {'fighter': ('sk-hero-fighter', 'SK_Hero_Fighter'), 'wizard': ('sk-hero-wizard', 'SK_Hero_Wizard'), 'raider': ('sk-foe-raider', 'SK_Foe_Raider'),
       'rogue': ('sk-hero-rogue', 'SK_Hero_Rogue'), 'cleric': ('sk-hero-cleric', 'SK_Hero_Cleric')}
args = sys.argv[sys.argv.index('--') + 1:]
old_dir, keys = args[0], args[1:]


def fcurves_of(a):
    if hasattr(a, 'fcurves'):
        return list(a.fcurves)
    out = []
    for layer in a.layers:                                   # Blender 5 slotted actions: layers > strips > channelbags > fcurves
        for strip in layer.strips:
            for cb in strip.channelbags:
                out += list(cb.fcurves)
    return out


def load(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.render.fps = 30                        # the importer converts seconds to frames at the scene rate
    bpy.ops.import_scene.gltf(filepath=path)
    ao = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    ob = max((o for o in bpy.data.objects if o.type == 'MESH'), key=lambda o: len(o.data.vertices))
    verts = np.array([v.co[:] for v in ob.data.vertices])
    acts = {}
    for a in bpy.data.actions:
        curves = {}
        for fc in fcurves_of(a):
            curves[(fc.data_path, fc.array_index)] = np.array([kp.co[:] for kp in fc.keyframe_points])
        acts[a.name] = (tuple(a.frame_range), curves)
    return [b.name for b in ao.data.bones], verts, acts


out = {}
for key in keys:
    aid, name = IDS[key]
    ob, ov, oa = load(os.path.join(old_dir, f'{name}.glb'))
    nb, nv, na = load(os.path.join(ROOT, 'Art', aid, 'r002', f'{name}.glb'))
    res = dict(same_bones=ob == nb, verts=[len(ov), len(nv)], max_vertex_diff_mm=round(float(np.abs(ov - nv).max()) * 1000, 6) if len(ov) == len(nv) else None,
               old_clips=sorted(oa), new_clips=sorted(na), extra_clips=sorted(set(na) - set(oa)), clips={})
    for c, (rng, curves) in oa.items():
        nrng, ncurves = na[c]
        worst = 0.0
        same_keys = set(curves) == set(ncurves)
        for k, arr in curves.items():
            if k in ncurves and arr.shape == ncurves[k].shape:
                worst = max(worst, float(np.abs(arr - ncurves[k]).max()))
            else:
                same_keys = False
        res['clips'][c] = dict(range_old=list(rng), range_new=list(nrng), same_curves=same_keys, max_abs_key_diff=round(worst, 9))
    res['curve_count'] = sum(len(v) for v in (oa[c][1] for c in oa))
    res['pass'] = bool(res['curve_count'] > 100 and res['same_bones'] and res['verts'][0] == res['verts'][1] and (res['max_vertex_diff_mm'] or 0) < 1e-3 and res['extra_clips'] == ['Brace']
                       and all(v['same_curves'] and v['range_old'] == v['range_new'] and v['max_abs_key_diff'] < 1e-6 for v in res['clips'].values()))
    out[key] = res
    print('COMPARE', key, res['pass'], res['max_vertex_diff_mm'], res['extra_clips'])
with open(os.path.join(ROOT, 'Art', 'rig-pilot', 'cast', 'metrics', 'compare_r002.json'), 'w') as fh:
    json.dump(out, fh, indent=1)
failed = [k for k, v in out.items() if not v['pass']]
if failed:
    print('COMPARE FAILED', ', '.join(failed))
    sys.exit(1)
