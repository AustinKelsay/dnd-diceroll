"""#140 spike: camera / grid / filtering previews -> Art/spike-140/previews/*.png (1920x1080, EEVEE)

  blender -b --python Art/spike-140/scripts/render_previews.py [-- shot1 shot2 ...]

Shots (all keep the camera looking at the board from the south, +Y is "north"):
  A_fov50_pitch58_board      today's camera: FOV 50, ~58 deg pitch, whole board in frame
  B_fov40_pitch45_board      FOV 40, 45 deg, whole board
  C_fov40_pitch35_party      FOV 40, 35 deg, closer to the party
  grid_off / grid_on         same view (FOV 40, 45 deg, party framing): no grid vs mock contextual grid
  filter_closest / _linear   same close shot, texture sampling Closest vs Linear (16x anisotropic requested)
  occluders_highlight        wide view with the exported occluder meshes tinted orange
World and baked light are shown unlit (texture x COLOR_0, glow mask x1.3); figures are lit by a warm key + cool fill.
Occluder pieces are rendered as they are exported; Unreal would dither-fade the ones in front of the camera.
"""
import math
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
EXP = os.path.join(ROOT, 'Art', 'spike-140', 'exports')
PREV = os.path.join(ROOT, 'Art', 'spike-140', 'previews')
os.environ.setdefault('THEME_LOCAL', tempfile.mkdtemp(prefix='spike140_'))
sys.path.insert(0, os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r001', 'scripts'))

import bpy
import numpy as np
from mathutils import Euler, Vector

import render_lib as R

CELL = 1.5
W, H = 18, 12
FIGHTER_CELL = (7, 5)
# (file, cell, yaw degrees, scale) ; yaw 0 = facing +X (east), r001 heroes are 1.15 m -> 1.80 m
HERO_SCALE = 1.80 / 1.15
STANDINS = [('Art/hero-cleric/r001/SM_Hero_Cleric.glb', (6, 6), -20), ('Art/hero-wizard/r001/SM_Hero_Wizard.glb', (6, 4), 15),
            ('Art/hero-rogue/r001/SM_Hero_Rogue.glb', (5, 5), 0), ('Art/foe-raider/r001/SM_Foe_Raider.glb', (11, 5), 180),
            ('Art/foe-raider/r001/SM_Foe_Raider.glb', (12, 7), 200), ('Art/foe-archer/r001/SM_Foe_Archer.glb', (13, 3), 160)]


def cell_xy(i, j):
    return ((i - (W - 1) / 2) * CELL, (j - (H - 1) / 2) * CELL)


def mat_emission(name, rgba, strength=1.0, alpha=1.0, blend=False):
    m = bpy.data.materials.new(name)
    nt = m.node_tree
    for n in list(nt.nodes):
        if n.type != 'OUTPUT_MATERIAL':
            nt.nodes.remove(n)
    out = nt.nodes['Material Output']
    em = nt.nodes.new('ShaderNodeEmission')
    em.inputs['Color'].default_value = rgba
    em.inputs['Strength'].default_value = strength
    if blend:
        tr = nt.nodes.new('ShaderNodeBsdfTransparent')
        mix = nt.nodes.new('ShaderNodeMixShader')
        mix.inputs['Fac'].default_value = alpha
        nt.links.new(tr.outputs[0], mix.inputs[1])
        nt.links.new(em.outputs[0], mix.inputs[2])
        nt.links.new(mix.outputs[0], out.inputs['Surface'])
        m.surface_render_method = 'BLENDED'
        m.use_backface_culling = False
    else:
        nt.links.new(em.outputs[0], out.inputs['Surface'])
    return m


def world_unlit(ob, glow_gain=1.3):
    """texture x COLOR_0.rgb, brightened where COLOR_0.a (glow mask) is 1."""
    for slot in ob.material_slots:
        nt = slot.material.node_tree
        tex = next(n for n in nt.nodes if n.type == 'TEX_IMAGE')
        out = next(n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL')
        ca = nt.nodes.new('ShaderNodeVertexColor')
        ca.layer_name = ob.data.color_attributes[0].name
        mul = nt.nodes.new('ShaderNodeMix')
        mul.data_type = 'RGBA'
        mul.blend_type = 'MULTIPLY'
        mul.inputs['Factor'].default_value = 1.0
        nt.links.new(tex.outputs['Color'], mul.inputs[6])
        nt.links.new(ca.outputs['Color'], mul.inputs[7])
        gain = nt.nodes.new('ShaderNodeMath')
        gain.operation = 'MULTIPLY_ADD'
        gain.inputs[1].default_value = glow_gain - 1
        gain.inputs[2].default_value = 1.0
        nt.links.new(ca.outputs['Alpha'], gain.inputs[0])
        sc = nt.nodes.new('ShaderNodeMix')
        sc.data_type = 'RGBA'
        sc.blend_type = 'MULTIPLY'
        sc.inputs['Factor'].default_value = 1.0
        nt.links.new(mul.outputs[2], sc.inputs[6])
        nt.links.new(gain.outputs[0], sc.inputs[7])
        em = nt.nodes.new('ShaderNodeEmission')
        nt.links.new(sc.outputs[2], em.inputs['Color'])
        nt.links.new(em.outputs[0], out.inputs['Surface'])
        slot.material.use_backface_culling = True


def tint_object(ob, rgb):
    for slot in ob.material_slots:
        nt = slot.material.node_tree
        em = next(n for n in nt.nodes if n.type == 'EMISSION')
        src = em.inputs['Color'].links[0].from_socket
        mul = nt.nodes.new('ShaderNodeMix')
        mul.data_type = 'RGBA'
        mul.blend_type = 'MIX'
        mul.inputs['Factor'].default_value = .6
        mul.inputs[7].default_value = (*rgb, 1)
        nt.links.new(src, mul.inputs[6])
        nt.links.new(mul.outputs[2], em.inputs['Color'])


def set_interpolation(kind):
    for m in bpy.data.materials:
        if m.node_tree:
            for n in m.node_tree.nodes:
                if n.type == 'TEX_IMAGE':
                    n.interpolation = kind


def lit_figure(obs):
    """Keep the Principled material (texture) - the key/fill lights below shade it."""
    for o in obs:
        for slot in o.material_slots:
            nt = slot.material.node_tree
            b = nt.nodes.get('Principled BSDF')
            if b:
                b.inputs['Roughness'].default_value = 1.0
                b.inputs['Specular IOR Level'].default_value = 0.0


def import_figure(path, cell, yaw, scale=1.0):
    before = set(bpy.data.objects)
    obs = R.import_glb(path)
    root = bpy.data.objects.new('fig_' + os.path.basename(path), None)
    bpy.context.scene.collection.objects.link(root)
    for o in set(bpy.data.objects) - before:          # meshes, armatures and importer empties
        if o.parent is None and o is not root:
            o.parent = root
    x, y = cell_xy(*cell)
    root.location = (x, y, 0.0)
    root.rotation_euler = (0, 0, math.radians(yaw))
    root.scale = (scale, scale, scale)
    lit_figure(obs)
    return root, obs


def blob_shadow(cell, r=.55):
    me = bpy.data.meshes.new('blob')
    x, y = cell_xy(*cell)
    seg = 24
    verts = [(x, y, .05)] + [(x + r * math.cos(a), y + r * .85 * math.sin(a), .05) for a in np.linspace(0, 2 * math.pi, seg, endpoint=False)]
    faces = [(0, 1 + i, 1 + (i + 1) % seg) for i in range(seg)]
    me.from_pydata(verts, [], faces)
    o = bpy.data.objects.new('blob', me)
    bpy.context.scene.collection.objects.link(o)
    o.data.materials.append(mat_emission('blobm', (0, 0, 0, 1), 1.0, .42, blend=True))
    return o


def flat_mesh(name, quads, z, mat):
    me = bpy.data.meshes.new(name)
    verts, faces = [], []
    for q in quads:
        b = len(verts)
        verts += [(p[0], p[1], z) for p in q]
        faces.append((b, b + 1, b + 2, b + 3))
    me.from_pydata(verts, [], faces)
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    o.data.materials.append(mat)
    return o


def seg_quad(a, b, w):
    a, b = np.array(a, float), np.array(b, float)
    d = (b - a) / np.linalg.norm(b - a)
    n = np.array((-d[1], d[0])) * w / 2
    e = d * w / 2
    return [a - e - n, b + e - n, b + e + n, a - e + n]


def add_contextual_grid():
    """Mock: faint cell edges within a 4-cell Manhattan reach, a slightly stronger contour, path ribbon, footprint ring."""
    fi, fj = FIGHTER_CELL
    reach = 4
    inside = lambda i, j: 0 <= i < W and 0 <= j < H and abs(i - fi) + abs(j - fj) <= reach
    grid_edges, contour = set(), set()
    for i in range(W):
        for j in range(H):
            if not inside(i, j):
                continue
            x0, y0 = (i - W / 2) * CELL, (j - H / 2) * CELL
            edges = {('h', i, j): ((x0, y0), (x0 + CELL, y0)), ('h', i, j + 1): ((x0, y0 + CELL), (x0 + CELL, y0 + CELL)),
                     ('v', i, j): ((x0, y0), (x0, y0 + CELL)), ('v', i + 1, j): ((x0 + CELL, y0), (x0 + CELL, y0 + CELL))}
            nb = {('h', i, j): (i, j - 1), ('h', i, j + 1): (i, j + 1), ('v', i, j): (i - 1, j), ('v', i + 1, j): (i + 1, j)}
            for k, seg in edges.items():
                grid_edges.add((k, seg))
                if not inside(*nb[k]):
                    contour.add((k, seg))
    faint = mat_emission('grid_faint', (1.0, .86, .62, 1), 1.0, .18, blend=True)
    strong = mat_emission('grid_contour', (1.0, .86, .62, 1), 1.0, .5, blend=True)
    flat_mesh('grid_faint', [seg_quad(a, b, .035) for _, (a, b) in grid_edges], .05, faint)
    flat_mesh('grid_contour', [seg_quad(a, b, .07) for _, (a, b) in contour], .055, strong)
    # path ribbon: cardinal path 3 east, 2 north from the fighter cell, with square corners
    path = [(fi, fj), (fi + 1, fj), (fi + 2, fj), (fi + 3, fj), (fi + 3, fj + 1), (fi + 3, fj + 2)]
    pts = [cell_xy(*c) for c in path]
    quads = [seg_quad(a, b, .20) for a, b in zip(pts, pts[1:])]
    flat_mesh('path', quads, .06, mat_emission('path', (1.0, .78, .30, 1), 1.0, .62, blend=True))
    # destination bracket: four small corner ticks on the last cell
    cx, cy = pts[-1]
    h = CELL / 2 - .05
    t = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            t.append(seg_quad((cx + sx * h, cy + sy * h), (cx + sx * (h - .3), cy + sy * h), .06))
            t.append(seg_quad((cx + sx * h, cy + sy * h), (cx + sx * h, cy + sy * (h - .3)), .06))
    flat_mesh('bracket', t, .062, mat_emission('bracket', (1.0, .9, .6, 1), 1.0, .85, blend=True))
    # 56 cm hollow footprint ring
    x, y = cell_xy(fi, fj)
    seg = 40
    ring = []
    for k in range(seg):
        a0, a1 = 2 * math.pi * k / seg, 2 * math.pi * (k + 1) / seg
        r0, r1 = .25, .28
        ring.append([(x + r0 * math.cos(a0), y + r0 * math.sin(a0)), (x + r1 * math.cos(a0), y + r1 * math.sin(a0)),
                     (x + r1 * math.cos(a1), y + r1 * math.sin(a1)), (x + r0 * math.cos(a1), y + r0 * math.sin(a1))])
    flat_mesh('ring', ring, .065, mat_emission('ring', (.55, .95, 1.0, 1), 1.0, .85, blend=True))


def project(cam, pts):
    sc = bpy.context.scene
    from bpy_extras.object_utils import world_to_camera_view
    return [world_to_camera_view(sc, cam, Vector(p)) for p in pts]


def place_camera(fov, pitch, target, fit_pts=None, dist=None, yaw=-90, margin=.94):
    """Camera looking at target from yaw (-90 = from the south) at pitch; distance fits fit_pts unless dist is given."""
    sc = bpy.context.scene
    for o in [o for o in bpy.data.objects if o.type == 'CAMERA']:
        bpy.data.objects.remove(o)
    cd = bpy.data.cameras.new('cam')
    cd.sensor_fit = 'HORIZONTAL'
    cd.angle = math.radians(fov)
    cd.clip_start, cd.clip_end = .1, 400
    cam = bpy.data.objects.new('cam', cd)
    sc.collection.objects.link(cam)
    sc.camera = cam
    d = Vector((math.cos(math.radians(yaw)) * math.cos(math.radians(pitch)), math.sin(math.radians(yaw)) * math.cos(math.radians(pitch)), math.sin(math.radians(pitch))))
    tgt = Vector(target)

    def setd(D):
        cam.location = tgt + d * D
        cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
        bpy.context.view_layer.update()

    if dist is None:
        lo, hi = 2.0, 200.0
        for _ in range(40):
            mid = (lo + hi) / 2
            setd(mid)
            p = project(cam, fit_pts)
            ok = all(abs(v.x - .5) <= margin / 2 and abs(v.y - .5) <= margin / 2 and v.z > 0 for v in p)
            lo, hi = (lo, mid) if ok else (mid, hi)
        dist = hi
    setd(dist)
    return cam, dist


def render(name):
    sc = bpy.context.scene
    path = os.path.join(PREV, name + '.png')
    sc.render.filepath = path
    sc.render.image_settings.file_format = 'PNG'
    sc.render.image_settings.color_mode = 'RGB'
    bpy.ops.render.render(write_still=True)
    print('RENDERED', path)


def main():
    only = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    os.makedirs(PREV, exist_ok=True)
    sc = R.setup_scene((1920, 1080), bg=(.02, .02, .03))
    sc.render.fps = 30
    try:
        sc.eevee.taa_render_samples = 32
        sc.view_settings.view_transform = 'Standard'
    except Exception:
        pass
    bpy.context.preferences.system.anisotropic_filter = 'FILTER_16'
    wobs = R.import_glb(os.path.join(EXP, 'SM_World_VigilSpike.glb'))
    oobs = R.import_glb(os.path.join(EXP, 'SM_World_VigilSpike_Occluders.glb'))
    for o in wobs + oobs:
        world_unlit(o)
    R.lights(sun_dir=(-.35, .3, -.85), sun=2.6, fill=.8)
    # figures
    fr, fobs = import_figure(os.path.join(EXP, 'SK_Hero_FighterSpike.glb'), FIGHTER_CELL, 0)
    ao = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    ao.animation_data_create()
    ao.animation_data.action = bpy.data.actions['Idle']
    for tr in ao.animation_data.nla_tracks:
        tr.mute = True
    sc.frame_set(12)
    for path, cell, yaw in STANDINS:
        import_figure(os.path.join(ROOT, path), cell, yaw, HERO_SCALE)
    for cell in [FIGHTER_CELL] + [c for _, c, _ in STANDINS]:
        blob_shadow(cell)
    fx, fy = cell_xy(*FIGHTER_CELL)
    hx, hy = W * CELL / 2, H * CELL / 2
    board = [(-hx, -hy, 0), (hx, -hy, 0), (hx, hy, 0), (-hx, hy, 0)]
    party = [cell_xy(i, j) + (0,) for i, j in ((4, 3), (14, 3), (14, 8), (4, 8))] + [cell_xy(i, j) + (1.8,) for i, j in ((5, 5), (13, 7))]
    shots = {}
    shots['A_fov50_pitch58_board'] = lambda: place_camera(50, 58, (0, 0, 0), board)
    shots['B_fov40_pitch45_board'] = lambda: place_camera(40, 45, (0, 0, 0), board)
    shots['C_fov40_pitch35_party'] = lambda: place_camera(40, 35, (fx + 2.2, fy, .9), party)
    medium = lambda: place_camera(40, 45, (fx + 1.5, fy - .2, .8), [cell_xy(i, j) + (0,) for i, j in ((3, 2), (11, 2), (11, 9), (3, 9))] + [(fx, fy, 1.9)], margin=.9)
    shots['grid_off'] = medium
    shots['grid_on'] = medium
    close = lambda: place_camera(40, 35, (fx, fy, .85), [(fx - 2.2, fy - 1.6, 0), (fx + 2.2, fy + 1.6, 0), (fx, fy, 2.0)], margin=.85)
    shots['filter_closest'] = close
    shots['filter_linear'] = close
    shots['occluders_highlight'] = lambda: place_camera(40, 26, (0, 2, 4), dist=70, yaw=-62)
    names = only or list(shots)
    grid_added = False
    for nm in names:
        if nm == 'grid_on' and not grid_added:
            add_contextual_grid()
            grid_added = True
        if nm != 'grid_on' and grid_added:
            for o in [o for o in bpy.data.objects if o.name.split('.')[0] in ('grid_faint', 'grid_contour', 'path', 'bracket', 'ring')]:
                o.hide_render = True
        if nm == 'occluders_highlight':
            for o in oobs:
                tint_object(o, (1.0, .45, .1))
        set_interpolation('Linear' if nm == 'filter_linear' else 'Closest')
        shots[nm]()
        render(nm)
    print('PREVIEWS done')


main()
