"""Characters from vigil.blend -> SM_Hero_*/SM_Foe_*.glb (1.15 m tall, facing +X, pivot at feet)."""
import sys; sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import bpy, numpy as np, tlib as L

W, H = 18, 12
def coord(i, j): return ((i - (W - 1) / 2) * 2, (j - (H - 1) / 2) * 2)
def level(x, y): return .4 if 6 <= y < 8 else .8 if 8 <= y < 10 else 1.2 if y >= 10 else 0.
# worlds_pass.CAST: (unit, cell, yaw). Placement there was T(cell) @ Rz(yaw) @ S(1.2, 1.2, 1).
CAST = [('arden', (7, 4), 28), ('nix', (6, 2), 25), ('sera', (5, 4), 25), ('ivo', (5, 6), 35),
        ('raider', (10, 5), -60), ('raider', (12, 3), -55), ('vault', (12, 9), -110)]
OUT = [('SM_Hero_Fighter', 0), ('SM_Hero_Rogue', 1), ('SM_Hero_Cleric', 2), ('SM_Hero_Wizard', 3),
       ('SM_Foe_Raider', 4), ('SM_Foe_Archer', 6), ('SM_Foe_Scout', 4)]
HEIGHT = 1.15
ORB_PX = (448, 448, 32)            # free 128px block of the 512 atlas (x, y top-down, size)
BONE = (0, .25, .5, .75)           # chars3.BONE patch, source UVs (u0, u1, v0, v1); raider uses it only for antlers

L.open_blend('vigil')
src = {}
for i, (key, cell, yaw) in enumerate(CAST):
    x, y = coord(*cell)
    M = L.Rz(90) @ L.S(1 / 1.2, 1 / 1.2, 1) @ L.Rz(-yaw) @ L.T(-x, -y, -level(x, y))  # back to authored model, front -Y -> +X
    parts = []
    for o in bpy.data.objects:
        if not o.name.startswith(f'{key}_{i}_'):
            continue
        s, texs = L.soup_from_object(o)
        s = L.xform(s, M)
        if o.name.endswith(':2s') or 'fx:orb' in o.name:
            s = L.cat(s, L.flipped(s, .004))          # formerly two-sided: duplicate with flipped normals
        if 'fx:orb' in o.name:
            s['mat'][:] = 'orb'
        else:
            s['mat'][:] = 'atlas'
            tex = texs[o.data.materials[0].name]
        parts.append(s)
    src[i] = (L.cat(*parts), tex)
    print('SRC', key, i, len(src[i][0]['P']))


def scout_palette(t):
    """Duller, lighter-armoured scout: desaturate, push toward olive leather, dim the orange trim."""
    t = t.copy()
    rgb = t[..., :3]
    g = rgb.mean(-1, keepdims=True)
    rgb = g + (rgb - g) * .35                       # strong desaturation
    rgb = rgb * np.array([.92, .98, .78]) * 1.3    # olive-brown cast, slightly lighter
    t[..., :3] = np.clip(rgb, 0, 1)
    return t


bpy.ops.wm.read_factory_settings(use_empty=True)
for name, idx in OUT:
    s, tex = src[idx]
    s = dict(s); s['mat'] = s['mat'].copy()
    tex = tex.copy()
    if name == 'SM_Foe_Scout':
        uv = s['UV']
        antler = np.all((uv[..., 0] >= BONE[0] - 1e-4) & (uv[..., 0] <= BONE[1] + 1e-4) &
                        (uv[..., 1] >= BONE[2] - 1e-4) & (uv[..., 1] <= BONE[3] + 1e-4), 1)
        print('scout: dropping', int(antler.sum()), 'antler tris')
        s = L.sub(s, ~antler)
        tex = scout_palette(tex)
    # orb: solid bright swatch in the unused corner of the atlas
    x0, y0, n = ORB_PX
    used = s['UV'][s['mat'] == 'atlas'].reshape(-1, 2)
    assert not np.any((used[:, 0] > x0 / 512 + .001) & (used[:, 1] > y0 / 512 + .001)), 'orb block overlaps atlas use'
    tex[y0:y0 + n, x0:x0 + n, :3] = (.62, .86, 1.0)
    orb = s['mat'] == 'orb'
    s['UV'][orb, :, 0] = (x0 + n / 2) / 512
    s['UV'][orb, :, 1] = (y0 + n / 2) / 512
    s = L.clip_z(s, 0.0)
    top = s['P'][..., 2].max()
    s = L.xform(s, L.S(HEIGHT / top))
    s = L.direct_uv(s)
    ob = L.make_object(name, s, tex)
    L.export_glb(ob)
