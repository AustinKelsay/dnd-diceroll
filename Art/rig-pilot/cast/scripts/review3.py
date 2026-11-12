"""Fresh-process review of the batch 3 exports (#150): validation numbers and contact sheets for the sorcerer, warlock and druid.

    blender -b --python Art/rig-pilot/cast/scripts/review3.py -- validate <sorcerer|warlock|druid>
    blender -b --python Art/rig-pilot/cast/scripts/review3.py -- sheet3 bind|turn|clips|stone

review.py and review2.py (batches 1 and 2) are not edited, so the asset records that hash them stay valid: this script loads review2.py
(and through it review.py and the r001 review functions) into its own namespace after registering the batch 3 classes, so validate expects
the ten clips. validate writes metrics/validate_<key>.json with an explicit `checks` table and `pass`; any failed check prints the failures
and EXITS NON-ZERO, so a failed bone, clip, LOD, seam, influence, height or glow check can never be recorded as a pass. Sheets need ImageMagick 7.
"""
import math
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cast_characters as C   # noqa: E402  (puts the r001 scripts on sys.path)
import cast_batch3            # noqa: E402,F401  registers sorcerer, warlock, druid (and, through cast_batch2, batch 2 and the Brace clip)

_namespace = globals()
_args = sys.argv[sys.argv.index('--') + 1:]
_REVIEW2 = os.path.join(HERE, 'review2.py')
exec(compile(open(_REVIEW2).read().split('\nmode = _args[0]')[0], _REVIEW2, 'exec'), _namespace)   # batch 2 functions, which exec the batch 1 and r001 ones

import json                                                         # noqa: E402
import bpy                                                          # noqa: E402,F811
import numpy as np                                                  # noqa: E402,F811
from mathutils import Vector                                        # noqa: E402,F811

B3KEYS = ('sorcerer', 'warlock', 'druid')
EXPECT_CLIPS = ['Idle', 'Walk', 'Melee', 'Ranged', 'Cast', 'Hit', 'Downed', 'Death', 'Recover', 'Brace']


def judge(out):
    """The explicit pass/fail table for one validation result (the contract numbers of #147/#150)."""
    seam, drift = out['seam_mm_first_vs_last_max_vertex'], out['root_drift_mm']
    lr = out['lod_ratio']
    atlas = [list(a) for a in out['atlas_px']]
    checks = {
        'bones_25_match_contract': out['bones'] == 25 and bool(out['bones_match_contract']),
        'ten_clips_with_expected_ranges': sorted(out['clips']) == sorted(EXPECT_CLIPS) and all(out['clips_match'].get(n) for n in EXPECT_CLIPS),
        'one_material_one_1024_atlas': len(out['materials']) == 1 and atlas == [[1024, 1024]],
        'glow_alpha_present': out['glow_alpha']['verts_alpha_gt_0'] > 0 and out['glow_alpha']['alpha_max'] >= .99,
        'at_most_4_influences': out['max_influences'] <= 4,
        'height_175_to_185_cm': 1.75 <= out['body_height_m_excluding_held_items'] <= 1.85,
        'tris_lod0_3k_to_6k': 3000 <= out['tris_lod0'] <= 6000,
        'lod1_about_half': .40 <= lr['LOD1'] <= .60,
        'lod2_about_quarter': .15 <= lr['LOD2'] <= .35,
        'lod_bones_25_same_names': all(out['lods'][k]['bones'] == 25 and out['lods'][k]['same_bone_names'] for k in ('LOD1', 'LOD2')),
        'idle_walk_loop_seams_0mm': seam['Idle'] < .01 and seam['Walk'] < .01,
        'action_clips_end_on_idle_0mm': all(seam[n] < .01 for n in ('Melee', 'Ranged', 'Cast', 'Hit', 'Brace')),
        'root_drift_0mm_every_clip': all(v < .01 for v in drift.values()),
    }
    return checks


def validate_checked(key):
    validate(key)
    p = os.path.join(MET, f'validate_{key}.json')
    out = json.load(open(p))
    checks = judge(out)
    out['checks'] = checks
    out['pass'] = bool(all(checks.values()))
    with open(p, 'w') as fh:
        json.dump(out, fh, indent=1)
    failed = [k for k, v in checks.items() if not v]
    print('VALIDATE_RESULT', key, 'PASS' if out['pass'] else 'FAIL ' + ', '.join(failed))
    if failed:
        sys.stderr.write(f'validate {key} FAILED: {failed}\n')
        sys.exit(1)


ALL_RIGS3 = ALL_RIGS + B3KEYS
TILE_FRAMES3 = TILE_FRAMES2


def rig_glb(key):
    return glb(key)


def sheet3_bind():
    """The wizard, ranger and cleric (the nearest looks) beside the three batch 3 classes, bind pose over Idle frame 0, same scale."""
    tmp = tempfile.mkdtemp(prefix='cast150_')
    fresh()
    setup_render(2400, 800)
    objs = {}
    keys = ('wizard', 'sorcerer', 'warlock', 'ranger', 'druid', 'cleric')
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
    for tag, action, label in (('bind', None, 'Bind pose: wizard, sorcerer, warlock, ranger, druid, cleric (same scale)'),
                               ('idle', 'Idle', 'Idle frame 0')):
        for key, (ao, ms) in objs.items():
            set_pose(ao, action, 0)
        render(f'{tmp}/{tag}.png')
        magick(f'{tmp}/{tag}.png', '-gravity', 'northwest', '-fill', 'white', '-pointsize', '30', '-annotate', '+14+10', label, f'{tmp}/{tag}_l.png')
    magick(f'{tmp}/bind_l.png', f'{tmp}/idle_l.png', '-append', os.path.join(PREV, 'sheet_15_batch3_bind_idle.png'))


def sheet3_turn():
    tmp = tempfile.mkdtemp(prefix='cast150_')
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
    magick(*rows, '-append', os.path.join(PREV, 'sheet_16_batch3_turnaround.png'))


def sheet3_clips():
    """All ten clips, three frames each, for each batch 3 class (sheets 17 to 19): three clips per row."""
    tmp = tempfile.mkdtemp(prefix='cast150_')
    for n, key in zip((17, 18, 19), B3KEYS):
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


def sheet3_stone():
    """Two parties on dark stone at the gate-1 camera: (wizard, sorcerer, warlock, druid) and (ranger, druid, rogue, warlock)."""
    groups = (('a', ('wizard', 'sorcerer', 'warlock', 'druid')), ('b', ('ranger', 'druid', 'rogue', 'warlock')))
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
    magick(*outs, '-append', os.path.join(PREV, 'sheet_20_batch3_stone_readability.png'))


mode = _args[0]
if mode == 'validate':
    validate_checked(_args[1])
elif mode == 'sheet3':
    {'bind': sheet3_bind, 'turn': sheet3_turn, 'clips': sheet3_clips, 'stone': sheet3_stone}[_args[1]]()
