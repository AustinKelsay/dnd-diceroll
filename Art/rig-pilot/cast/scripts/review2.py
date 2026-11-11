"""Fresh-process review of the batch 2 exports (#150): validation numbers and contact sheets for barbarian, monk, paladin, ranger and the
r002 re-exports of the five earlier characters (ten clips).

    blender -b --python Art/rig-pilot/cast/scripts/review2.py -- validate <key> [--r002]
    blender -b --python Art/rig-pilot/cast/scripts/review2.py -- sheet2 bind|turn|clips|brace|stone

review.py (batch 1) is not edited, so the batch 1 asset records that hash it stay valid: this script loads it (and through it the r001
review functions) into its own namespace after registering the batch 2 classes and the Brace clip, so validate expects ten clips.
validate <key> writes metrics/validate_<key>.json (or validate_<key>_r002.json with --r002). Sheets need ImageMagick 7.
"""
import math
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cast_characters as C   # noqa: E402  (puts the r001 scripts on sys.path)
import cast_batch2            # noqa: E402,F401  registers barbarian, monk, paladin, ranger and the Brace clip (the clip table has ten)

_namespace = globals()
_args = sys.argv[sys.argv.index('--') + 1:]
_REVIEW = os.path.join(HERE, 'review.py')
exec(compile(open(_REVIEW).read().split('mode = _args[0]')[0], _REVIEW, 'exec'), _namespace)   # batch 1 functions, which exec the r001 ones

import bpy                                                          # noqa: E402,F811
import numpy as np                                                  # noqa: E402,F811
from mathutils import Vector                                        # noqa: E402,F811

# ------------------------------------------------------------------ batch 2 (barbarian, monk, paladin, ranger) and Brace
B2 = ('barbarian', 'monk', 'paladin', 'ranger')
ALL_RIGS = ('fighter', 'wizard', 'raider', 'rogue', 'cleric') + B2
LEGACY_REV2 = {'fighter', 'wizard', 'raider', 'rogue', 'cleric'}
_glb_r001 = glb


def validate_rev(key, rev):
    """validate() for one character; rev 'r002' reads the re-exported LOD0 glb (ten clips) and writes validate_<key>_r002.json."""
    global glb, MET
    real_met = MET
    if rev == 'r002':
        def glb(k, suffix=''):
            a, n = CH[k]
            return os.path.join(R, 'Art', a, 'r002' if suffix == '' else 'r001', f'{n}{suffix}.glb')
        import tempfile as _t
        MET = _t.mkdtemp(prefix='cast150_val_')
    validate(key)
    if rev == 'r002':
        import shutil
        shutil.move(os.path.join(MET, f'validate_{key}.json'), os.path.join(real_met, f'validate_{key}_r002.json'))
        glb, MET = _glb_r001, real_met


def _party(sc, keys, xs, yaws=None):
    objs = {}
    for i, key in enumerate(keys):
        ao, ms = load(glb(key))
        ao.location = (xs[i], 0, 0)
        if yaws:
            ao.rotation_euler.z = math.radians(yaws[i])
        objs[key] = (ao, ms)
    return objs


def sheet2_bind():
    """Fighter beside the four batch 2 classes, bind pose over Idle frame 0, same scale."""
    tmp = tempfile.mkdtemp(prefix='cast150_')
    fresh()
    setup_render(2200, 800)
    objs = {}
    keys = ('fighter',) + B2
    for i, key in enumerate(keys):
        ao, ms = load(glb(key))
        ao.location.y = (i - 2) * 1.2
        for m in ms:
            unlit(m)
        objs[key] = (ao, ms)
    bpy.context.view_layer.update()
    cam = ortho_cam(6.3, (0, 0, 0.95), (1, 0, 0))
    cam.location = Vector((0, 0, 0.95)) + VIEW * 8
    cam.rotation_euler = (-VIEW).to_track_quat('-Z', 'Y').to_euler()
    for tag, action, label in (('bind', None, 'Bind pose: fighter, barbarian, monk, paladin, ranger (same scale)'),
                               ('idle', 'Idle', 'Idle frame 0 (the longbow sits tilted: it is authored upright at the Ranged hold)')):
        for key, (ao, ms) in objs.items():
            set_pose(ao, action, 0)
        render(f'{tmp}/{tag}.png')
        magick(f'{tmp}/{tag}.png', '-gravity', 'northwest', '-fill', 'white', '-pointsize', '30', '-annotate', '+14+10', label, f'{tmp}/{tag}_l.png')
    magick(f'{tmp}/bind_l.png', f'{tmp}/idle_l.png', '-append', os.path.join(PREV, 'sheet_06_batch2_bind_idle.png'))


