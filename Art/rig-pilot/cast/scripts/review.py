"""Fresh-process review of the cast exports: validation numbers and contact sheets.

    blender -b --python Art/rig-pilot/cast/scripts/review.py -- validate rogue|cleric
    blender -b --python Art/rig-pilot/cast/scripts/review.py -- sheet bind|turn|clips|stone

validate re-imports <name>.glb and the two LOD glbs into an empty scene and writes metrics/validate_<key>.json (bones, clips and
frame ranges at 30 fps, tris per LOD, loop seams, root drift, influences, glow alpha). It reuses the r001 review functions
unchanged (loaded from Art/rig-pilot/r001/scripts/review.py with this folder's character table and output folders); the sheets
are new. Sheets need ImageMagick 7.
"""
import math
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cast_characters as C   # noqa: E402  (puts the r001 scripts on sys.path)

R001_REVIEW = os.path.join(C.R001, 'review.py')
_namespace = globals()
_args = sys.argv[sys.argv.index('--') + 1:]
_source = open(R001_REVIEW).read().split('mode = args[0]')[0]
exec(compile(_source, R001_REVIEW, 'exec'), _namespace)           # defines fresh, load, validate, setup_render, unlit, ortho_cam, render, magick, ...

import bpy                                                          # noqa: E402,F811
from mathutils import Vector                                        # noqa: E402,F811

CAST_ROOT = os.path.abspath(os.path.join(HERE, '..'))
PREV = os.path.join(CAST_ROOT, 'previews')
MET = os.path.join(CAST_ROOT, 'metrics')
os.makedirs(PREV, exist_ok=True)
os.makedirs(MET, exist_ok=True)
CH.update({k: (C.ASSET_IDS[k], C.CHARACTERS[k][1]) for k in C.CHARACTERS})
CH['fighter'] = ('sk-hero-fighter', 'SK_Hero_Fighter')
CH['wizard'] = ('sk-hero-wizard', 'SK_Hero_Wizard')
VIEW = Vector((.75, -.66, 0)).normalized()


def sheet_bind():
    """Fighter, rogue, cleric and wizard side by side, bind pose over Idle frame 0, same scale."""
    tmp = tempfile.mkdtemp(prefix='cast150_')
    fresh()
    setup_render(2000, 800)
    objs = {}
    keys = ('fighter', 'rogue', 'cleric', 'wizard')
    for i, key in enumerate(keys):
        ao, ms = load(glb(key))
        ao.location.y = (i - 1.5) * 1.5
        for m in ms:
            unlit(m)
        objs[key] = (ao, ms)
    bpy.context.view_layer.update()
    cam = ortho_cam(4.4, (0, 0, 1.0), (1, 0, 0))
    cam.location = Vector((0, 0, 1.0)) + VIEW * 8
    cam.rotation_euler = (-VIEW).to_track_quat('-Z', 'Y').to_euler()
    for tag, action, label in (('bind', None, 'Bind pose: fighter, rogue, cleric, wizard (same scale)'), ('idle', 'Idle', 'Idle frame 0 (held items sit where the hands are in Idle)')):
        for key, (ao, ms) in objs.items():
            set_pose(ao, action, 0)
        render(f'{tmp}/{tag}.png')
        magick(f'{tmp}/{tag}.png', '-gravity', 'northwest', '-fill', 'white', '-pointsize', '30', '-annotate', '+14+10', label, f'{tmp}/{tag}_l.png')
    magick(f'{tmp}/bind_l.png', f'{tmp}/idle_l.png', '-append', os.path.join(PREV, 'sheet_01_cast_bind_idle.png'))


def sheet_turn():
    """Rogue and cleric from the front, 3/4, side and back at Idle frame 0."""
    tmp = tempfile.mkdtemp(prefix='cast150_')
    rows = []
    for key in ('rogue', 'cleric'):
        fresh()
        setup_render(360, 520)
        ao, ms = load(glb(key))
        for m in ms:
            unlit(m)
        set_pose(ao, 'Idle', 0)
        cam = ortho_cam(2.1, (0, 0, .92), (1, 0, 0))
        tiles = []
        for name, deg_ in (('front', 0), ('three-quarter', -40), ('side', -90), ('back', 180)):
            d = Vector((math.cos(math.radians(deg_)), math.sin(math.radians(deg_)), 0))
            cam.location = Vector((0, 0, .92)) + d * 8
            cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
            p = f'{tmp}/{key}_{name}.png'
            render(p)
            magick(p, '-gravity', 'southwest', '-fill', 'white', '-pointsize', '20', '-annotate', '+6+4', f'{key} {name}', p)
            tiles.append(p)
        row = f'{tmp}/row_{key}.png'
        magick(*tiles, '+append', row)
        rows.append(row)
    magick(*rows, '-append', os.path.join(PREV, 'sheet_02_cast_turnaround.png'))


TILE_FRAMES = {'Idle': (0, 20, 40), 'Walk': (5, 13, 21), 'Melee': (7, 11, 20), 'Ranged': (10, 22, 26), 'Cast': (12, 22, 34), 'Hit': (3, 6, 10),
               'Downed': (8, 24, 48), 'Death': (12, 28, 54), 'Recover': (10, 30, 50)}


def sheet_clips():
    """All nine clips, three frames each, for the rogue (sheet_03) and the cleric (sheet_04): three clips per row."""
    tmp = tempfile.mkdtemp(prefix='cast150_')
    for n, key in ((3, 'rogue'), (4, 'cleric')):
        fresh()
        setup_render(260, 360)
        ao, ms = load(glb(key))
        for m in ms:
            unlit(m)
        cam = ortho_cam(2.2, (0, 0, .95), (.75, -.66, 0))
        rows = []
        names = list(TILE_FRAMES)
        for r in range(3):
            tiles = []
            for name in names[r * 3:(r + 1) * 3]:
                for f in TILE_FRAMES[name]:
                    set_pose(ao, name, f)
                    px = ao.matrix_world @ ao.pose.bones['pelvis'].head
                    cam.location = Vector((px.x, px.y, .9)) + VIEW * 8
                    p = f'{tmp}/{key}_{name}_{f}.png'
                    render(p)
                    magick(p, '-gravity', 'southwest', '-fill', 'white', '-pointsize', '18', '-annotate', '+6+4', f'{name} f{f}', p)
                    tiles.append(p)
            row = f'{tmp}/row_{key}_{r}.png'
            magick(*tiles, '+append', row)
            rows.append(row)
        magick(*rows, '-background', '#14161c', '-append', os.path.join(PREV, f'sheet_0{n}_{key}_clips.png'))


def sheet_stone():
    """The party of four on dark stone at the gate-1 camera (FOV 40, 45 degrees), lit like the r001 readability sheet."""
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
    for key, (x, y, yaw) in {'fighter': (-1.6, .1, 0), 'rogue': (-.55, -.1, 0), 'cleric': (.5, .1, 0), 'wizard': (1.55, -.1, 0)}.items():
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
    dist = 7.0
    tgt = Vector((0, 0, .75))
    d = Vector((0, -math.cos(pitch), math.sin(pitch)))
    cam.location = tgt + d * dist
    cam.rotation_euler = (math.radians(90) - pitch, 0, 0)
    sc.camera = cam
    render(os.path.join(PREV, 'sheet_05_cast_stone_readability.png'))


mode = _args[0]
if mode == 'validate':
    validate(_args[1])
elif mode == 'sheet':
    {'bind': sheet_bind, 'turn': sheet_turn, 'clips': sheet_clips, 'stone': sheet_stone}[_args[1]]()
