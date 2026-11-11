"""Previews of the overview set (issue #149), through the game's overview camera: Blender EEVEE, 1920 x 1080.

  blender -b --python Art/dark-fantasy-sources/r002-overview/scripts/render_previews_overview.py [-- shot ...]      (under the machine lock)

Shots: game_start / game_route / game_finale (the overview camera with the route dioramas placed as BuildOverview places them, with the HUD blocks of the
start screen and the route screen drawn over them in grey, so what the HUD hides is visible), table_wide, and one close view per diorama
(diorama_root, diorama_vigil, diorama_vault).  The lighting is a stand-in (three warm point lights at the candle flames, a dim cool fill); the game's
tonemap, grade and shadows are not here.  Camera: UE eye (0, -420, 288) cm, FOV 50 (horizontal), aim (0, 450, -282): Blender world = UE with Y mirrored.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ov_lib as O
from ov_lib import L, R, bpy, np, Vector
sys.path.insert(0, os.path.join(O.SRC, 'r001', 'scripts'))
import render_lib as RL

ROOT = O.ROOT
PREV = os.path.join(O.OUT_SRC, 'previews')
LIFT = 0.80
LIGHTS_M = {'candelabra': (-0.30, 1.00, 0.43), 'pillar': (-0.34, -1.00, 0.30), 'cluster': (1.55, 0.95, 0.12)}
W, H = 1920, 1080
HUD = {
    'start': [(20, 15, 465, 1060), (1330, 15, 1900, 250), (1330, 850, 1900, 1050), (485, 1010, 1310, 1070)],
    'route': [(12, 10, 415, 75), (12, 85, 310, 485), (405, 210, 1515, 325), (428, 335, 938, 715), (985, 335, 1493, 715), (1775, 10, 1910, 75), (285, 985, 840, 1005)],
}
TABLE = os.path.join(ROOT, 'Art', 'overview-tabletop', 'r001', 'SM_Overview_Tabletop.glb')
DIO = {k: os.path.join(ROOT, 'Art', f'overview-diorama-{k}', 'r001', f'SM_Overview_Diorama_{k.capitalize()}.glb') for k in ('root', 'vigil', 'vault')}


def game_like(ob, ambient=.55, glow_gain=2.6):
    for slot in ob.material_slots:
        nt = slot.material.node_tree
        tex = next(n for n in nt.nodes if n.type == 'TEX_IMAGE')
        out = next(n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL')
        bsdf = nt.nodes.get('Principled BSDF')
        ca = nt.nodes.new('ShaderNodeVertexColor')
        ca.layer_name = ob.data.color_attributes[0].name
        mul = nt.nodes.new('ShaderNodeMix')
        mul.data_type, mul.blend_type = 'RGBA', 'MULTIPLY'
        mul.inputs['Factor'].default_value = 1.0
        nt.links.new(tex.outputs['Color'], mul.inputs[6])
        nt.links.new(ca.outputs['Color'], mul.inputs[7])
        nt.links.new(mul.outputs[2], bsdf.inputs['Base Color'])
        bsdf.inputs['Roughness'].default_value = 1.0
        bsdf.inputs['Specular IOR Level'].default_value = 0.0
        em = nt.nodes.new('ShaderNodeEmission')
        nt.links.new(mul.outputs[2], em.inputs['Color'])
        k = nt.nodes.new('ShaderNodeMath')
        k.operation = 'MULTIPLY_ADD'
        k.inputs[1].default_value, k.inputs[2].default_value = glow_gain, ambient
        nt.links.new(ca.outputs['Alpha'], k.inputs[0])
        nt.links.new(k.outputs[0], em.inputs['Strength'])
        add = nt.nodes.new('ShaderNodeAddShader')
        nt.links.new(bsdf.outputs[0], add.inputs[0])
        nt.links.new(em.outputs[0], add.inputs[1])
        nt.links.new(add.outputs[0], out.inputs['Surface'])
        slot.material.use_backface_culling = True


def put(path, loc_ue, yaw_ue, label):
    """Imports a glb and places it like the game does: UE location (cm) and yaw, with Y mirrored into Blender."""
    obs = RL.import_glb(path)
    root = bpy.data.objects.new(label, None)
    bpy.context.scene.collection.objects.link(root)
    for o in obs:
        if o.parent is None:
            o.parent = root
        game_like(o)
    root.location = (loc_ue[0] / 100, -loc_ue[1] / 100, loc_ue[2] / 100)
    root.rotation_euler = (0, 0, math.radians(-yaw_ue))
    return root


def scene(dioramas, with_floor=True):
    sc = RL.setup_scene((W, H), bg=(.012, .010, .010))
    sc.render.resolution_x, sc.render.resolution_y = W, H
    sc.view_settings.view_transform = 'Standard'
    put(TABLE, (0, 0, LIFT * 100), -90, 'table')
    n = len(dioramas)
    for i, key in enumerate(dioramas):
        x = 0.0 if n == 1 else (52.0 if i == 0 else -52.0)
        put(DIO[key], (x, 22.0, LIFT * 100), -90 + (-10.0 if i == 0 else 10.0), 'dio_' + key)
    if with_floor:
        bpy.ops.mesh.primitive_plane_add(size=6, location=(0, 0, .05))
        fl = bpy.context.object
        m = bpy.data.materials.new('floor')
        m.diffuse_color = (.01, .008, .006, 1)
        fl.data.materials.append(m)
    for name, (x, y, z) in LIGHTS_M.items():
        ld = bpy.data.lights.new(name, 'POINT')
        ld.color = (1.0, .62, .32)
        ld.energy = {'candelabra': 90, 'pillar': 90, 'cluster': 45}[name]
        ld.shadow_soft_size = .02
        lo = bpy.data.objects.new(name, ld)
        bpy.context.scene.collection.objects.link(lo)
        # asset (x front, y right) -> world: the table's yaw is +90 degrees about Z in Blender, so (x, y) -> (-y, x)
        lo.location = (-y, x, LIFT + z)
    fl = bpy.data.lights.new('fill', 'SUN')
    fl.energy, fl.color = .35, (.55, .65, 1.0)
    fo = bpy.data.objects.new('fill', fl)
    fo.rotation_euler = (math.radians(50), 0, math.radians(30))
    bpy.context.scene.collection.objects.link(fo)
    return sc


def camera(eye, look, fov=50.0):
    sc = bpy.context.scene
    cd = bpy.data.cameras.new('cam')
    cd.sensor_fit, cd.angle = 'HORIZONTAL', math.radians(fov)
    cd.clip_start, cd.clip_end = .05, 50
    co = bpy.data.objects.new('cam', cd)
    sc.collection.objects.link(co)
    d = Vector(look)
    co.location = eye
    co.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    sc.camera = co


def game_camera():
    camera((0, 4.20, LIFT + 2.08), (0, -4.50, -2.82))


def render(path, hud=None):
    sc = bpy.context.scene
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    if hud:
        img = bpy.data.images.load(path)
        a = np.empty(W * H * 4, np.float32)
        img.pixels.foreach_get(a)
        a = a.reshape(H, W, 4)[::-1].copy()
        for (x0, y0, x1, y1) in HUD[hud]:
            a[y0:y1, x0:x1, :3] = a[y0:y1, x0:x1, :3] * .25 + .06
        L.save_png(a, path)
        bpy.data.images.remove(img)
    print('PREVIEW', path)


def main():
    only = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    os.makedirs(PREV, exist_ok=True)
    shots = {
        'game_start': (['root', 'vigil'], 'start'), 'game_route': (['root', 'vigil'], 'route'), 'game_finale': (['vault'], 'route'),
        'game_nohud': (['root', 'vigil'], None),
    }
    for name, (dios, hud) in shots.items():
        if only and name not in only:
            continue
        dios = [d for d in dios if os.path.exists(DIO[d])]
        scene(dios)
        game_camera()
        render(os.path.join(PREV, name + '.png'), hud)
    for key in ('root', 'vigil', 'vault'):
        if (only and 'diorama_' + key not in only) or not os.path.exists(DIO[key]):
            continue
        sc = RL.setup_scene((1280, 800), bg=(.012, .010, .010))
        put(DIO[key], (0, 0, 0), -90, 'd')
        ld = bpy.data.lights.new('k', 'SUN')
        ld.energy, ld.color = 2.2, (1.0, .78, .55)
        lo = bpy.data.objects.new('k', ld)
        lo.rotation_euler = (math.radians(50), math.radians(20), math.radians(160))
        sc.collection.objects.link(lo)
        camera((0, 1.0, .55), (0, -1.0, -.62), 40)
        W_, H_ = 1280, 800
        sc.render.resolution_x, sc.render.resolution_y = W_, H_
        sc.render.filepath = os.path.join(PREV, f'diorama_{key}.png')
        bpy.ops.render.render(write_still=True)
    if not only or 'table_wide' in only:
        scene([d for d in ('root', 'vigil') if os.path.exists(DIO[d])])
        camera((0, 3.0, LIFT + 1.7), (0, -3.0, -1.9), 60)
        render(os.path.join(PREV, 'table_wide.png'))


if __name__ == '__main__':
    main()
