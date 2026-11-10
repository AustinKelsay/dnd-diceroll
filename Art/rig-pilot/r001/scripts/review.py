"""Fresh-process review of the exported glbs: validation numbers and contact sheets.

    blender -b --python Art/rig-pilot/r001/scripts/review.py -- validate fighter|wizard|raider
    blender -b --python Art/rig-pilot/r001/scripts/review.py -- sheet bind|clips|stone

validate re-imports <name>.glb and the two LOD glbs into an empty scene, then writes metrics/validate_<key>.json
(bones, clips and frame ranges at 30 fps, tris per LOD, deformation at mid-clip frames, loop seam distance, ...).
sheet renders tiles into a temp dir and ImageMagick assembles them under previews/.
"""
import json
import math
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
R = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
PREV = os.path.join(R, 'Art', 'rig-pilot', 'r001', 'previews')
MET = os.path.join(R, 'Art', 'rig-pilot', 'r001', 'metrics')
sys.path.insert(0, HERE)
import bpy
import numpy as np
from mathutils import Vector

import clips as CL
import rigkit as K

CH = {'fighter': ('sk-hero-fighter', 'SK_Hero_Fighter'), 'wizard': ('sk-hero-wizard', 'SK_Hero_Wizard'), 'raider': ('sk-foe-raider', 'SK_Foe_Raider')}
args = sys.argv[sys.argv.index('--') + 1:]
os.makedirs(PREV, exist_ok=True)
os.makedirs(MET, exist_ok=True)


def glb(key, suffix=''):
    a, n = CH[key]
    return os.path.join(R, 'Art', a, 'r001', f'{n}{suffix}.glb')


def fresh():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.fps = 30
    return sc


