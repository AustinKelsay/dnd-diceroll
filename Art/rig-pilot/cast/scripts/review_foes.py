"""Fresh-process review of the foe batch exports (#150): validation numbers and contact sheets for the scout, archer, skirmisher, brute and warden.

    blender -b --python-exit-code 1 --python Art/rig-pilot/cast/scripts/review_foes.py -- validate <scout|archer|skirmisher|brute|warden>
    blender -b --python-exit-code 1 --python Art/rig-pilot/cast/scripts/review_foes.py -- sheetf bind|turn|clips|stone|override

The earlier review scripts are not edited: this script registers the foe classes, loads review2.py (and through it review.py and the r001 review
functions) into its own namespace, and replaces the validation gate with the foe one. validate runs the r001 validation, then writes
metrics/validate_<key>.json with an explicit `checks` table and `pass`; any failed check prints the failures and EXITS NON-ZERO, so a failed bone, clip,
LOD, seam, influence, height, override or glow check can never be recorded as a pass. Sheets need ImageMagick 7 and are named sheet_f01..; start
Blender with --python-exit-code 1 so an exception also exits non-zero.

Foe height bands (lead calls): scout, archer and skirmisher 175 to 185 cm; the brute up to about 205 cm (its horns); the warden up to about 210 cm
(its crest). Heights exclude held items (vertices weighted to weapon_socket or shield_socket).
"""
import math
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cast_characters as C   # noqa: E402  (puts the r001 scripts on sys.path)
import cast_foes              # noqa: E402,F401  registers the foes (and, through cast_batch2, batch 2 and the Brace clip)
import overrides_foes as OF   # noqa: E402

_namespace = globals()
_args = sys.argv[sys.argv.index('--') + 1:]
_REVIEW2 = os.path.join(HERE, 'review2.py')
exec(compile(open(_REVIEW2).read().split('\nmode = _args[0]')[0], _REVIEW2, 'exec'), _namespace)   # batch 2 functions, which exec the batch 1 and r001 ones

import json                                                         # noqa: E402
import bpy                                                          # noqa: E402,F811
import numpy as np                                                  # noqa: E402,F811
from mathutils import Vector                                        # noqa: E402,F811

FOES = ('scout', 'archer', 'skirmisher', 'brute', 'warden')
HEIGHT_BAND = {'scout': (1.75, 1.85), 'archer': (1.75, 1.85), 'skirmisher': (1.75, 1.85), 'brute': (1.85, 2.06), 'warden': (1.85, 2.10)}
OVERRIDE_FOE = OF.OVERRIDES      # key -> (shared clip, action, frames)


def foe_override_checks(key):
    """The per-class override clip in a fresh process: present with its frame range, starts and ends on Idle frame 0 (maximum vertex distance to Idle
    frame 0), root drift 0; for the archer also the knife's hide-and-show scale."""
    if key not in OVERRIDE_FOE:
        return {}
    action, frames = OVERRIDE_FOE[key][1], OVERRIDE_FOE[key][2]
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
    if key == 'archer':
        scale = {}
        for f in range(frames + 1):
            set_pose(ao, action, f)
            scale[f] = round(float(ao.pose.bones['weapon_socket'].matrix.to_scale().x), 4)
        out['blade_scale_by_frame'] = scale
    return out


def foe_gate(key, path):
    """Every predicate of a validation JSON, recorded in it as `checks` and `pass`. Returns the list of failures."""
    d = json.load(open(path))
    fails = []

    def check(name, ok, detail=''):
        d.setdefault('checks', {})[name] = bool(ok)
        if not ok:
            fails.append(f'{name} {detail}'.strip())
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
    lo, hi = HEIGHT_BAND[key]
    check(f'height {lo * 100:.0f} to {hi * 100:.0f} cm excluding held items', lo <= d['body_height_m_excluding_held_items'] <= hi, str(d['body_height_m_excluding_held_items']))
    check('LOD0 3,000 to 6,000 triangles', 3000 <= d['lods']['LOD0']['tris'] <= 6000, str(d['lods']['LOD0']['tris']))
    ov = foe_override_checks(key)
    if key in OVERRIDE_FOE:
        action, frames = OVERRIDE_FOE[key][1], OVERRIDE_FOE[key][2]
        d['override'] = ov
        check(f'override {action}: present with frames 0 to {frames}', ov.get('present') and ov.get('range') == [0.0, float(frames)], str(ov))
        check(f'override {action}: starts and ends on Idle frame 0 (0.01 mm)', ov.get('present') and ov['first_vs_idle0_mm'] <= 0.01 and ov['last_vs_idle0_mm'] <= 0.01, str(ov))
        check(f'override {action}: root drift 0', ov.get('present') and ov['root_drift_mm'] <= 0.01, str(ov))
        if key == 'archer':
            bs = ov.get('blade_scale_by_frame', {})
            check(f'override {action}: the knife is hidden (scale 0.01 or less) from frame 2 to 31, full size at frame 0 and from frame 35',
                  bool(bs) and bs[0] >= 0.999 and all(bs[f] <= 0.01 for f in range(2, 32)) and all(bs[f] >= 0.999 for f in range(35, frames + 1)), str(bs))
    d['pass'] = not fails
    d['failures'] = fails
    with open(path, 'w') as fh:
        json.dump(d, fh, indent=1)
    return fails


