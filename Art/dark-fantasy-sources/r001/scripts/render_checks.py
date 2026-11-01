"""Overhead occlusion/hole checks: game-like camera (55 deg pitch, 25 m) from the four sides,
1.10 m grid over the 18x12 footprint. blender -b --python render_checks.py -- vigil|root ..."""
import math, sys
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import bpy, numpy as np
from mathutils import Vector
import render_lib as R, tlib as L

CELL = 1.10
NX, NY = 18, 12


def grid_overlay():
    """Thin emissive lines at z=+0.02 over the footprint; outline in gold."""
    verts, faces, mats = [], [], []
    w = .025
    hx, hy = NX * CELL / 2, NY * CELL / 2
    def bar(x0, y0, x1, y1, m):
        i = len(verts)
        if abs(x1 - x0) > abs(y1 - y0):
            verts.extend([(x0, y0 - w, .02), (x1, y0 - w, .02), (x1, y0 + w, .02), (x0, y0 + w, .02)])
        else:
            verts.extend([(x0 - w, y0, .02), (x0 + w, y0, .02), (x0 + w, y1, .02), (x0 - w, y1, .02)])
        faces.append((i, i + 1, i + 2, i + 3)); mats.append(m)
    for i in range(NX + 1):
        x = -hx + i * CELL
        bar(x, -hy, x, hy, 1 if i in (0, NX) else 0)
    for j in range(NY + 1):
        y = -hy + j * CELL
        bar(-hx, y, hx, y, 1 if j in (0, NY) else 0)
    me = bpy.data.meshes.new('grid'); me.from_pydata(verts, [], faces)
    for k, col in enumerate([(.2, .85, 1.0), (1.0, .75, .2)]):
        m = bpy.data.materials.new(f'grid{k}')
        nt = m.node_tree
        for n in list(nt.nodes):
            if n.type != 'OUTPUT_MATERIAL': nt.nodes.remove(n)
        e = nt.nodes.new('ShaderNodeEmission'); e.inputs['Color'].default_value = (*col, 1); e.inputs['Strength'].default_value = 2.0
        nt.links.new(e.outputs[0], nt.nodes['Material Output'].inputs['Surface'])
        me.materials.append(m)
    me.polygons.foreach_set('material_index', mats)
    o = bpy.data.objects.new('grid', me); bpy.context.scene.collection.objects.link(o)


def game_camera(yaw, pitch=55, dist=25.0, hfov=62):
    cam = bpy.data.cameras.new('game'); cam.sensor_fit = 'HORIZONTAL'; cam.sensor_width = 36
    cam.lens = 18 / math.tan(math.radians(hfov / 2)); cam.clip_start = .1; cam.clip_end = 2000
    o = bpy.data.objects.new('game', cam); bpy.context.scene.collection.objects.link(o)
    r, p = math.radians(yaw), math.radians(pitch)
    o.location = (math.cos(r) * math.cos(p) * dist, math.sin(r) * math.cos(p) * dist, math.sin(p) * dist)
    o.rotation_euler = (-Vector(o.location)).to_track_quat('-Z', 'Y').to_euler()
    bpy.context.scene.camera = o
    return o


def side_label(cam, text):
    cu = bpy.data.curves.new('t', 'FONT'); cu.body = text; cu.size = .045
    t = bpy.data.objects.new('t', cu); bpy.context.scene.collection.objects.link(t)
    m = bpy.data.materials.new('tm'); nt = m.node_tree
    for n in list(nt.nodes):
        if n.type != 'OUTPUT_MATERIAL': nt.nodes.remove(n)
    e = nt.nodes.new('ShaderNodeEmission'); e.inputs['Color'].default_value = (1, .9, .6, 1)
    nt.links.new(e.outputs[0], nt.nodes['Material Output'].inputs['Surface']); cu.materials.append(m)
    t.parent = cam
    # place in the camera frustum at depth 1: half width = 18/lens
    hw = 18 / cam.data.lens
    t.location = (-hw * .95, -hw * 9 / 16 * .9, -1)


for key in sys.argv[sys.argv.index('--') + 1:]:
    name = 'SM_World_Vigil' if key == 'vigil' else 'SM_World_Root'
    tiles = []
    for yaw, side in ((-90, 'from south (-Y)'), (0, 'from east (+X)'), (90, 'from north (+Y)'), (180, 'from west (-X)')):
        R.setup_scene((960, 540), bg=(.02, .025, .04) if key == 'vigil' else (.05, .07, .05))
        obs = R.import_glb(f'{L.BUILD}/{name}.glb')
        for o in obs: R.unlit_vertex_color(o)
        grid_overlay()
        cam = game_camera(yaw)
        side_label(cam, f'{key.upper()}  55deg, 25 m, {side}  -  1.10 m grid 18x12')
        tiles.append(R.render_to_array(f'{L.WORK}/tile.png'))
    L.save_png(R.sheet(tiles, 2), f'{L.BUILD}/check_{key}.png')
    print('CHECK', key)