def load(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    ao = next((o for o in new if o.type == 'ARMATURE'), None)
    meshes = [o for o in new if o.type == 'MESH']
    return ao, meshes


def mute_nla(ao):
    if ao.animation_data:
        for tr in ao.animation_data.nla_tracks:
            tr.mute = True


def set_pose(ao, action, frame):
    ao.animation_data_create()
    mute_nla(ao)
    ao.animation_data.action = bpy.data.actions[action] if action else None
    if not action:                       # real bind pose: clear every pose channel left over from the importer
        for pb in ao.pose.bones:
            pb.rotation_mode = 'QUATERNION'
            pb.rotation_quaternion = (1, 0, 0, 0)
            pb.location = (0, 0, 0)
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()


def eval_pts(ob):
    dg = bpy.context.evaluated_depsgraph_get()
    e = ob.evaluated_get(dg)
    m = e.to_mesh()
    P = np.array([ob.matrix_world @ v.co for v in m.vertices])
    e.to_mesh_clear()
    return P


def validate(key):
    sc = fresh()
    ao, meshes = load(glb(key))
    ob = max(meshes, key=lambda m: len(m.data.polygons))
    out = dict(character=key, file=os.path.basename(glb(key)))
    bones = [b.name for b in ao.data.bones]
    out['bones'] = len(bones)
    out['bones_match_contract'] = bones == K.BONE_ORDER or sorted(bones) == sorted(K.BONE_ORDER)
    out['bone_order_equal'] = bones == K.BONE_ORDER
    acts = {a.name: [round(a.frame_range[0], 2), round(a.frame_range[1], 2)] for a in bpy.data.actions}
    out['clips'] = acts
    expect = {n: [0.0, float(v[0])] for n, v in CL.CLIPS.items()}
    out['clips_match'] = {n: acts.get(n) == expect[n] for n in expect}
    out['tris_lod0'] = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    out['materials'] = [m.name for m in ob.data.materials]
    out['atlas_px'] = [tuple(i.size) for i in bpy.data.images if i.size[0] > 0]
    ca = ob.data.color_attributes
    out['color_attributes'] = [(c.name, c.data_type, c.domain) for c in ca]
    if len(ca):
        c = np.empty(len(ca[0].data) * 4, np.float32)
        ca[0].data.foreach_get('color', c)
        c = c.reshape(-1, 4)
        out['glow_alpha'] = dict(verts_alpha_gt_0=int((c[:, 3] > .01).sum()), verts=len(c), alpha_min=round(float(c[:, 3].min()), 3),
                                 alpha_max=round(float(c[:, 3].max()), 3), rgb_min=round(float(c[:, :3].min()), 3))
    vg = {g.index: g.name for g in ob.vertex_groups}
    heights = []
    body = []
    for v in ob.data.vertices:
        names = {vg[g.group] for g in v.groups if g.weight > .5}
        if not names & {'weapon_socket', 'shield_socket'}:
            body.append(v.co.z)
    out['body_height_m_excluding_held_items'] = round(max(body) - min(body), 3)
    out['max_influences'] = max(len([g for g in v.groups if g.weight > 1e-4]) for v in ob.data.vertices)
    set_pose(ao, None, 0)
    bind = eval_pts(ob)
    dz = {}
    for name, f in (('Idle', 30), ('Walk', 8), ('Walk', 23), ('Melee', 11), ('Ranged', 22), ('Cast', 22), ('Hit', 3), ('Downed', 60), ('Death', 66), ('Recover', 30)):
        set_pose(ao, name, f)
        P = eval_pts(ob)
        d = np.linalg.norm(P - bind, axis=1)
        dz[f'{name}@{f}'] = dict(max_cm=round(float(d.max()) * 100, 1), mean_cm=round(float(d.mean()) * 100, 1), moving_verts=int((d > .005).sum()))
        assert np.isfinite(P).all()
    out['deformation_vs_bind'] = dz
    # loops: first vs last pose, and root drift for every clip
    seam = {}
    drift = {}
    for name, (frames, loop, _) in CL.CLIPS.items():
        set_pose(ao, name, 0)
        A = eval_pts(ob)
        pb = ao.pose.bones['root']
        r0 = np.array(pb.matrix.translation)
        set_pose(ao, name, frames)
        Bp = eval_pts(ob)
        seam[name] = round(float(np.linalg.norm(A - Bp, axis=1).max()) * 1000, 3)
        drift[name] = round(float(np.linalg.norm(np.array(ao.pose.bones['root'].matrix.translation) - r0)) * 1000, 3)
        if name in ('Melee', 'Ranged', 'Cast', 'Hit', 'Recover'):
            pass
    out['seam_mm_first_vs_last_max_vertex'] = seam
    out['root_drift_mm'] = drift
    out['seconds'] = {n: round(v[0] / 30, 3) for n, v in CL.CLIPS.items()}
    # LODs from their own glbs
    lods = {}
    for lod in (1, 2):
        fresh()
        ao2, m2 = load(glb(key, f'_LOD{lod}'))
        o2 = max(m2, key=lambda m: len(m.data.polygons))
        lods[f'LOD{lod}'] = dict(tris=sum(len(p.vertices) - 2 for p in o2.data.polygons), bones=len(ao2.data.bones),
                                 same_bone_names=[b.name for b in ao2.data.bones] == bones)
    lods['LOD0'] = dict(tris=out['tris_lod0'])
    out['lods'] = lods
    out['lod_ratio'] = {k: round(v['tris'] / out['tris_lod0'], 3) for k, v in lods.items()}
    with open(os.path.join(MET, f'validate_{key}.json'), 'w') as fh:
        json.dump(out, fh, indent=1)
    print('VALIDATE', json.dumps(out))


# ------------------------------------------------------------------ rendering
def setup_render(w, h, bg=(.075, .085, .11)):
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE'
    sc.render.resolution_x, sc.render.resolution_y = w, h
    sc.render.resolution_percentage = 100
    sc.view_settings.view_transform = 'Standard'
    try:
        sc.eevee.taa_render_samples = 8
    except Exception:
        pass
    wd = bpy.data.worlds.new('w')
    wd.use_nodes = True
    wd.node_tree.nodes['Background'].inputs[0].default_value = (*bg, 1)
    wd.node_tree.nodes['Background'].inputs[1].default_value = 1.0
    sc.world = wd
    return sc


def unlit(ob):
    """Texture x vertex colour (RGB only) as emission: the authored colours, no lighting."""
    for slot in ob.material_slots:
        nt = slot.material.node_tree
        tex = next(n for n in nt.nodes if n.type == 'TEX_IMAGE')
        out = next(n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL')
        em = nt.nodes.new('ShaderNodeEmission')
        nt.links.new(tex.outputs['Color'], em.inputs['Color'])
        nt.links.new(em.outputs[0], out.inputs['Surface'])
        slot.material.use_backface_culling = True


def ortho_cam(scale, loc, direction):
    cd = bpy.data.cameras.new('c')
    cd.type = 'ORTHO'
    cd.ortho_scale = scale
    cam = bpy.data.objects.new('c', cd)
    bpy.context.scene.collection.objects.link(cam)
    d = Vector(direction).normalized()
    cam.location = Vector(loc) + d * 8
    cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
    cd.clip_end = 100
    bpy.context.scene.camera = cam
    return cam


def render(path):
    sc = bpy.context.scene
    sc.render.filepath = path
    sc.render.image_settings.file_format = 'PNG'
    bpy.ops.render.render(write_still=True)


def magick(*a):
    subprocess.run(['magick', *a], check=True)


def sheet_bind():
    tmp = tempfile.mkdtemp(prefix='rig147_')
    sc = fresh()
    setup_render(1800, 900)
    objs = {}
    for i, key in enumerate(('fighter', 'wizard', 'raider')):
        ao, ms = load(glb(key))
        ao.location.y = (i - 1) * 1.8
        for m in ms:
            unlit(m)
        objs[key] = (ao, ms)
    bpy.context.view_layer.update()
    cam = ortho_cam(2.5, (0, 0, 1.0), (1.0, -.0001, .0))
    cam.rotation_euler = (math.radians(90), 0, math.radians(90))
    cam.location = (8, 0, 1.0)
    # camera looks along -X from +X: heroes face the camera; shift to see their profile 3/4
    d = Vector((.75, -.66, 0)).normalized()
    cam.location = Vector((0, 0, 1.05)) + d * 8
    cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
    cam.data.ortho_scale = 4.8
    # tile 1: bind pose
    for key, (ao, ms) in objs.items():
        set_pose(ao, None, 0)
    render(tmp + '/bind.png')
    for key, (ao, ms) in objs.items():
        set_pose(ao, 'Idle', 0)
    render(tmp + '/idle.png')
    magick(tmp + '/bind.png', '-gravity', 'northwest', '-fill', 'white', '-pointsize', '34', '-annotate', '+14+10', 'Bind pose: fighter, wizard, raider (same scale)',
           tmp + '/bind_l.png')
    magick(tmp + '/idle.png', '-gravity', 'northwest', '-fill', 'white', '-pointsize', '28', '-annotate', '+14+10', 'Idle frame 0 (held items sit where the hands are in Idle)', tmp + '/idle_l.png')
    magick(tmp + '/bind_l.png', tmp + '/idle_l.png', '-append', os.path.join(PREV, 'sheet_01_characters_bind_idle.png'))


TILES = {'Idle': (0, 20, 40), 'Walk': (5, 13, 21), 'Melee': (7, 11, 20), 'Ranged': (10, 22, 26), 'Cast': (12, 22, 34), 'Hit': (3, 6, 10),
         'Downed': (8, 24, 48), 'Death': (12, 28, 54), 'Recover': (10, 30, 50)}


def sheet_clips():
    tmp = tempfile.mkdtemp(prefix='rig147_')
    labelled = {}
    for key, names in (('fighter', list(TILES)), ('wizard', ['Cast', 'Downed'])):
        sc = fresh()
        setup_render(300, 400)
        ao, ms = load(glb(key))
        for m in ms:
            unlit(m)
        cam = ortho_cam(2.3, (0, 0, .95), (.75, -.66, 0))
        for name in names:
            tiles = []
            for f in TILES[name]:
                set_pose(ao, name, f)
                px = ao.matrix_world @ ao.pose.bones['pelvis'].head
                d = Vector((.75, -.66, 0)).normalized()
                cam.location = Vector((px.x, px.y, .9)) + d * 8
                p = f'{tmp}/{key}_{name}_{f}.png'
                render(p)
                magick(p, '-gravity', 'southwest', '-fill', 'white', '-pointsize', '20', '-annotate', '+6+4', f'{name} f{f}', p)
                tiles.append(p)
            row = f'{tmp}/row_{key}_{name}.png'
            magick(*tiles, '+append', row)
            labelled[(key, name)] = row
    rows = [labelled[('fighter', n)] for n in TILES]
    # pair clips two per sheet row so the sheet stays compact
    pairs = []
    for i in range(0, len(rows), 2):
        grp = rows[i:i + 2]
        out = f'{tmp}/pair{i}.png'
        magick(*grp, '-background', '#14161c', '+append', out) if len(grp) == 2 else magick(grp[0], '-background', '#14161c', '-gravity', 'west', '-extent', '1800x400', out)
        pairs.append(out)
    magick(*pairs, '-append', os.path.join(PREV, 'sheet_02_fighter_clips.png'))
    magick(labelled[('wizard', 'Cast')], labelled[('wizard', 'Downed')], '+append', os.path.join(PREV, 'sheet_03_wizard_cast_downed.png'))


def stone_texture():
    n = 512
    rng = np.random.default_rng(147)
    A = np.ones((n, n, 4), np.float32)
    base = np.array((.085, .105, .145))
    nz = K.smooth_noise(rng, n, 32)
    A[..., :3] = base * (.8 + .5 * nz[..., None])
    for i in range(0, n, 128):                                 # irregular paving joints
        A[i:i + 3, :, :3] *= .45
        A[:, i:i + 3, :3] *= .45
    for r in range(4):
        off = int(rng.integers(0, 128))
        A[r * 128 + 3:(r + 1) * 128, off:off + 3, :3] *= .55
    return A


def sheet_stone():
    sc = fresh()
    setup_render(1920, 1080, bg=(.03, .035, .05))
    sc.render.film_transparent = False
    img = K.new_image('stone', stone_texture())
    me = bpy.data.meshes.new('floor')
    me.from_pydata([(-6, -6, 0), (6, -6, 0), (6, 6, 0), (-6, 6, 0)], [], [(0, 1, 2, 3)])
    uv = me.uv_layers.new(name='UVMap')
    for l, p in zip(me.loops, ((0, 0), (7, 0), (7, 7), (0, 7))):
        uv.data[l.index].uv = p
    fl = bpy.data.objects.new('floor', me)
    sc.collection.objects.link(fl)
    mat = bpy.data.materials.new('stone')
    nt = mat.node_tree
    b = nt.nodes['Principled BSDF']
    b.inputs['Roughness'].default_value = .9
    tn = nt.nodes.new('ShaderNodeTexImage')
    tn.image = img
    tn.interpolation = 'Closest'
    nt.links.new(tn.outputs['Color'], b.inputs['Base Color'])
    me.materials.append(mat)
    spots = {'fighter': (-1.2, 0.0, 0), 'wizard': (-.3, .2, 0), 'raider': (1.0, 0.0, 180)}
    for key, (x, y, yaw) in spots.items():
        ao, ms = load(glb(key))
        ao.location = (x, y, 0)
        ao.rotation_euler.z = math.radians(yaw)
        for m in ms:
            for s in m.material_slots:
                s.material.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 1.0
        set_pose(ao, 'Idle', 0)
    for name, rot, e, col in (('key', (math.radians(52), math.radians(8), math.radians(-35)), 3.2, (1, .93, .82)),
                              ('rim', (math.radians(118), math.radians(-6), math.radians(150)), 1.6, (.62, .74, 1.0))):
        ld = bpy.data.lights.new(name, 'SUN')
        ld.energy = e
        ld.color = col
        ld.angle = math.radians(5)
        o = bpy.data.objects.new(name, ld)
        o.rotation_euler = rot
        sc.collection.objects.link(o)
    sc.world.node_tree.nodes['Background'].inputs[0].default_value = (.05, .06, .09, 1)
    sc.world.node_tree.nodes['Background'].inputs[1].default_value = 1.2
    cd = bpy.data.cameras.new('cam')
    cd.angle = math.radians(40)
    cam = bpy.data.objects.new('cam', cd)
    sc.collection.objects.link(cam)
    pitch = math.radians(45)
    dist = 8.2
    tgt = Vector((-.1, 0, .75))
    d = Vector((0, -math.cos(pitch), math.sin(pitch)))
    cam.location = tgt + d * dist
    cam.rotation_euler = (math.radians(90) - pitch, 0, 0)
    sc.camera = cam
    render(os.path.join(PREV, 'sheet_04_stone_readability.png'))


mode = args[0]
if mode == 'validate':
    validate(args[1])
elif mode == 'sheet':
    {'bind': sheet_bind, 'clips': sheet_clips, 'stone': sheet_stone}[args[1]]()
