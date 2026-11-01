"""Cache every source material texture (top-down float RGBA) to work/textures.npz as '<blend>/<material>'."""
import sys; sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import bpy, numpy as np, tlib as L
out = {}
for b in ('vigil', 'root', 'mission'):
    L.open_blend(b)
    for m in bpy.data.materials:
        out[f'{b}/{m.name}'] = L.material_texture(m)
np.savez_compressed(f'{L.WORK}/textures.npz', **out)
print('TEX', len(out), sorted(out)[:80])
