"""Fresh-process review of the batch 2 exports (#150): validation numbers and contact sheets for barbarian, monk, paladin, ranger and the
r002 re-exports of the five earlier characters (ten clips).

    blender -b --python-exit-code 1 --python Art/rig-pilot/cast/scripts/review2.py -- validate <key> [--r002]
    blender -b --python-exit-code 1 --python Art/rig-pilot/cast/scripts/review2.py -- sheet2 bind|turn|clips|brace|stone|override

Every command takes `--python-exit-code 1`: Blender then exits 1 when the script raises (an uncaught exception otherwise still exits 0, so a failed validation could read as success). The gates also exit 1 themselves.

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


SEAM_CLIPS = ('Idle', 'Walk', 'Melee', 'Ranged', 'Cast', 'Hit', 'Brace')       # loops, and the one-shots that start and end on Idle frame 0
OVERRIDE_CLIPS = {'ranger': ('Ranged_Ranger', 36), 'barbarian': ('Melee_Barbarian', 30)}
SHARED_OF = {'ranger': 'Ranged', 'barbarian': 'Melee'}


def override_checks(key, rev):
    """The per-class override clip (overrides.py) in a fresh process: present with its frame range, starts and ends on Idle frame 0 (maximum vertex
    distance to Idle frame 0, both ends), root drift 0."""
    out = {}
    if key not in OVERRIDE_CLIPS or rev != 'r001':
        return out
    action, frames = OVERRIDE_CLIPS[key]
    fresh()
    ao, meshes = load(glb(key))
    ob = max(meshes, key=lambda m: len(m.data.polygons))
    if action not in bpy.data.actions:
        return {'present': False}
    a = bpy.data.actions[action]
    set_pose(ao, 'Idle', 0)
    idle0 = eval_pts(ob)
    set_pose(ao, action, 0)
    first = eval_pts(ob)
    r0 = np.array(ao.pose.bones['root'].matrix.translation)
    set_pose(ao, action, frames)
    last = eval_pts(ob)
    out = dict(present=True, range=[round(a.frame_range[0], 2), round(a.frame_range[1], 2)],
               first_vs_idle0_mm=round(float(np.linalg.norm(first - idle0, axis=1).max()) * 1000, 3),
               last_vs_idle0_mm=round(float(np.linalg.norm(last - idle0, axis=1).max()) * 1000, 3),
               root_drift_mm=round(float(np.linalg.norm(np.array(ao.pose.bones['root'].matrix.translation) - r0)) * 1000, 3))
    return out


def gate(key, rev, path):
    """Every predicate of a validation JSON, recorded in it as `checks` and `pass`. Returns the list of failures."""
    d = json.load(open(path))
    fails = []

    def check(name, ok, detail=''):
        d.setdefault('checks', {})[name] = bool(ok)
        if not ok:
            fails.append(f'{name} {detail}'.strip())
    hero = key != 'raider'
    check('bones: 25 and the contract', d['bones'] == 25 and d['bones_match_contract'], f"({d['bones']})")
    check('clips: all ten with their frame ranges', len(d['clips_match']) == 10 and all(d['clips_match'].values()), str(d['clips_match']))
    check('lods: same bone names, LOD1 and LOD2', all(d['lods'][k]['same_bone_names'] for k in ('LOD1', 'LOD2')))
    check('lods: LOD1 40-55% and LOD2 20-32% of LOD0', 0.40 <= d['lod_ratio']['LOD1'] <= 0.55 and 0.20 <= d['lod_ratio']['LOD2'] <= 0.32, str(d['lod_ratio']))
    seams = d['seam_mm_first_vs_last_max_vertex']
    check('seams: loops and Idle-to-Idle one-shots close to 0.01 mm', all(seams[c] <= 0.01 for c in SEAM_CLIPS), str({c: seams[c] for c in SEAM_CLIPS}))
    check('root drift 0.01 mm or less in every clip', max(d['root_drift_mm'].values()) <= 0.01, str(d['root_drift_mm']))
    check('at most 4 influences per vertex', d['max_influences'] <= 4, str(d['max_influences']))
    check('one material on a 1024x1024 atlas', len(d['materials']) == 1 and [tuple(x) for x in d['atlas_px']] == [(1024, 1024)], str((d['materials'], d['atlas_px'])))
    check('glow alpha present on runes and gems', d['glow_alpha']['verts_alpha_gt_0'] > 0 and d['glow_alpha']['alpha_max'] <= 1.0 + 1e-6)
    if hero:
        check('height 175 to 185 cm excluding held items', 1.75 <= d['body_height_m_excluding_held_items'] <= 1.85, str(d['body_height_m_excluding_held_items']))
        check('LOD0 3,000 to 6,000 triangles', 3000 <= d['lods']['LOD0']['tris'] <= 6000, str(d['lods']['LOD0']['tris']))
    ov = override_checks(key, rev)
    if key in OVERRIDE_CLIPS and rev == 'r001':
        action, frames = OVERRIDE_CLIPS[key]
        d['override'] = ov
        check(f'override {action}: present with frames 0 to {frames}', ov.get('present') and ov.get('range') == [0.0, float(frames)], str(ov))
        check(f'override {action}: starts and ends on Idle frame 0 (0.01 mm)', ov.get('present') and ov['first_vs_idle0_mm'] <= 0.01 and ov['last_vs_idle0_mm'] <= 0.01, str(ov))
        check(f'override {action}: root drift 0', ov.get('present') and ov['root_drift_mm'] <= 0.01, str(ov))
    d['pass'] = not fails
    d['failures'] = fails
    with open(path, 'w') as fh:
        json.dump(d, fh, indent=1)
    return fails


def validate_rev(key, rev):
    """validate() for one character, then gate(): every predicate enforced, exit 1 on any failure. rev 'r002' reads the re-exported LOD0 glb
    (ten clips) and writes validate_<key>_r002.json."""
    global glb, MET
    real_met = MET
    if rev == 'r002':
        def glb(k, suffix=''):
            a, n = CH[k]
            return os.path.join(R, 'Art', a, 'r002' if suffix == '' else 'r001', f'{n}{suffix}.glb')
        import tempfile as _t
        MET = _t.mkdtemp(prefix='cast150_val_', dir=os.path.join(R, '.local', 'scratch'))
    validate(key)
    if rev == 'r002':
        import shutil
        shutil.move(os.path.join(MET, f'validate_{key}.json'), os.path.join(real_met, f'validate_{key}_r002.json'))
        shutil.rmtree(MET, ignore_errors=True)
        MET = real_met
    path = os.path.join(real_met, f'validate_{key}.json' if rev == 'r001' else f'validate_{key}_r002.json')
    fails = gate(key, rev, path)
    glb = _glb_r001
    if fails:
        print('VALIDATION FAILED', key, rev, '; '.join(fails))
        sys.stdout.flush()
        sys.exit(1)
    print('VALIDATION PASSED', key, rev)


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
    tmp = tempfile.mkdtemp(prefix='cast150_', dir=os.path.join(R, '.local', 'scratch'))
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
    tmp = tempfile.mkdtemp(prefix='cast150_', dir=os.path.join(R, '.local', 'scratch'))
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
    tmp = tempfile.mkdtemp(prefix='cast150_', dir=os.path.join(R, '.local', 'scratch'))
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
                shown = OVERRIDE_CLIPS[key][0] if SHARED_OF.get(key) == name else name       # the class's own clip stands for the shared one it replaces
                for f in TILE_FRAMES2[name]:
                    set_pose(ao, shown, f)
                    px = ao.matrix_world @ ao.pose.bones['pelvis'].head
                    cam.location = Vector((px.x, px.y, .9)) + VIEW * 8
                    p = f'{tmp}/{key}_{name}_{f}.png'
                    render(p)
                    magick(p, '-gravity', 'southwest', '-fill', 'white', '-pointsize', '18', '-annotate', '+6+4', f'{shown if shown != name else name} f{f}', p)
                    tiles.append(p)
            row = f'{tmp}/row_{key}_{r}.png'
            magick(*tiles, '-background', '#14161c', '+append', '-gravity', 'west', '-extent', '2340x360', row)
            rows.append(row)
        magick(*rows, '-background', '#14161c', '-append', os.path.join(PREV, f'sheet_{n:02d}_{key}_clips.png'))


def sheet2_brace():
    """Brace on every rig (frames 0, 5, 11, 17, 24), one rig per row: the fifth clip of the shared set looks right on all nine."""
    tmp = tempfile.mkdtemp(prefix='cast150_', dir=os.path.join(R, '.local', 'scratch'))
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
    magick(*outs, '-append', os.path.join(PREV, 'sheet_14_batch2_stone_readability.png'))


def sheet2_override():
    """The two per-class clips (overrides.py) beside the shared clip they replace: the ranger's Ranged_Ranger and the barbarian's Melee_Barbarian,
    seen from the three-quarter view and from the front (the string and the haft are easiest to read from there). Writes sheet_15 and sheet_16."""
    tmp = tempfile.mkdtemp(prefix='cast150_', dir=os.path.join(R, '.local', 'scratch'))
    spec = (('ranger', 'Ranged_Ranger', 'Ranged', (0, 5, 9, 14, 20, 23, 27, 29, 32, 36), 15), ('barbarian', 'Melee_Barbarian', 'Melee', (0, 4, 7, 9, 11, 13, 16, 20, 24, 30), 16))
    for key, action, shared, frames, n in spec:
        fresh()
        setup_render(220, 330)
        ao, ms = load(glb(key))
        for m in ms:
            unlit(m)
        cam = ortho_cam(2.2, (0, 0, .95), (.75, -.66, 0))
        rows = []
        for label, name in ((action, action), (f'shared {shared}', shared)):
            for view, vec in (('3/4', VIEW), ('front', Vector((1, 0, 0)))):
                tiles = []
                for f in frames:
                    fr = min(f, 36 if shared == 'Ranged' else 30)
                    set_pose(ao, name, fr)
                    px = ao.matrix_world @ ao.pose.bones['pelvis'].head
                    cam.location = Vector((px.x, px.y, .9)) + vec * 8
                    cam.rotation_euler = (-vec).to_track_quat('-Z', 'Y').to_euler()
                    p = f'{tmp}/{key}_{name}_{view[0]}_{f}.png'
                    render(p)
                    magick(p, '-gravity', 'southwest', '-fill', 'white', '-pointsize', '16', '-annotate', '+4+3', f'{label} {view} f{f}', p)
                    tiles.append(p)
                row = f'{tmp}/row_{key}_{name}_{view[0]}.png'
                magick(*tiles, '+append', row)
                rows.append(row)
        magick(*rows, '-background', '#14161c', '-append', os.path.join(PREV, f'sheet_{n}_{key}_override_clip.png'))


mode = _args[0]
if mode == 'validate':
    validate_rev(_args[1], 'r002' if '--r002' in _args else 'r001')
elif mode == 'sheet':
    {'bind': sheet_bind, 'turn': sheet_turn, 'clips': sheet_clips, 'stone': sheet_stone}[_args[1]]()
elif mode == 'sheet2':
    {'bind': sheet2_bind, 'turn': sheet2_turn, 'clips': sheet2_clips, 'brace': sheet2_brace, 'stone': sheet2_stone, 'override': sheet2_override}[_args[1]]()
