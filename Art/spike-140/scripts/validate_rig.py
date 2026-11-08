"""#140 spike: re-import SK_Hero_FighterSpike.glb into an empty scene (fresh process) and check it.

  blender -b --python Art/spike-140/scripts/validate_rig.py

Reports bone count/names, actions with frame ranges, triangles, materials, atlas size, how far the skinned mesh moves at
mid-walk and mid-idle against the bind pose, and renders bind / mid-walk / mid-idle stills to Art/spike-140/previews/.
"""
import json
import math
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
OUT = os.path.join(ROOT, 'Art', 'spike-140', 'exports')
PREV = os.path.join(ROOT, 'Art', 'spike-140', 'previews')
os.environ.setdefault('THEME_LOCAL', tempfile.mkdtemp(prefix='spike140_'))
sys.path.insert(0, os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r001', 'scripts'))

import bpy
import numpy as np
from mathutils import Vector

import render_lib as R

EXPECT_BONES = 25
os.makedirs(PREV, exist_ok=True)
sc = R.setup_scene((960, 1080))
sc.render.fps = 30                       # the importer converts seconds to frames at the scene rate
bpy.ops.import_scene.gltf(filepath=os.path.join(OUT, 'SK_Hero_FighterSpike.glb'))
arms = [o for o in bpy.data.objects if o.type == 'ARMATURE']
meshes = [o for o in bpy.data.objects if o.type == 'MESH']
ao = arms[0]
ob = max((m for m in meshes if m.parent == ao or m.find_armature()), key=lambda m: len(m.data.polygons))
bones = [b.name for b in ao.data.bones]
print('VALIDATE bones', len(bones), bones)
acts = {a.name: tuple(round(x, 2) for x in a.frame_range) for a in bpy.data.actions}
print('VALIDATE actions', acts)
tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
print('VALIDATE tris', tris, 'materials', [m.name for m in ob.data.materials], 'image', [tuple(i.size) for i in bpy.data.images if i.size[0] > 0])
print('VALIDATE vertex groups', len(ob.vertex_groups))


def eval_pts():
    dg = bpy.context.evaluated_depsgraph_get()
    e = ob.evaluated_get(dg)
    m = e.to_mesh()
    P = np.array([ob.matrix_world @ v.co for v in m.vertices])
    e.to_mesh_clear()
    return P


def pose(act_name, frame):
    a = bpy.data.actions[act_name]
    ao.animation_data_create()
    ao.animation_data.action = a
    for tr in list(ao.animation_data.nla_tracks):
        tr.mute = True
    sc.frame_set(frame)
    bpy.context.view_layer.update()


for tr in ao.animation_data.nla_tracks if ao.animation_data else []:
    tr.mute = True
ao.animation_data.action = None
sc.frame_set(0)
bind = eval_pts()
res = {}
for name, f in (('Idle', 30), ('Walk', 8), ('Walk', 23)):
    pose(name, f)
    P = eval_pts()
    d = np.linalg.norm(P - bind, axis=1)
    res[f'{name}@{f}'] = dict(max_cm=round(float(d.max()) * 100, 1), mean_cm=round(float(d.mean()) * 100, 1),
                              moving_verts=int((d > .005).sum()))
    print('VALIDATE deform', name, f, res[f'{name}@{f}'])
# loop closure: first and last pose identical
for name, last in (('Idle', 60), ('Walk', 30)):
    pose(name, 0)
    A = eval_pts()
    pose(name, last)
    Bp = eval_pts()
    print('VALIDATE loop', name, 'max first/last diff (mm)', round(float(np.abs(A - Bp).max()) * 1000, 3))

# renders (unlit texture, three-quarter ortho)
R.unlit_texture(ob, 1.0)
R.lights()
cam_obs = [ob]
for tag, (name, f) in (('bind', (None, 0)), ('walk_mid', ('Walk', 8)), ('idle_mid', ('Idle', 30))):
    if name:
        pose(name, f)
    else:
        ao.animation_data.action = None
        sc.frame_set(0)
    for o in [o for o in bpy.data.objects if o.type == 'CAMERA']:
        bpy.data.objects.remove(o)
    cam = R.camera_three_quarter(cam_obs, direction=(1.0, -.9, .25), margin=1.1)
    cam.data.ortho_scale = 2.3
    cam.location = (cam.location - Vector((0, 0, 0)))
    R.render_to_array(os.path.join(PREV, f'rig_{tag}.png'))
print('VALIDATE done')
