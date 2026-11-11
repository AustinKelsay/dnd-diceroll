"""The three route dioramas, r002 (issue #149): the forest clearing, the drowned cathedral and the vault hall as miniatures on the war table.

  blender -b --python Art/dark-fantasy-sources/r002-overview/scripts/build_dioramas_r002.py [-- root|vigil|vault ...]   (under the machine lock)

Each diorama is the real r002 world cropped to a window, scaled to a 58 cm tray (about 1:50), so it reads as its own world at the overview zoom
by construction: the same paving, walls, spires, Waystone and stained glass, with the baked vertex light and glow mask the world carries.  Built from the
LOD exports (world LOD2, occluders LOD1/LOD2) plus a few kit pieces placed where the game puts obstacles, packed into one 1024 atlas (world x 1/4, occluders
x 1/8, props x 1/4), set on a dark-oak tray with brass corners and a plaque, and decimated to a triangle target.

Conventions: see ov_lib (the asset faces +X, +Y is the viewer's right, origin = middle of the tray underside).  World to diorama: x_a = -y_w (the world's
front, -Y, faces the viewer), y_a = x_w, scale s.
Outputs: Art/overview-diorama-<name>/r001/SM_Overview_Diorama_<Name>.glb, r002-overview/diorama_metrics.json, atlases under r002-overview/.
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ov_lib as O
from ov_lib import L, R, bpy, np, T, Rz, Soup, cbox, clip_attr
import lod_r002 as LOD

ROOT, OUT_SRC = O.ROOT, O.OUT_SRC
ATLAS, TRAY_W = 1024, 0.58
STEP, PLINTH, SKIRT, RAIL = 0.05, 0.11, 0.022, 0.014          # a two-tier display riser: it lifts the miniature above the route banner
STAGE_Z = PLINTH + SKIRT


def glb(asset, rev, name):
    return os.path.join(ROOT, 'Art', asset, f'r{rev:03d}', name + '.glb')


def kit(asset, rev, name):
    return glb(asset, rev, name)


# world window (x0, x1, y0, y1) in the glb's frame, zmax (world metres), back = which sign of y the tall back architecture has in that frame (Vigil +Y, Vault -Y),
# zscale = vertical exaggeration (a miniature reads better a little taller), pscale = kit pieces drawn larger than the world scale (they are the landmarks),
# props = (glb, x, y, yaw, down), gain = texture brightness (the overview's three candles light it)
SPECS = {
    'root': dict(
        asset='overview-diorama-root', name='SM_Overview_Diorama_Root', window=(-14, 14, -10, 10), zmax=16.0, back='+y', zscale=2.3, pscale=1.9, skirt='soil', plaque='leaf', gain=(1.4, 1.9, 1.3),
        sources=[('world', glb('world-root', 2, 'SM_World_Root_LOD2'), 4), ('occ', glb('world-root-occluders', 1, 'SM_World_Root_Occluders_LOD2'), 8)],
        props=[(kit('prop-root-pillar', 1, 'SM_Prop_Root_Pillar_LOD2'), -6, 4, 20, 4), (kit('prop-root-pillar-b', 1, 'SM_Prop_Root_Pillar_B_LOD2'), 7, 5, 200, 4),
               (kit('prop-root-pillar-c', 1, 'SM_Prop_Root_Pillar_C_LOD2'), -1, -5, 70, 4), (kit('prop-root-stones', 1, 'SM_Prop_Root_Stones_LOD2'), 9, -3, 10, 4),
               (kit('prop-root-arch-2cell', 1, 'SM_Prop_Root_Arch2Cell_LOD2'), -9, -5, 0, 4), (kit('scatter-root-mushrooms', 1, 'SM_Scatter_Root_Mushrooms'), 3, -1, 0, 4),
               (kit('scatter-root-mushrooms', 1, 'SM_Scatter_Root_Mushrooms'), -8, 0, 90, 4)],
        target=14000),
    'vigil': dict(
        asset='overview-diorama-vigil', name='SM_Overview_Diorama_Vigil', window=(-19, 19, -11, 25), zmax=22.0, back='+y', zscale=1.35, pscale=1.6, skirt='ashlar', plaque='wave', gain=2.0,
        sources=[('world', glb('world-vigil', 2, 'SM_World_Vigil_LOD2'), 4), ('occ', glb('world-vigil-occluders', 2, 'SM_World_Vigil_Occluders_LOD2'), 16)],
        props=[(kit('prop-pillar', 2, 'SM_Prop_Pillar_LOD2'), -7, 3, 0, 4), (kit('prop-pillar-b', 1, 'SM_Prop_Pillar_B_LOD2'), 6, 6, 0, 4),
               (kit('prop-pillar', 2, 'SM_Prop_Pillar_LOD2'), 3, -5, 0, 4), (kit('prop-wall-c', 1, 'SM_Prop_Wall_C_LOD2'), -3, -8, 0, 4),
               (kit('prop-arch-2cell', 1, 'SM_Prop_Arch2Cell_LOD2'), 9, -2, 90, 4), (kit('prop-pillar-b', 1, 'SM_Prop_Pillar_B_LOD2'), -11, -9, 0, 4)],
        target=18000),
    'vault': dict(
        asset='overview-diorama-vault', name='SM_Overview_Diorama_Vault', window=(-18, 18, -17, 9), zmax=12.0, back='-y', zscale=2.4, pscale=1.7, skirt='ashlar', plaque='rune', gain=2.4,
        sources=[('world', glb('world-vault', 1, 'SM_World_Vault_LOD2'), 4), ('occ', glb('world-vault-occluders', 1, 'SM_World_Vault_Occluders_LOD1'), 8)],
        props=[(kit('prop-vault-pillar', 1, 'SM_Prop_Vault_Pillar_LOD2'), -7, -3, 0, 4), (kit('prop-vault-pillar-b', 1, 'SM_Prop_Vault_Pillar_B_LOD2'), 6, -2, 0, 4),
               (kit('prop-vault-pillar-c', 1, 'SM_Prop_Vault_Pillar_C_LOD2'), 1, 2, 0, 4), (kit('prop-vault-wall-b', 1, 'SM_Prop_Vault_Wall_B_LOD2'), -3, 5, 90, 4),
               (kit('prop-vault-stones-c', 1, 'SM_Prop_Vault_Stones_C_LOD2'), 8, 4, 0, 4), (kit('scatter-vault-crystals', 1, 'SM_Scatter_Vault_Crystals'), 0, -8, 0, 4)],
        target=None),
}


def soil_tile(n=128, seed=401):
    f = O.fbm_tile(n, n, 4, seed, 4)
    g = O.pnoise(n, n, 18, 18, seed + 7)
    img = np.array((.20, .13, .08)) * (.55 + .8 * f[..., None]) * (.85 + .3 * g[..., None])
    rng = np.random.default_rng(seed)
    acc = np.zeros((n, n))
    for _ in range(10):                                                  # a few roots in the cross-section
        y = rng.uniform(0, n)
        O.stroke(acc, [(0, y), (n * .3, y + rng.normal(0, 4)), (n * .6, y + rng.normal(0, 6)), (n, y + rng.normal(0, 4))], rng.uniform(1.0, 2.2), .8)
    O.ink(img, acc, (.30, .20, .10))
    return O.rgba(img)


def plaque_tile(kind, w=128, h=24):
    """Brass plate with a rim and a small engraved sigil: a leaf and root, waves under an arch, a rune diamond."""
    img = np.array(O.metal_tile((.82, .62, .24), w, 331, streak=True)[:h])
    acc = np.zeros((h, w))
    O.stroke(acc, [(2, 2), (w - 3, 2), (w - 3, h - 3), (2, h - 3), (2, 2)], .8, .9)
    cx, cy = w / 2, h / 2
    if kind == 'leaf':
        O.fill_poly(acc, [(cx, cy - 8), (cx + 7, cy), (cx, cy + 8), (cx - 7, cy)], .9)
        O.stroke(acc, [(cx, cy + 8), (cx, cy + 11)], .8)
    elif kind == 'wave':
        for k in range(3):
            pts = [(cx - 14 + i * 2, cy - 4 + k * 4 + 1.8 * math.sin(i * .9)) for i in range(15)]
            O.stroke(acc, pts, .7, .9)
    else:
        O.stroke(acc, [(cx, cy - 8), (cx + 7, cy), (cx, cy + 8), (cx - 7, cy), (cx, cy - 8)], .8, .9)
        O.stroke(acc, [(cx, cy - 4), (cx, cy + 4)], .7, .9)
    for k in range(5):                                                   # dashes either side
        O.stroke(acc, [(12 + k * 5, cy), (14 + k * 5, cy)], .6, .8)
        O.stroke(acc, [(w - 12 - k * 5, cy), (w - 14 - k * 5, cy)], .6, .8)
    O.ink(img[..., :3], acc, (.20, .12, .04))
    return O.rgba(img[..., :3])


def tray_soup(W, D, spec):
    """The tray in asset coordinates (x = depth toward the viewer, y = width), stage plane at STAGE_Z.  Returns the soup and the textures it needs."""
    sp = Soup()
    sp.box('wood', (0, 0, STEP / 2), (D + .09, W + .09, STEP), tile=1.0)
    sp.box('wood', (0, 0, (STEP + PLINTH) / 2), (D + .07, W + .07, PLINTH - STEP), tile=1.0)
    sp.box('skirt', (0, 0, PLINTH + SKIRT / 2), (D, W, SKIRT), tile=1.0, bottom=False)
    rh = PLINTH + SKIRT + RAIL
    for (cx, cy, sx, sy) in ((D / 2 + .014, 0, .028, W + .056), (-D / 2 - .014, 0, .028, W + .056), (0, W / 2 + .014, D, .028), (0, -W / 2 - .014, D, .028)):
        sp.box('wood', (cx, cy, (PLINTH + rh) / 2), (sx, sy, rh - PLINTH), tile=1.0)
    for sx in (-1, 1):
        for sy in (-1, 1):
            sp.box('brass', (sx * (D / 2 + .016), sy * (W / 2 + .016), (PLINTH + rh) / 2 + .003), (.04, .04, rh - PLINTH + .006), tile=1.0)
    pw, ph = .2, .026
    xf = D / 2 + .014 + .0145
    sp.quad('plaque', (xf, -pw / 2, PLINTH + .004), (xf, pw / 2, PLINTH + .004), (xf, pw / 2, PLINTH + .004 + ph), (xf, -pw / 2, PLINTH + .004 + ph), (0, 1), (1, 1), (1, 0), (0, 0))
    s = sp.soup()
    # vertex light: the top faces catch the candles, the vertical faces are darker
    N = O.tri_normals(s['P'])
    shade = np.where(N[:, 2] > .5, .85, np.where(N[:, 0] > .5, .75, .55))
    s['C'] = np.repeat(shade[:, None, None], 3, 1).repeat(3, 2).astype(float)
    return s


def to_asset(P, spec, s, centre):
    """World (x_w, y_w, z) -> asset (x_a, y_a, z): a quarter turn about Z that puts the world's tall back architecture away from the viewer (a rotation, never a mirror),
    scaled, the heights exaggerated by spec['zscale'], lifted onto the stage."""
    xc, yc = centre
    out = np.empty_like(P)
    if spec['back'] == '+y':
        out[..., 0], out[..., 1] = -(P[..., 1] - yc) * s, (P[..., 0] - xc) * s
    else:
        out[..., 0], out[..., 1] = (P[..., 1] - yc) * s, -(P[..., 0] - xc) * s
    out[..., 2] = P[..., 2] * s * spec['zscale'] + STAGE_Z
    return out


def compose(key, spec):
    x0, x1, y0, y1 = spec['window']
    s = (TRAY_W / (x1 - x0))
    s0 = s
    W, D = (x1 - x0) * s, (y1 - y0) * s
    centre = ((x0 + x1) / 2, (y0 + y1) / 2)
    atlas = O.Atlas(ATLAS)
    parts_P, parts_UV, parts_C, parts_A = [], [], [], []
    stats = {}
    for tag, path, down in spec['sources']:
        P, UVtd, COL, img = O.read_glb_arrays(path)
        n_src = len(P)
        tris = np.concatenate([P, UVtd, COL], -1)                         # (n, 3, 3 + 2 + 4)
        tris = clip_attr(tris, x0, x1, y0, y1)
        tris = clip_z(tris, spec['zmax'])
        key_img = tag
        img = np.array(img)
        img[..., :3] = np.clip(img[..., :3] * np.asarray(spec['gain']), 0, 1)
        atlas.add(key_img, img, down=down, pad=4)
        P2 = to_asset(tris[..., :3], spec, s, centre)
        UV2 = atlas.remap(key_img, tris[..., 3:5])
        parts_P.append(P2)
        parts_UV.append(UV2)
        parts_C.append(tris[..., 5:8])
        parts_A.append(tris[..., 8])
        stats[tag] = dict(source_tris=int(n_src), in_window=int(len(tris)))
    for k, (path, px, py, yaw, down) in enumerate(spec['props']):
        P, UVtd, COL, img = O.read_glb_arrays(path)
        ca, sa = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        Q = P.copy() * spec['pscale']
        Q[..., 0], Q[..., 1] = P[..., 0] * spec['pscale'] * ca - P[..., 1] * spec['pscale'] * sa + px, P[..., 0] * spec['pscale'] * sa + P[..., 1] * spec['pscale'] * ca + py
        Q[..., 2] = P[..., 2] * spec['pscale']
        img = np.array(img)
        img[..., :3] = np.clip(img[..., :3] * np.asarray(spec['gain']), 0, 1)
        key_img = f'prop{k}'
        atlas.add(key_img, img, down=down * 2, pad=4)
        parts_P.append(to_asset(Q, spec, s, centre))
        parts_UV.append(atlas.remap(key_img, UVtd))
        parts_C.append(COL[..., :3])
        parts_A.append(COL[..., 3])
    # the tray
    texs = dict(wood=O.dark_wood_tile(256, 311), brass=O.metal_tile((.80, .60, .22), 64, 331), plaque=plaque_tile(spec['plaque']),
                skirt=soil_tile() if spec['skirt'] == 'soil' else O.rgba(np.array(VL_ashlar(128))[..., :3] * .8))
    tray = tray_soup(W, D, spec)
    for k, t in texs.items():
        atlas.add(k, t, down=1, pad=4)
    tray_UV = np.empty_like(tray['UV'])
    for k in texs:
        m = tray['mat'] == k
        if m.any():
            tray_UV[m] = atlas.remap(k, np.clip(tray['UV'][m], 0.0, 1.0))
    parts_P.append(tray['P'])
    parts_UV.append(tray_UV)
    parts_C.append(L.srgb2lin(np.clip(tray['C'], 0, 1)))
    parts_A.append(np.zeros(tray['C'].shape[:2]))
    P = np.concatenate(parts_P)
    UV = np.concatenate(parts_UV)
    COL = np.concatenate([np.concatenate(parts_C), np.concatenate(parts_A)[..., None]], -1)
    stats['tray_footprint_m'] = [round(D + .07, 3), round(W + .07, 3)]
    stats['scale'] = round(s, 5)
    stats['atlas_fill'] = round(atlas.fill_fraction(4), 3)
    stats['atlas_regions_px'] = {k: [v[2], v[3]] for k, v in atlas.regions.items()}
    return P, UV, COL, atlas, stats


def clip_z(tris, zmax):
    """Cuts everything above zmax (world metres), carrying all columns; the cut is open (the pieces that reach it are trunks and walls)."""
    z = tris[..., 2]
    below = z.max(1) <= zmax
    keep = [tris[below]]
    for i in np.nonzero(~below & (z.min(1) < zmax))[0]:
        poly = L._clip_poly(list(tris[i]), 2, zmax, True)
        if len(poly) >= 3:
            keep.append(np.array([(poly[0], poly[j], poly[j + 1]) for j in range(1, len(poly) - 1)]))
    return np.concatenate(keep)


def VL_ashlar(n):
    return O.VL.ashlar_tile(n, 2.0, seed=181)


def finish(key, spec, P, UV, COL, atlas, stats):
    name = spec['name']
    if spec.get('target') and len(P) > spec['target']:
        img = L.new_image('T_' + name[3:], atlas.img)
        P, UV, COL = LOD.decimate(name, P, UV, COL, img, spec['target'], pin_extremes=.0015, pin_glow=True)
        bpy.ops.wm.read_factory_settings(use_empty=True)
    img = L.new_image('T_' + name[3:], atlas.img)
    out = os.path.join(ROOT, 'Art', spec['asset'], 'r001', name + '.glb')
    LOD.write_lod(name, P, UV, COL, img, out)
    L.save_png(atlas.img, os.path.join(OUT_SRC, f'T_{name[3:]}_Atlas.png'))
    st = R.glb_stats(out)
    st.update(stats, asset_id=spec['asset'], file=os.path.relpath(out, ROOT), glow_vertices_share=round(float((COL[..., 3] > .5).mean()), 4))
    print('DIORAMA', key, st['triangles'], 'tris', st['size_m'])
    return st


def main():
    only = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    metrics = {}
    path = os.path.join(OUT_SRC, 'diorama_metrics.json')
    if os.path.exists(path):
        metrics = json.load(open(path))
    for key, spec in SPECS.items():
        if only and key not in only:
            continue
        import time
        t0 = time.time()
        P, UV, COL, atlas, stats = compose(key, spec)
        print(key, 'composed', len(P), 'tris before decimation', round(time.time() - t0, 1), 's', flush=True)
        metrics[spec['name']] = finish(key, spec, P, UV, COL, atlas, stats)
        print(key, 'finished', round(time.time() - t0, 1), 's', flush=True)
    json.dump(metrics, open(path, 'w'), indent=1)


if __name__ == '__main__':
    main()