def validate_foe(key):
    """validate() for one foe, then foe_gate(): every predicate enforced, exit 1 on any failure."""
    validate(key)
    fails = foe_gate(key, os.path.join(MET, f'validate_{key}.json'))
    if fails:
        print('VALIDATION FAILED', key, '; '.join(fails))
        sys.stdout.flush()
        sys.exit(1)
    print('VALIDATION PASSED', key)


# ------------------------------------------------------------------ sheets
SHOWN = {k: OVERRIDE_FOE[k][1] for k in OVERRIDE_FOE}          # the clip each class plays instead of the shared one
SHARED_FOE = {OVERRIDE_FOE[k][0]: k for k in OVERRIDE_FOE}


def _tmp():
    return tempfile.mkdtemp(prefix='cast150_', dir=os.path.join(R, '.local', 'scratch'))


def sheetf_bind():
    """The raider (the rigged foe already in the game) and the five foes side by side, bind pose over Idle frame 0, same scale."""
    tmp = _tmp()
    fresh()
    setup_render(2700, 900)
    objs = {}
    keys = ('raider',) + FOES
    for i, key in enumerate(keys):
        ao, ms = load(glb(key))
        ao.location.y = (i - 2.5) * 1.25
        for m in ms:
            unlit(m)
        objs[key] = (ao, ms)
    bpy.context.view_layer.update()
    cam = ortho_cam(7.2, (0, 0, 1.0), (1, 0, 0))
    cam.location = Vector((0, 0, 1.0)) + VIEW * 8
    cam.rotation_euler = (-VIEW).to_track_quat('-Z', 'Y').to_euler()
    for tag, action, label in (('bind', None, 'Bind pose: raider (rigged in #147), scout, archer, skirmisher, brute, warden (same scale)'),
                               ('idle', 'Idle', 'Idle frame 0 (the shortbow sits tilted: it is authored upright at the Ranged hold)')):
        for key, (ao, ms) in objs.items():
            set_pose(ao, action, 0)
        render(f'{tmp}/{tag}.png')
        magick(f'{tmp}/{tag}.png', '-gravity', 'northwest', '-fill', 'white', '-pointsize', '30', '-annotate', '+14+10', label, f'{tmp}/{tag}_l.png')
    magick(f'{tmp}/bind_l.png', f'{tmp}/idle_l.png', '-append', os.path.join(PREV, 'sheet_f01_foes_bind_idle.png'))


def sheetf_turn():
    """Each foe from the front, 3/4, side and back at Idle frame 0."""
    tmp = _tmp()
    rows = []
    for key in FOES:
        fresh()
        setup_render(360, 580)
        ao, ms = load(glb(key))
        for m in ms:
            unlit(m)
        set_pose(ao, 'Idle', 0)
        cam = ortho_cam(2.8, (0, 0, 1.08), (1, 0, 0))
        tiles = []
        for name, deg_ in (('front', 0), ('three-quarter', -40), ('side', -90), ('back', 180)):
            d = Vector((math.cos(math.radians(deg_)), math.sin(math.radians(deg_)), 0))
            cam.location = Vector((0, 0, 1.08)) + d * 8
            cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
            p = f'{tmp}/{key}_{name}.png'
            render(p)
            magick(p, '-gravity', 'southwest', '-fill', 'white', '-pointsize', '20', '-annotate', '+6+4', f'{key} {name}', p)
            tiles.append(p)
        row = f'{tmp}/row_{key}.png'
        magick(*tiles, '+append', row)
        rows.append(row)
    magick(*rows[:3], '-append', os.path.join(PREV, 'sheet_f02_foes_turnaround_a.png'))
    magick(*rows[3:], '-append', os.path.join(PREV, 'sheet_f03_foes_turnaround_b.png'))


