"""Fresh-process review of the batch 4 exports (#150): validation numbers and contact sheets for the bard and artificer.

    blender -b --python Art/rig-pilot/cast/scripts/review4.py -- validate <bard|artificer>
    blender -b --python Art/rig-pilot/cast/scripts/review4.py -- sheet4 bind|turn|clips|stone

review.py, review2.py and review3.py (batches 1 to 3) are not edited by batch 4: this script loads review2.py (and through it review.py and the r001
review functions) into its own namespace after registering the batch 4 classes, so validate expects the ten clips. validate runs review2's
validate_rev, which writes metrics/validate_<key>.json with an explicit `checks` table and `pass`; any failed check prints the failures and
EXITS NON-ZERO, so a failed bone, clip, LOD, seam, influence, height or glow check can never be recorded as a pass. Sheets need ImageMagick 7.
Start Blender with --python-exit-code 1 so an exception also exits non-zero.
"""
import math
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cast_characters as C   # noqa: E402  (puts the r001 scripts on sys.path)
import cast_batch4            # noqa: E402,F401  registers bard, artificer (and, through cast_batch3 and cast_batch2, batches 2 and 3 and the Brace clip)

_namespace = globals()
_args = sys.argv[sys.argv.index('--') + 1:]
_REVIEW2 = os.path.join(HERE, 'review2.py')
exec(compile(open(_REVIEW2).read().split('\nmode = _args[0]')[0], _REVIEW2, 'exec'), _namespace)   # batch 2 functions, which exec the batch 1 and r001 ones

import json                                                         # noqa: E402
import bpy                                                          # noqa: E402,F811
import numpy as np                                                  # noqa: E402,F811
from mathutils import Vector                                        # noqa: E402,F811

B3KEYS = ('bard', 'artificer')


def validate_checked(key):
    """review2's validate_rev: validate(), then gate() (every predicate recorded in the JSON as `checks` and `pass`) and exit 1 on any failure.
    The batch 4 classes carry no per-class override, so the gate expects exactly the ten shared clips."""
    validate_rev(key, 'r001')


ALL_RIGS3 = ALL_RIGS + B3KEYS
B4KEYS = B3KEYS
TILE_FRAMES3 = TILE_FRAMES2


def rig_glb(key):
    return glb(key)


def sheet4_bind():
    """The cleric, rogue, fighter and paladin (the nearest looks) beside the bard and artificer, bind pose over Idle frame 0, same scale."""
    tmp = tempfile.mkdtemp(prefix='cast150_', dir=os.path.join(R, '.local', 'scratch'))
    fresh()
    setup_render(2400, 800)
    objs = {}
    keys = ('cleric', 'bard', 'rogue', 'fighter', 'artificer', 'paladin')
    for i, key in enumerate(keys):
        ao, ms = load(glb(key) if key in B3KEYS or key not in LEGACY_REV2 else os.path.join(R, 'Art', CH[key][0], 'r002', f'{CH[key][1]}.glb'))
        ao.location.y = (i - 2.5) * 1.15
        for m in ms:
            unlit(m)
        objs[key] = (ao, ms)
    bpy.context.view_layer.update()
    cam = ortho_cam(6.9, (0, 0, 0.95), (1, 0, 0))
    cam.location = Vector((0, 0, 0.95)) + VIEW * 8
    cam.rotation_euler = (-VIEW).to_track_quat('-Z', 'Y').to_euler()
    for tag, action, label in (('bind', None, 'Bind pose: cleric, bard, rogue, fighter, artificer, paladin (same scale)'),
                               ('idle', 'Idle', 'Idle frame 0')):
        for key, (ao, ms) in objs.items():
            set_pose(ao, action, 0)
        render(f'{tmp}/{tag}.png')
        magick(f'{tmp}/{tag}.png', '-gravity', 'northwest', '-fill', 'white', '-pointsize', '30', '-annotate', '+14+10', label, f'{tmp}/{tag}_l.png')
    magick(f'{tmp}/bind_l.png', f'{tmp}/idle_l.png', '-append', os.path.join(PREV, 'sheet_23_batch4_bind_idle.png'))


