"""Previews of the journey map (issue #248) through the game's journey camera: Blender EEVEE, 1920 x 1080, the HUD's bands drawn in grey.

  blender -b --python-exit-code 1 --python Art/dark-fantasy-sources/r001-journey/scripts/render_previews_journey.py [-- shot ...]

Shots: fresh (stage 1's fork, forest and cathedral raised), mid (two stages won, the third's fork, fog over four and five), finale (four won, two Vault roads),
victory (the whole road, the Vault showcased) and defeat (lost at the third stage, the rest unreached).  Placement follows JourneyLayout.h (via journey_layout.json
and the constants below).  The lighting is a stand-in (warm points at the two candles and over each plinth, a dim cool fill); the game's grade is not here, so the
game capture is what counts.  Camera: UE eye (0, -303.17, 260.04) cm above the table top, aim (0, 0.61566, -0.78801), FOV 50 horizontal.
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath(os.path.join(HERE, '..', '..', 'r002-overview', 'scripts')))
import render_previews_overview as RP
from render_previews_overview import bpy, np, L, RL

ROOT = RP.ROOT
SRC = os.path.abspath(os.path.join(HERE, '..'))
PREV = os.path.join(SRC, 'previews')
LAY = json.load(open(os.path.join(SRC, 'journey_layout.json')))
LIFT = 80.0
W, H = 1920, 1080
S = 1.5                                   # 1080 / 720 design units
MESH = lambda name, aid: os.path.join(ROOT, 'Art', aid, 'r001', name + '.glb')
SCALE = {'root': .70, 'vigil': .62, 'vault': .72}
PLINTH_H = 14.0
MAP_TOP = 0.6


def hud_rects(ended):
    Wd = W / S
    rects = [(Wd - 130, 4, Wd - 4, 48), (8, 4, 288, 50)]
    if not ended:
        rects += [(8, 54, 208, 54 + 3 * 68 + 64), (Wd / 2 - 260, 6, Wd / 2 + 260, 48), (Wd / 2 - 400, 476, Wd / 2 - 10, 646), (Wd / 2 + 10, 476, Wd / 2 + 400, 646)]
    else:
        rects += [(Wd / 2 - 440, 476, Wd / 2 + 440, 646)]
    return [tuple(int(v * S) for v in r) for r in rects]


def put(path, x, y, z, yaw, scale=(1, 1, 1), label='p', fog=False):
    obs = RL.import_glb(path)
    root = bpy.data.objects.new(label, None)
    bpy.context.scene.collection.objects.link(root)
    for o in obs:
        if o.parent is None:
            o.parent = root
        if fog:
            fog_material(o)
        else:
            RP.game_like(o, ambient=.5)
    root.location = (x / 100, -y / 100, (LIFT + z) / 100)
    root.rotation_euler = (0, 0, math.radians(-yaw))
    root.scale = scale
    return root


def fog_material(ob):
    for slot in ob.material_slots:
        m = slot.material
        nt = m.node_tree
        for n in list(nt.nodes):
            if n.type not in ('OUTPUT_MATERIAL', 'TEX_IMAGE'):
                nt.nodes.remove(n)
        tex = next(n for n in nt.nodes if n.type == 'TEX_IMAGE')
        out = next(n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL')
        ca = nt.nodes.new('ShaderNodeVertexColor')
        ca.layer_name = ob.data.color_attributes[0].name
        mul = nt.nodes.new('ShaderNodeMix')
        mul.data_type, mul.blend_type = 'RGBA', 'MULTIPLY'
        mul.inputs['Factor'].default_value = 1.0
        nt.links.new(tex.outputs['Color'], mul.inputs[6])
        nt.links.new(ca.outputs['Color'], mul.inputs[7])
        em = nt.nodes.new('ShaderNodeEmission')
        nt.links.new(mul.outputs[2], em.inputs['Color'])
        tr = nt.nodes.new('ShaderNodeBsdfTransparent')
        mix = nt.nodes.new('ShaderNodeMixShader')
        nt.links.new(ca.outputs['Alpha'], mix.inputs[0])
        nt.links.new(tr.outputs[0], mix.inputs[1])
        nt.links.new(em.outputs[0], mix.inputs[2])
        nt.links.new(mix.outputs[0], out.inputs['Surface'])
        m.surface_render_method = 'BLENDED'
        m.use_backface_culling = False


def light(x, y, z, energy, color):
    ld = bpy.data.lights.new('l', 'POINT')
    ld.color, ld.energy, ld.shadow_soft_size = color, energy, .03
    lo = bpy.data.objects.new('l', ld)
    bpy.context.scene.collection.objects.link(lo)
    lo.location = (x / 100, -y / 100, (LIFT + z) / 100)


def ink(a, b, walked):
    dx, dy = b[0] - a[0], b[1] - a[1]
    yaw = math.degrees(math.atan2(dy, dx))
    put(MESH('SM_Journey_Ink' if walked else 'SM_Journey_InkDashed', 'journey-ink' if walked else 'journey-ink-dashed'), a[0], a[1], MAP_TOP + .05, yaw, (math.hypot(dx, dy) / 100, 1, 1))


TOKEN = {'cleared': (1.0, .8, .46), 'lost': (.62, .16, .1), 'current': (1.3, 1.16, .9), 'fogged': (.3, .29, .28), 'unreached': (.46, .43, .4)}


def scene(stages, raised, goal_reached):
    """stages: list of (mark, world); raised: list of (world, spot_x, scale_boost)."""
    sc = RL.setup_scene((W, H), bg=(.012, .010, .010))
    sc.render.resolution_x, sc.render.resolution_y = W, H
    sc.view_settings.view_transform = 'Standard'
    put(MESH('SM_Journey_Table', 'journey-table'), 0, 0, 0, -90)
    put(MESH('SM_Journey_Map', 'journey-map'), 0, 0, 0, -90)
    spots = LAY['stages_cm']
    for i, (mark, world) in enumerate(stages):
        x, y = spots[i]
        prev = LAY['trailhead_cm'] if i == 0 else spots[i - 1]
        ink(prev, (x, y), mark in ('cleared', 'lost', 'current'))
        tok = put(MESH('SM_Journey_Stage', 'journey-stage'), x, y, MAP_TOP, -90)
        for o in tok.children:
            for slot in o.material_slots:
                b = slot.material.node_tree.nodes.get('Principled BSDF')
        if world:
            put(MESH('SM_Journey_Emblem_' + world.capitalize(), 'journey-emblem-' + world), x, y, MAP_TOP, -90)
        if mark == 'cleared':
            put(MESH('SM_Journey_Pennant', 'journey-pennant'), x, y, MAP_TOP, -90)
        if mark == 'lost':
            put(MESH('SM_Journey_Lost', 'journey-lost'), x, y, MAP_TOP, -90)
        if mark == 'fogged':
            put(MESH('SM_Journey_Fog', 'journey-fog'), x, y, MAP_TOP + .4, -90 + (i * 37 % 40 - 20), fog=True)
    ink(spots[-1], LAY['goal_cm'], goal_reached)
    gx, gy = LAY['goal_cm']
    put(MESH('SM_Journey_Goal', 'journey-goal'), gx, gy, MAP_TOP, -90)
    fork = next((i for i, (m, _) in enumerate(stages) if m == 'current'), None)
    for k, (world, sx, boost) in enumerate(raised):
        scale = SCALE[world] * boost
        if fork is not None:
            ink(spots[fork], (sx, -100 + 36 * scale), True)
        yaw = -90 + (0 if len(raised) == 1 else (-6 if k == 0 else 6))
        put(MESH('SM_Journey_Plinth', 'journey-plinth'), sx, -100, MAP_TOP, yaw, (scale,) * 3)
        put(RP.DIO[world], sx, -100, MAP_TOP + PLINTH_H * scale, yaw, (scale,) * 3)
        light(sx, -134, 92, 60, (1.0, .86, .66))
    for (x, y, z) in LAY['candles_cm']:
        light(x, y, z, 90, (1.0, .62, .32))
    light(gx, gy - 8, 26, 8, (.45, .9, 1.0))
    fl = bpy.data.lights.new('fill', 'SUN')
    fl.energy, fl.color = .3, (.55, .65, 1.0)
    fo = bpy.data.objects.new('fill', fl)
    fo.rotation_euler = (math.radians(50), 0, math.radians(30))
    bpy.context.scene.collection.objects.link(fo)


def camera():
    RP.camera((0, 3.0317, (LIFT + 260.04) / 100), (0, -0.61566, -0.78801), 50.0)


def render(path, ended):
    sc = bpy.context.scene
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    img = bpy.data.images.load(path)
    a = np.empty(W * H * 4, np.float32)
    img.pixels.foreach_get(a)
    a = a.reshape(H, W, 4)[::-1].copy()
    for (x0, y0, x1, y1) in hud_rects(ended):
        a[y0:y1, x0:x1, :3] = a[y0:y1, x0:x1, :3] * .25 + .06
    L.save_png(a, path)
    bpy.data.images.remove(img)
    print('PREVIEW', path)


def main():
    only = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    os.makedirs(PREV, exist_ok=True)
    shots = {
        'fresh': ([('current', None)] + [('fogged', None)] * 4, [('root', 40, 1.0), ('vigil', -40, 1.0)], False, False),
        'mid': ([('cleared', 'root'), ('cleared', 'vigil'), ('current', None), ('fogged', None), ('fogged', None)], [('vigil', 40, 1.0), ('root', -40, 1.0)], False, False),
        'finale': ([('cleared', 'root'), ('cleared', 'vigil'), ('cleared', 'vigil'), ('cleared', 'root'), ('current', None)], [('vault', 40, 1.0), ('vault', -40, 1.0)], False, False),
        'victory': ([('cleared', 'root'), ('cleared', 'vigil'), ('cleared', 'vigil'), ('cleared', 'root'), ('cleared', 'vault')], [('vault', 0, 1.0)], True, True),
        'defeat': ([('cleared', 'root'), ('cleared', 'vigil'), ('lost', 'vigil'), ('unreached', None), ('unreached', None)], [('vigil', 0, 1.0)], False, True),
    }
    for name, (stages, raised, reached, ended) in shots.items():
        if only and name not in only:
            continue
        scene(stages, raised, reached)
        camera()
        render(os.path.join(PREV, 'journey_' + name + '.png'), ended)


if __name__ == '__main__':
    RP.run_guarded(main)