def sheetf_clips():
    """All ten clips, three frames each, for each foe (sheets f04 to f08): three clips per row; the class's own clip stands for the shared one it replaces."""
    tmp = _tmp()
    for n, key in zip((4, 5, 6, 7, 8), FOES):
        fresh()
        setup_render(260, 400)
        ao, ms = load(glb(key))
        for m in ms:
            unlit(m)
        cam = ortho_cam(2.8, (0, 0, 1.05), (.75, -.66, 0))
        rows = []
        names = list(TILE_FRAMES2)
        for r in range(4):
            tiles = []
            for name in names[r * 3:(r + 1) * 3]:
                shown = OVERRIDE_FOE[key][1] if key in OVERRIDE_FOE and OVERRIDE_FOE[key][0] == name else name
                for f in TILE_FRAMES2[name]:
                    set_pose(ao, shown, f)
                    px = ao.matrix_world @ ao.pose.bones['pelvis'].head
                    cam.location = Vector((px.x, px.y, 1.05)) + VIEW * 8
                    p = f'{tmp}/{key}_{name}_{f}.png'
                    render(p)
                    magick(p, '-gravity', 'southwest', '-fill', 'white', '-pointsize', '18', '-annotate', '+6+4', f'{shown} f{f}', p)
                    tiles.append(p)
            row = f'{tmp}/row_{key}_{r}.png'
            magick(*tiles, '-background', '#14161c', '+append', '-gravity', 'west', '-extent', '2340x400', row)
            rows.append(row)
        magick(*rows, '-background', '#14161c', '-append', os.path.join(PREV, f'sheet_f{n:02d}_{key}_clips.png'))


def _stone_party(keys, label, out):
    tmp = _tmp()
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
        ao, ms = load(glb(key))
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
    p = f'{tmp}/party.png'
    render(p)
    magick(p, '-gravity', 'northwest', '-fill', 'white', '-pointsize', '28', '-annotate', '+12+8', label, p)
    return p


def sheetf_stone():
    """Foes on dark stone at the gate-1 camera, beside the heroes they must never be mistaken for: the agile foes beside the rogue and ranger, the heavy
    foes beside the barbarian and paladin, and the whole foe cast."""
    groups = (('a', ('scout', 'archer', 'skirmisher', 'raider')), ('b', ('brute', 'warden', 'barbarian', 'paladin')), ('c', ('scout', 'rogue', 'archer', 'ranger')),
              ('d', ('skirmisher', 'brute', 'warden', 'raider')))
    outs = [_stone_party(keys, ' / '.join(keys), None) for _, keys in groups]
    magick(*outs[:2], '-append', os.path.join(PREV, 'sheet_f09_foes_stone_readability_a.png'))
    magick(*outs[2:], '-append', os.path.join(PREV, 'sheet_f10_foes_stone_readability_b.png'))


def sheetf_override():
    """The archer's Ranged_Archer and the brute's Melee_Brute beside the shared clips they replace, from the three-quarter view and from the front."""
    tmp = _tmp()
    spec = (('archer', 'Ranged_Archer', 'Ranged', (0, 1, 2, 5, 14, 20, 23, 27, 32, 36), 11), ('brute', 'Melee_Brute', 'Melee', (0, 4, 7, 9, 11, 13, 16, 20, 24, 30), 12))
    for key, action, shared, frames, n in spec:
        fresh()
        setup_render(220, 360)
        ao, ms = load(glb(key))
        for m in ms:
            unlit(m)
        cam = ortho_cam(2.8, (0, 0, 1.05), (.75, -.66, 0))
        rows = []
        for label, name in ((action, action), (f'shared {shared}', shared)):
            for view, vec in (('3/4', VIEW), ('front', Vector((1, 0, 0)))):
                tiles = []
                for f in frames:
                    fr = min(f, 36 if shared == 'Ranged' else 30)
                    set_pose(ao, name, fr)
                    px = ao.matrix_world @ ao.pose.bones['pelvis'].head
                    cam.location = Vector((px.x, px.y, 1.05)) + vec * 8
                    cam.rotation_euler = (-vec).to_track_quat('-Z', 'Y').to_euler()
                    p = f'{tmp}/{key}_{name}_{view[0]}_{f}.png'
                    render(p)
                    magick(p, '-gravity', 'southwest', '-fill', 'white', '-pointsize', '16', '-annotate', '+4+3', f'{label} {view} f{f}', p)
                    tiles.append(p)
                row = f'{tmp}/row_{key}_{name}_{view[0]}.png'
                magick(*tiles, '+append', row)
                rows.append(row)
        magick(*rows, '-background', '#14161c', '-append', os.path.join(PREV, f'sheet_f{n}_{key}_override_clip.png'))


mode = _args[0]
if mode == 'validate':
    validate_foe(_args[1])
elif mode == 'sheetf':
    {'bind': sheetf_bind, 'turn': sheetf_turn, 'clips': sheetf_clips, 'stone': sheetf_stone, 'override': sheetf_override}[_args[1]]()
