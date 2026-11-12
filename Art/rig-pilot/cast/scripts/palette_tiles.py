"""Per-class atlas tile report for the #150 batch 4 palette design (Blender, fresh process, read only).

    blender -b --python-exit-code 1 --python Art/rig-pilot/cast/scripts/palette_tiles.py

For each of the 13 party classes it re-imports the LOD0 glb and writes Art/rig-pilot/cast/palettes/tiles.json: for every one of the 13 atlas
tiles (index = the REGIONS order, row-major on a 4x4 grid, row 0 at the top, so Unreal's UV (u, v) falls in tile floor(v*4)*4 + floor(u*4))
the mean linear-ish sRGB colour, the share of the mesh surface area that uses it and the mean height of those faces in metres. The palette
table (palettes.json) is designed against this: slots name tiles, never meshes.
"""
import json
import os
import sys

import bpy
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
REGIONS = ['plate', 'trim', 'cloth', 'leather', 'blade', 'glow', 'rune', 'skin', 'bone', 'accent', 'dark', 'emblem', 'emblem2']
CLASSES = {  # class key: (asset id, mesh name, revision folder with the shipped LOD0 glb)
    'fighter': ('sk-hero-fighter', 'SK_Hero_Fighter', 'r002'), 'rogue': ('sk-hero-rogue', 'SK_Hero_Rogue', 'r002'),
    'cleric': ('sk-hero-cleric', 'SK_Hero_Cleric', 'r002'), 'wizard': ('sk-hero-wizard', 'SK_Hero_Wizard', 'r002'),
    'barbarian': ('sk-hero-barbarian', 'SK_Hero_Barbarian', 'r001'), 'monk': ('sk-hero-monk', 'SK_Hero_Monk', 'r001'),
    'paladin': ('sk-hero-paladin', 'SK_Hero_Paladin', 'r001'), 'ranger': ('sk-hero-ranger', 'SK_Hero_Ranger', 'r001'),
    'sorcerer': ('sk-hero-sorcerer', 'SK_Hero_Sorcerer', 'r001'), 'warlock': ('sk-hero-warlock', 'SK_Hero_Warlock', 'r001'),
    'druid': ('sk-hero-druid', 'SK_Hero_Druid', 'r001'), 'bard': ('sk-hero-bard', 'SK_Hero_Bard', 'r001'),
    'artificer': ('sk-hero-artificer', 'SK_Hero_Artificer', 'r001'),
}
out = {}
for key, (aid, name, rev) in CLASSES.items():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, 'Art', aid, rev, f'{name}.glb'))
    me = next(o for o in bpy.context.scene.objects if o.type == 'MESH').data
    img = next(i for i in bpy.data.images if i.size[0] == 1024)
    px = np.array(img.pixels[:], np.float32).reshape(1024, 1024, 4)[::-1]      # Blender pixels are bottom-up; flip to row 0 = top
    uv = me.uv_layers.active.data
    area = np.zeros(13)
    zsum = np.zeros(13)
    for poly in me.polygons:
        u, v = np.mean([uv[l].uv for l in poly.loop_indices], axis=0)
        tile = int((1 - v) * 4) * 4 + int(u * 4)                                # Blender v is bottom-up
        if tile > 12:
            continue
        area[tile] += poly.area
        zsum[tile] += poly.area * poly.center.z
    total = area.sum()
    tiles = {}
    for i, region in enumerate(REGIONS):
        c, r = i % 4, i // 4
        mean = px[r * 256 + 8:(r + 1) * 256 - 8, c * 256 + 8:(c + 1) * 256 - 8, :3].reshape(-1, 3).mean(0)
        tiles[region] = dict(index=i, rgb=[round(float(x), 3) for x in mean], area_share=round(float(area[i] / total), 4),
                             mean_z=round(float(zsum[i] / area[i]), 3) if area[i] else None)
    out[key] = tiles
    print(key, {k: v['area_share'] for k, v in tiles.items() if v['area_share'] > .02})
with open(os.path.join(ROOT, 'Art', 'rig-pilot', 'cast', 'palettes', 'tiles.json'), 'w') as fh:
    json.dump(out, fh, indent=1)