def sheet2_turn():
    """Barbarian, monk, paladin, ranger from the front, 3/4, side and back at Idle frame 0."""
    tmp = tempfile.mkdtemp(prefix='cast150_')
    rows = []
    for key in B2:
        fresh()
        setup_render(360, 520)
        ao, ms = load(glb(key))
        for m in ms:
            unlit(m)
        set_pose(ao, 'Idle', 0)
        cam = ortho_cam(2.2, (0, 0, .95), (1, 0, 0))
        tiles = []
        for name, deg_ in (('front', 0), ('three-quarter', -40), ('side', -90), ('back', 180)):
            d = Vector((math.cos(math.radians(deg_)), math.sin(math.radians(deg_)), 0))
            cam.location = Vector((0, 0, .95)) + d * 8
            cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
            p = f'{tmp}/{key}_{name}.png'
            render(p)
            magick(p, '-gravity', 'southwest', '-fill', 'white', '-pointsize', '20', '-annotate', '+6+4', f'{key} {name}', p)
            tiles.append(p)
        row = f'{tmp}/row_{key}.png'
        magick(*tiles, '+append', row)
        rows.append(row)
    magick(*rows, '-append', os.path.join(PREV, 'sheet_07_batch2_turnaround.png'))


TILE_FRAMES2 = {**TILE_FRAMES, 'Brace': (5, 11, 17)}


def sheet2_clips():
    """All ten clips, three frames each, for each batch 2 class (sheets 08 to 11): three clips per row."""
    tmp = tempfile.mkdtemp(prefix='cast150_')
    for n, key in zip((8, 9, 10, 11), B2):
        fresh()
        setup_render(260, 360)
        ao, ms = load(glb(key))
        for m in ms:
            unlit(m)
        cam = ortho_cam(2.2, (0, 0, .95), (.75, -.66, 0))
        rows = []
        names = list(TILE_FRAMES2)
        for r in range(4):
            tiles = []
            for name in names[r * 3:(r + 1) * 3]:
                for f in TILE_FRAMES2[name]:
                    set_pose(ao, name, f)
                    px = ao.matrix_world @ ao.pose.bones['pelvis'].head
                    cam.location = Vector((px.x, px.y, .9)) + VIEW * 8
                    p = f'{tmp}/{key}_{name}_{f}.png'
                    render(p)
                    magick(p, '-gravity', 'southwest', '-fill', 'white', '-pointsize', '18', '-annotate', '+6+4', f'{name} f{f}', p)
                    tiles.append(p)
            row = f'{tmp}/row_{key}_{r}.png'
            magick(*tiles, '-background', '#14161c', '+append', '-gravity', 'west', '-extent', '2340x360', row)
            rows.append(row)
        magick(*rows, '-background', '#14161c', '-append', os.path.join(PREV, f'sheet_{n:02d}_{key}_clips.png'))


def sheet2_brace():
    """Brace on every rig (frames 0, 5, 11, 17, 24), one rig per row: the fifth clip of the shared set looks right on all nine."""
    tmp = tempfile.mkdtemp(prefix='cast150_')
    rows = []
    for key in ALL_RIGS:
        fresh()
        setup_render(230, 330)
        path = os.path.join(R, 'Art', CH[key][0], 'r002' if key in LEGACY_REV2 else 'r001', f'{CH[key][1]}.glb')
        ao, ms = load(path)
        for m in ms:
            unlit(m)
        cam = ortho_cam(2.2, (0, 0, .95), (.75, -.66, 0))
        tiles = []
        for f in (0, 5, 11, 17, 24):
            set_pose(ao, 'Brace', f)
            px = ao.matrix_world @ ao.pose.bones['pelvis'].head
            cam.location = Vector((px.x, px.y, .9)) + VIEW * 8
            p = f'{tmp}/{key}_{f}.png'
            render(p)
            magick(p, '-gravity', 'southwest', '-fill', 'white', '-pointsize', '18', '-annotate', '+6+4', f'{key} Brace f{f}', p)
            tiles.append(p)
        row = f'{tmp}/row_{key}.png'
        magick(*tiles, '+append', row)
        rows.append(row)
    magick(*rows[:5], '-background', '#14161c', '-append', os.path.join(PREV, 'sheet_12_brace_all_rigs_a.png'))
    magick(*rows[5:], '-background', '#14161c', '-append', os.path.join(PREV, 'sheet_13_brace_all_rigs_b.png'))


def sheet2_stone():
    """Two parties on dark stone at the gate-1 camera: (fighter, paladin, cleric, wizard) and (rogue, ranger, monk, barbarian), and the
    barbarian beside the raider foe and the fighter, lit like the earlier readability sheets."""
    groups = (('a', ('fighter', 'paladin', 'cleric', 'wizard')), ('b', ('rogue', 'ranger', 'monk', 'barbarian')), ('c', ('barbarian', 'raider', 'fighter', 'monk')))
    tmp = tempfile.mkdtemp(prefix='cast150_')
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
    magick(*outs, '-append', os.path.join(PREV, 'sheet_14_batch2_stone_readability.png'))


mode = _args[0]
if mode == 'validate':
    validate_rev(_args[1], 'r002' if '--r002' in _args else 'r001')
elif mode == 'sheet':
    {'bind': sheet_bind, 'turn': sheet_turn, 'clips': sheet_clips, 'stone': sheet_stone}[_args[1]]()
elif mode == 'sheet2':
    {'bind': sheet2_bind, 'turn': sheet2_turn, 'clips': sheet2_clips, 'brace': sheet2_brace, 'stone': sheet2_stone}[_args[1]]()
