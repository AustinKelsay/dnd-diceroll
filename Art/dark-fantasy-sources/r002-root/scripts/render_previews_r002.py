"""r002 previews: the world at the gate-1 cameras with scaled hero figures and kit pieces, plus kit sheets.

  blender -b --python Art/dark-fantasy-sources/r002/scripts/render_previews_r002.py [-- shot ...]

Shots (1600x900 EEVEE, FOV 40 horizontal as in Unreal, camera south of the board looking north):
  default_p45   whole 18 x 12 board, pitch 45
  close_p35     party framing, pitch 35
  far_p60       whole board plus surroundings, pitch 60
  tall_p40      low wide shot that shows the architecture and the occluder pieces (tinted orange)
  occ_highlight same wide view with the occluder mesh tinted orange
  kit_obstacles obstacle kit (3 pillars, 3 walls, 3 stones, 2-cell arch) next to a 180 cm figure, 1.5 m cell squares
  kit_scatter   scatter set and fx cards
  paving_close  low close crop of the paving for stone size / joint reading
Lighting is a game-like stand-in: baked COLOR_0 x texture as ambient, plus one shadowed sun and a cool fill.  Glow mask (COLOR_0.a)
brightens flames, runes and glass.  Mist and shaft cards are blended using COLOR_0.a as opacity.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import r002_lib as R
from r002_lib import L, bpy, np, Vector

import render_lib as RL

ROOT = R.ROOT
PREV = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r002', 'previews')
CELL = 1.5
W, H = 18, 12
HERO_SCALE = 1.80 / 1.15
GLB = lambda a, rev, n: os.path.join(ROOT, 'Art', a, f'r{rev:03d}', n + '.glb')


def cell_xy(i, j):
    return ((i - (W - 1) / 2) * CELL, (j - (H - 1) / 2) * CELL)


def game_like(ob, ambient=.5, glow_gain=2.2, kind='solid'):
    """texture x COLOR_0 as base colour + ambient emission, so a real sun still shades and shadows it."""
    for slot in ob.material_slots:
        m = slot.material
        nt = m.node_tree
        tex = next(n for n in nt.nodes if n.type == 'TEX_IMAGE')
        out = next(n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL')
        bsdf = nt.nodes.get('Principled BSDF')
        ca = nt.nodes.new('ShaderNodeVertexColor')
        ca.layer_name = ob.data.color_attributes[0].name
        mul = nt.nodes.new('ShaderNodeMix')
        mul.data_type = 'RGBA'
        mul.blend_type = 'MULTIPLY'
        mul.inputs['Factor'].default_value = 1.0
        nt.links.new(tex.outputs['Color'], mul.inputs[6])
        nt.links.new(ca.outputs['Color'], mul.inputs[7])
        if kind == 'solid':
            nt.links.new(mul.outputs[2], bsdf.inputs['Base Color'])
            bsdf.inputs['Roughness'].default_value = 1.0
            bsdf.inputs['Specular IOR Level'].default_value = 0.0
            em = nt.nodes.new('ShaderNodeEmission')
            nt.links.new(mul.outputs[2], em.inputs['Color'])
            strength = nt.nodes.new('ShaderNodeMath')
            strength.operation = 'MULTIPLY_ADD'
            strength.inputs[1].default_value = glow_gain
            strength.inputs[2].default_value = ambient
            nt.links.new(ca.outputs['Alpha'], strength.inputs[0])
            nt.links.new(strength.outputs[0], em.inputs['Strength'])
            add = nt.nodes.new('ShaderNodeAddShader')
            nt.links.new(bsdf.outputs[0], add.inputs[0])
            nt.links.new(em.outputs[0], add.inputs[1])
            nt.links.new(add.outputs[0], out.inputs['Surface'])
            m.use_backface_culling = True
        else:                                                    # blended card: COLOR_0.a is opacity, texture red modulates it
            tr = nt.nodes.new('ShaderNodeBsdfTransparent')
            em = nt.nodes.new('ShaderNodeEmission')
            nt.links.new(mul.outputs[2], em.inputs['Color'])
            em.inputs['Strength'].default_value = 1.4 if kind == 'shaft' else 1.0
            fac = nt.nodes.new('ShaderNodeMath')
            fac.operation = 'MULTIPLY'
            nt.links.new(ca.outputs['Alpha'], fac.inputs[0])
            nt.links.new(tex.outputs['Color'], fac.inputs[1]) if kind == 'mist' else None
            if kind != 'mist':
                fac.inputs[1].default_value = 1.0
            mix = nt.nodes.new('ShaderNodeMixShader')
            nt.links.new(fac.outputs[0], mix.inputs['Fac'])
            nt.links.new(tr.outputs[0], mix.inputs[1])
            nt.links.new(em.outputs[0], mix.inputs[2])
            nt.links.new(mix.outputs[0], out.inputs['Surface'])
            m.surface_render_method = 'BLENDED'
            m.use_backface_culling = False


def tint_object(ob, rgb, k=.6):
    for slot in ob.material_slots:
        nt = slot.material.node_tree
        bsdf = nt.nodes.get('Principled BSDF')
        src = bsdf.inputs['Base Color'].links[0].from_socket
        mix = nt.nodes.new('ShaderNodeMix')
        mix.data_type = 'RGBA'
        mix.inputs['Factor'].default_value = k
        mix.inputs[7].default_value = (*rgb, 1)
        nt.links.new(src, mix.inputs[6])
        nt.links.new(mix.outputs[2], bsdf.inputs['Base Color'])


def place(path, x, y, yaw=0, scale=1.0, z=0.0, kind='solid', lit_figure=False):
    before = set(bpy.data.objects)
    obs = RL.import_glb(path)
    root = bpy.data.objects.new('p_' + os.path.basename(path), None)
    bpy.context.scene.collection.objects.link(root)
    for o in set(bpy.data.objects) - before:
        if o.parent is None and o is not root:
            o.parent = root
    root.location = (x, y, z)
    root.rotation_euler = (0, 0, math.radians(yaw))
    root.scale = (scale,) * 3
    for o in obs:
        if lit_figure:
            for slot in o.material_slots:
                b = slot.material.node_tree.nodes.get('Principled BSDF')
                if b:
                    b.inputs['Roughness'].default_value = 1.0
                    b.inputs['Specular IOR Level'].default_value = 0.0
        else:
            game_like(o, kind=kind)
    return root, obs


def place_camera(fov, pitch, target, fit_pts=None, dist=None, yaw=-90, margin=.94):
    sc = bpy.context.scene
    for o in [o for o in bpy.data.objects if o.type == 'CAMERA']:
        bpy.data.objects.remove(o)
    cd = bpy.data.cameras.new('cam')
    cd.sensor_fit = 'HORIZONTAL'
    cd.angle = math.radians(fov)
    cd.clip_start, cd.clip_end = .1, 600
    cam = bpy.data.objects.new('cam', cd)
    sc.collection.objects.link(cam)
    sc.camera = cam
    p, y = math.radians(pitch), math.radians(yaw)
    d = Vector((math.cos(y) * math.cos(p), math.sin(y) * math.cos(p), math.sin(p)))
    tgt = Vector(target)

    def setd(D):
        cam.location = tgt + d * D
        cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
        bpy.context.view_layer.update()

    if dist is None:
        from bpy_extras.object_utils import world_to_camera_view
        lo, hi = 2.0, 300.0
        for _ in range(40):
            mid = (lo + hi) / 2
            setd(mid)
            pr = [world_to_camera_view(sc, cam, Vector(q)) for q in fit_pts]
            ok = all(abs(v.x - .5) <= margin / 2 and abs(v.y - .5) <= margin / 2 and v.z > 0 for v in pr)
            lo, hi = (lo, mid) if ok else (mid, hi)
        dist = hi
    setd(dist)
    return cam, dist


def render(name):
    sc = bpy.context.scene
    sc.render.filepath = os.path.join(PREV, name + '.png')
    sc.render.image_settings.file_format = 'PNG'
    sc.render.image_settings.color_mode = 'RGB'
    bpy.ops.render.render(write_still=True)
    print('RENDERED', sc.render.filepath)


def sun(direction=(-.38, .30, -.85), strength=2.6, fill=.5):
    for name, d, e, col in (('sun', direction, strength, (1, .93, .82)), ('fill', (.6, -.7, -.3), fill, (.6, .7, 1.0))):
        ld = bpy.data.lights.new(name, 'SUN')
        ld.energy = e
        ld.color = col
        ld.angle = math.radians(2.5)
        o = bpy.data.objects.new(name, ld)
        o.rotation_euler = Vector(d).to_track_quat('-Z', 'Y').to_euler()
        bpy.context.scene.collection.objects.link(o)


# ------------------------------------------------------------------ the board scene
BLOCKED = [  # (asset, rev, name, cell, yaw)
    ('prop-pillar', 2, 'SM_Prop_Pillar', (3, 8), 0), ('prop-pillar-b', 1, 'SM_Prop_Pillar_B', (14, 9), 0), ('prop-pillar-c', 1, 'SM_Prop_Pillar_C', (9, 2), 0),
    ('prop-wall', 2, 'SM_Prop_Wall', (6, 9), 0), ('prop-wall-b', 1, 'SM_Prop_Wall_B', (11, 8), 0), ('prop-wall-c', 1, 'SM_Prop_Wall_C', (2, 4), 0),
    ('prop-stones', 2, 'SM_Prop_Stones', (15, 4), 0), ('prop-stones-b', 1, 'SM_Prop_Stones_B', (8, 5), 0), ('prop-stones-c', 1, 'SM_Prop_Stones_C', (4, 2), 0),
]
SCATTER = [('scatter-rubble', 'SM_Scatter_Rubble', (7, 7), 20), ('scatter-candles', 'SM_Scatter_Candles', (5, 3), 0), ('scatter-bones', 'SM_Scatter_Bones', (12, 4), 30),
           ('scatter-roots', 'SM_Scatter_Roots', (2, 7), 0), ('scatter-weapons', 'SM_Scatter_Weapons', (9, 6), 40), ('scatter-puddle', 'SM_Scatter_Puddle', (10, 3), 0),
           ('scatter-urn', 'SM_Scatter_Urn', (13, 8), 0), ('scatter-moss', 'SM_Scatter_Moss', (6, 1), 0)]
HEROES = [('hero-cleric', 'SM_Hero_Cleric', (6, 6), -20), ('hero-wizard', 'SM_Hero_Wizard', (6, 4), 15), ('hero-rogue', 'SM_Hero_Rogue', (5, 5), 0),
          ('hero-fighter', 'SM_Hero_Fighter', (7, 5), 0), ('foe-raider', 'SM_Foe_Raider', (11, 5), 180), ('foe-raider', 'SM_Foe_Raider', (12, 7), 200),
          ('foe-archer', 'SM_Foe_Archer', (13, 3), 160)]


def build_board_scene():
    sc = RL.setup_scene((1600, 900), bg=(.02, .02, .03))
    sc.render.fps = 30
    try:
        sc.eevee.taa_render_samples = 32
        sc.eevee.use_shadows = True
    except Exception:
        pass
    bpy.context.preferences.system.anisotropic_filter = 'FILTER_16'
    world = RL.import_glb(GLB('world-vigil', 2, 'SM_World_Vigil'))
    occ = RL.import_glb(GLB('world-vigil-occluders', 1, 'SM_World_Vigil_Occluders'))
    for o in world + occ:
        game_like(o, ambient=.55)
    sun()
    for a, rev, n, cell, yaw in BLOCKED:
        place(GLB(a, rev, n), *cell_xy(*cell), yaw=yaw)
    # 2-cell arches: Y-adjacent pair (9,10)-(9,11) and an X-adjacent pair (3,0)-(4,0) rotated 90 degrees
    x, y = cell_xy(9, 10)
    place(GLB('prop-arch-2cell', 1, 'SM_Prop_Arch2Cell'), x, y + CELL / 2, 0)
    x, y = cell_xy(3, 0)
    place(GLB('prop-arch-2cell', 1, 'SM_Prop_Arch2Cell'), x + CELL / 2, y, 90)
    for a, n, cell, yaw in SCATTER:
        place(GLB(a, 1, n), *cell_xy(*cell), yaw=yaw)
    place(GLB('fx-mist-card-c', 1, 'SM_Fx_MistCard_C'), *cell_xy(12, 6), kind='mist')
    place(GLB('fx-mist-card', 1, 'SM_Fx_MistCard'), *cell_xy(1, 9), kind='mist')
    place(GLB('fx-light-shaft', 1, 'SM_Fx_LightShaft'), *cell_xy(8, 9), kind='shaft')
    for a, n, cell, yaw in HEROES:
        place(os.path.join(ROOT, 'Art', a, 'r001', n + '.glb'), *cell_xy(*cell), yaw=yaw, scale=HERO_SCALE, lit_figure=True)
    return world, occ


def board_shots(only):
    world, occ = build_board_scene()
    hx, hy = W * CELL / 2, H * CELL / 2
    board = [(-hx, -hy, 0), (hx, -hy, 0), (hx, hy, 0), (-hx, hy, 0)]
    fx, fy = cell_xy(6, 5)
    party = [cell_xy(i, j) + (0,) for i, j in ((3, 2), (11, 2), (11, 9), (3, 9))] + [(fx, fy, 1.9)]
    shots = {
        'default_p45': lambda: place_camera(40, 45, (0, 0, 0), board),
        'close_p35': lambda: place_camera(40, 35, (fx + 2.2, fy, .9), [cell_xy(i, j) + (0,) for i, j in ((4, 3), (13, 3), (13, 8), (4, 8))] + [cell_xy(5, 5) + (1.9,)], margin=.9),
        'far_p60': lambda: place_camera(40, 60, (0, 0, 0), [(p[0] * 1.35, p[1] * 1.35, 0) for p in board]),
        'wide_p26': lambda: place_camera(40, 26, (0, 2, 4), dist=70, yaw=-62),
        'occ_highlight': lambda: place_camera(40, 26, (0, 2, 4), dist=70, yaw=-62),
        'paving_close': lambda: place_camera(40, 35, (fx - 2.5, fy - 2.2, 0), dist=7.5, yaw=-80),
    }
    for nm in only or list(shots):
        if nm == 'occ_highlight':
            for o in occ:
                tint_object(o, (1.0, .45, .1))
        shots[nm]()
        render(nm)


def kit_row(name, items, fig=True, pitch=17, spacing=2.3):
    """One row of kit pieces seen from +X (their front), a 180 cm figure at the left, 1.5 m cell outlines and labels."""
    sc = RL.setup_scene((1800, 560), bg=(.03, .03, .04))
    try:
        sc.eevee.taa_render_samples = 32
    except Exception:
        pass
    sun(direction=(-.55, .35, -.75), strength=2.8)
    n = len(items)
    me = bpy.data.meshes.new('floor')
    me.from_pydata([(-6, -3, -.01), (6, -3, -.01), (6, spacing * (n + 1), -.01), (-6, spacing * (n + 1), -.01)], [], [(0, 1, 2, 3)])
    fo = bpy.data.objects.new('floor', me)
    sc.collection.objects.link(fo)
    mt = bpy.data.materials.new('floor')
    mt.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (.07, .07, .08, 1)
    mt.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 1
    me.materials.append(mt)
    allobs = []
    for k, (path, label, kind, yaw) in enumerate(items):
        x, y = 0.0, (k + 1) * spacing
        root, obs = place(path, x, y, yaw=yaw, kind=kind)
        allobs += obs
        h = CELL / 2
        me2 = bpy.data.meshes.new('cell')
        me2.from_pydata([(x - h, y - h, .004), (x + h, y - h, .004), (x + h, y + h, .004), (x - h, y + h, .004)], [(0, 1), (1, 2), (2, 3), (3, 0)], [])
        sc.collection.objects.link(bpy.data.objects.new('cell', me2))
        cu = bpy.data.curves.new('t', 'FONT')
        cu.body = label
        cu.size = .2
        cu.align_x = 'CENTER'
        t = bpy.data.objects.new('t', cu)
        t.location = (x + 1.0, y, .02)
        t.rotation_euler = (0, 0, math.pi / 2)
        sc.collection.objects.link(t)
        m = bpy.data.materials.new('tm')
        b = m.node_tree.nodes['Principled BSDF']
        b.inputs['Base Color'].default_value = (1, .86, .55, 1)
        b.inputs['Emission Color'].default_value = (1, .86, .55, 1)
        b.inputs['Emission Strength'].default_value = 1.0
        cu.materials.append(m)
    if fig:
        root, obs = place(os.path.join(ROOT, 'Art', 'hero-fighter', 'r001', 'SM_Hero_Fighter.glb'), 0.0, 0.0, yaw=0, scale=HERO_SCALE, lit_figure=True)
        allobs += obs
    bpy.context.view_layer.update()
    lo, hi = RL.bounds(allobs)
    pts = [(x, y, z) for x in (lo.x, hi.x + 1.2) for y in (lo.y - 1.0, hi.y + 1.0) for z in (lo.z, hi.z)]
    place_camera(40, pitch, ((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, (lo.z + hi.z) / 2), pts, yaw=0, margin=.97)
    render('_row_' + name)


def stack_rows(out, rows):
    import subprocess
    files = [os.path.join(PREV, '_row_' + r + '.png') for r in rows]
    subprocess.run(['magick', *files, '-append', os.path.join(PREV, out + '.png')], check=True)
    for f in files:
        os.remove(f)
    print('SHEET', out)


def main():
    only = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    os.makedirs(PREV, exist_ok=True)
    kits = [s for s in only if s.startswith('kit_')]
    boards = [s for s in only if not s.startswith('kit_')]
    if not only or boards:
        board_shots(boards)
    ob = lambda a, rev, n, label, yaw=0, kind='solid': (GLB(a, rev, n), label, kind, yaw)
    if not only or 'kit_obstacles' in kits:
        kit_row('o1', [ob('prop-pillar', 2, 'SM_Prop_Pillar', 'pillar A 2.95 m'), ob('prop-pillar-b', 1, 'SM_Prop_Pillar_B', 'pillar B 3.40 m'), ob('prop-pillar-c', 1, 'SM_Prop_Pillar_C', 'pillar C 1.75 m')])
        kit_row('o2', [ob('prop-wall', 2, 'SM_Prop_Wall', 'wall A 2.13 m'), ob('prop-wall-b', 1, 'SM_Prop_Wall_B', 'wall B 2.35 m'), ob('prop-wall-c', 1, 'SM_Prop_Wall_C', 'wall C 1.05 m')])
        kit_row('o3', [ob('prop-stones', 2, 'SM_Prop_Stones', 'stones A 1.26 m'), ob('prop-stones-b', 1, 'SM_Prop_Stones_B', 'stones B 2.06 m'), ob('prop-stones-c', 1, 'SM_Prop_Stones_C', 'stones C 0.74 m'),
                           ob('prop-arch-2cell', 1, 'SM_Prop_Arch2Cell', '2-cell arch 3.0 x 3.4 m')], spacing=2.5)
        stack_rows('kit_obstacles', ['o1', 'o2', 'o3'])
    if not only or 'kit_scatter' in kits:
        sc_items = [ob(a, 1, n, n[3:].replace('_', ' ').lower()) for a, n in [(s_[0], s_[1]) for s_ in SCATTER]]
        kit_row('s1', sc_items[:4], fig=True, pitch=30, spacing=1.7)
        kit_row('s2', sc_items[4:], fig=False, pitch=30, spacing=1.7)
        kit_row('s3', [ob('fx-mist-card', 1, 'SM_Fx_MistCard', 'mist card A', 0, 'mist'), ob('fx-mist-card-b', 1, 'SM_Fx_MistCard_B', 'mist card B', 0, 'mist'), ob('fx-mist-card-c', 1, 'SM_Fx_MistCard_C', 'mist card C', 0, 'mist'),
                       ob('fx-light-shaft', 1, 'SM_Fx_LightShaft', 'light shaft', 0, 'shaft'), ob('fx-light-shaft-b', 1, 'SM_Fx_LightShaft_B', 'light shaft B', 0, 'shaft')], fig=True, pitch=22, spacing=3.6)
        stack_rows('kit_scatter', ['s1', 's2', 's3'])
    print('PREVIEWS done')


main()
