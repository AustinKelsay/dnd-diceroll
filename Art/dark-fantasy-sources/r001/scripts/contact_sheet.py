"""Contact sheet of every exported asset: re-imports each .glb, three-quarter view, sun + fill,
neutral dark background, label with name and triangle count. -> theme-build/contact_sheet.png"""
import math, sys
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import bpy
from mathutils import Vector
import render_lib as R, tlib as L

ASSETS = ['SM_Hero_Fighter', 'SM_Hero_Rogue', 'SM_Hero_Cleric', 'SM_Hero_Wizard', 'SM_Foe_Raider', 'SM_Foe_Archer', 'SM_Foe_Scout',
          'SM_Prop_Stones', 'SM_Prop_Pillar', 'SM_Prop_Wall', 'SM_Prop_Brazier', 'SM_Prop_Flame', 'SM_Prop_Banner', 'SM_Prop_Cage',
          'SM_Tile_Vigil', 'SM_Tile_Root', 'SM_Diorama_Vigil', 'SM_Diorama_Root', 'SM_Tabletop', 'SM_World_Vigil', 'SM_World_Root']
SIZE = 420
tiles = []
for n in ASSETS:
    R.setup_scene((SIZE, SIZE), bg=(.045, .045, .05))
    obs = R.import_glb(f'{L.BUILD}/{n}.glb')
    tris = sum(len(o.data.polygons) for o in obs)
    baked = len(obs[0].data.color_attributes) > 0
    if baked:
        for o in obs: R.unlit_vertex_color(o, gain=1.6 if n == 'SM_Tabletop' else 1.0)
    else:
        R.lights(sun=4.0, fill=1.2)
    if n.startswith('SM_World'):
        cd = bpy.data.cameras.new('c'); cd.type = 'ORTHO'; cd.ortho_scale = 46; cd.clip_end = 3000
        o = bpy.data.objects.new('c', cd); bpy.context.scene.collection.objects.link(o)
        d = Vector((1, -.85, .9)).normalized()
        o.location = d * 120 + Vector((0, 6, 4)); o.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
        bpy.context.scene.camera = o
        cam = o
    else:
        cam = R.camera_three_quarter(obs, (1.0, -.85, .7) if n != 'SM_Tabletop' else (1.0, -.5, .75))
    R.label(cam, f'{n}  {tris} tris' + ('  +COLOR_0' if baked else ''), size=.045)
    tiles.append(R.render_to_array(f'{L.WORK}/tile.png'))
    print('TILE', n, tris)
L.save_png(R.sheet(tiles, 6), f'{L.BUILD}/contact_sheet.png')