def sheet4_turn():
    tmp = tempfile.mkdtemp(prefix='cast150_', dir=os.path.join(R, '.local', 'scratch'))
    rows = []
    for key in B3KEYS:
        fresh()
        setup_render(360, 540)
        ao, ms = load(glb(key))
        for m in ms:
            unlit(m)
        set_pose(ao, 'Idle', 0)
        cam = ortho_cam(2.3, (0, 0, .98), (1, 0, 0))
        tiles = []
        for name, deg_ in (('front', 0), ('three-quarter', -40), ('side', -90), ('back', 180)):
            d = Vector((math.cos(math.radians(deg_)), math.sin(math.radians(deg_)), 0))
            cam.location = Vector((0, 0, .98)) + d * 8
            cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
            p = f'{tmp}/{key}_{name}.png'
            render(p)
            magick(p, '-gravity', 'southwest', '-fill', 'white', '-pointsize', '20', '-annotate', '+6+4', f'{key} {name}', p)
            tiles.append(p)
        row = f'{tmp}/row_{key}.png'
        magick(*tiles, '+append', row)
        rows.append(row)
    magick(*rows, '-append', os.path.join(PREV, 'sheet_24_batch4_turnaround.png'))


def sheet4_clips():
    """All ten clips, three frames each, for each batch 4 class (sheets 25 and 26): three clips per row."""
    tmp = tempfile.mkdtemp(prefix='cast150_', dir=os.path.join(R, '.local', 'scratch'))
    for n, key in zip((25, 26), B3KEYS):
        fresh()
        setup_render(260, 380)
        ao, ms = load(glb(key))
        for m in ms:
            unlit(m)
        cam = ortho_cam(2.3, (0, 0, .98), (.75, -.66, 0))
        rows = []
        names = list(TILE_FRAMES3)
        for r in range(4):
            tiles = []
            for name in names[r * 3:(r + 1) * 3]:
                for f in TILE_FRAMES3[name]:
                    set_pose(ao, name, f)
                    px = ao.matrix_world @ ao.pose.bones['pelvis'].head
                    cam.location = Vector((px.x, px.y, .95)) + VIEW * 8
                    p = f'{tmp}/{key}_{name}_{f}.png'
                    render(p)
                    magick(p, '-gravity', 'southwest', '-fill', 'white', '-pointsize', '18', '-annotate', '+6+4', f'{name} f{f}', p)
                    tiles.append(p)
            row = f'{tmp}/row_{key}_{r}.png'
            magick(*tiles, '-background', '#14161c', '+append', '-gravity', 'west', '-extent', '2340x380', row)
            rows.append(row)
        magick(*rows, '-background', '#14161c', '-append', os.path.join(PREV, f'sheet_{n:02d}_{key}_clips.png'))


def sheet4_stone():
    """Two parties on dark stone at the gate-1 camera: (bard, artificer, cleric, paladin) and (fighter, bard, artificer, warlock)."""
    groups = (('a', ('bard', 'artificer', 'cleric', 'paladin')), ('b', ('fighter', 'bard', 'artificer', 'warlock')))
    tmp = tempfile.mkdtemp(prefix='cast150_', dir=os.path.join(R, '.local', 'scratch'))
    outs = []
    for tag, keys in groups:
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
        for key, (x, y) in zip(keys, ((-1.6, .1), (-.55, -.1), (.5, .1), (1.55, -.1))):
            ao, ms = load(glb(key) if key not in LEGACY_REV2 else os.path.join(R, 'Art', CH[key][0], 'r001', f'{CH[key][1]}.glb'))
            ao.location = (x, y, 0)
            for m in ms:
                for s_ in m.material_slots:
                    s_.material.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 1.0
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
        d = Vector((0, -math.cos(pitch), math.sin(pitch)))
        cam.location = Vector((0, 0, .75)) + d * 7.0
        cam.rotation_euler = (math.radians(90) - pitch, 0, 0)
        sc.camera = cam
        p = f'{tmp}/{tag}.png'
        render(p)
        magick(p, '-gravity', 'northwest', '-fill', 'white', '-pointsize', '28', '-annotate', '+12+8', ' / '.join(keys), p)
        outs.append(p)
    magick(*outs, '-append', os.path.join(PREV, 'sheet_27_batch4_stone_readability.png'))


mode = _args[0]
if mode == 'validate':
    validate_checked(_args[1])
elif mode == 'sheet4':
    {'bind': sheet4_bind, 'turn': sheet4_turn, 'clips': sheet4_clips, 'stone': sheet4_stone}[_args[1]]()
