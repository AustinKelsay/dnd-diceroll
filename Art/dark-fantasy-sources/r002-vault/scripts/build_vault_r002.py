"""The Vault r002 world (issue #149): metric world shell + occluder mesh, authored for the HUD-safe camera.

Run from the repository root:   blender -b --python Art/dark-fantasy-sources/r002-vault/scripts/build_vault_r002.py

Writes
  Art/world-vault/r001/SM_World_Vault.glb                        world shell: floor, far hall, low dressing (outside the sight volume)
  Art/world-vault-occluders/r001/SM_World_Vault_Occluders.glb    near architecture that enters the sight volume (dither fade): the walls, pilasters,
                                                                 alcoves, the Waystone apse, sarcophagi, statues, rib falls
  Art/dark-fantasy-sources/r002-vault/world_metrics.json         the measured numbers quoted in the README
  Art/dark-fantasy-sources/r002-vault/T_World_Vault_Atlas.png    the 2048 x 2048 atlas (identical image embedded in both glbs)

Layout (the finale, encounter 5; designed with the back on +Y and the front on -Y, then exported mirrored in Y because the glTF -> Unreal import mirrors Y, so in game the back wall is on the far side of the home camera):
  * the front is open; walls stand 1.8-2.8 cells outside the largest board (18 x 12) at the back (+Y) and sides (+-X) and run out as
    broken, declining runs toward the front;
  * the Waystone landmark stands in an apse in the middle of the back wall, on a three-step dais;
  * braziers (the game's own ring, 1.25 cells out) stay clear of the wall pilasters; dressing goes 2-5 cells out;
  * the playable footprint (27 x 18 m, the 16 x 11 board sits inside it) holds nothing above 10 cm.
Everything is original and procedural; nothing from r001's Vigil, nothing outside the repository.  Units: metres, 1 cell = 1.5 m, +X right,
+Y away from the home camera, Z up.  COLOR_0 is RGBA: RGB = baked light, A = glow mask (alpha 1 only on the cold-blue rune and crystal
materials, 0 elsewhere).
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import r002_lib as R
from r002_lib import L, bpy, np, Vector, make_bvh, tri_normals, smoothstep, fbm, vnoise, subdivide, bake_ao, tri_soup, edge_lengths
import camera_volume as CV
import paving_vault as PV
import vault_lib as VL
from vault_lib import T, Rz, Rx, Ry
from prims import Soup

ROOT = R.ROOT
OUT_WORLD = os.path.join(ROOT, 'Art', 'world-vault', 'r001')
OUT_OCC = os.path.join(ROOT, 'Art', 'world-vault-occluders', 'r001')
OUT_SRC = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r002-vault')

FX, FY, MARGIN = CV.FX, CV.FY, CV.MARGIN       # 13.5 x 9 m half extents, half-cell margin
MX, MY = FX + MARGIN, FY + MARGIN
WALL_X, WALL_Y, WALL_T = 16.5, 12.1, 1.5          # inner faces of the side and back walls, thickness
ZONE = 5.5                                       # paving extends this far beyond the margin (irregular fade after)
EXTENT = 48.0                                    # ground half extent
ATLAS = 2048
PAD = 16
SIGHT_MARGIN = 1.0
GLOW_MATS = VL.GLOW_MATS
GLOW_LIGHT = 0.5
BASE_Z = -.05
FLOOR_TILE_M = 2.0
rng = np.random.default_rng(1493)
STATS = {}
COUNTS = dict(medium=0, small=0, groups={})
PIECES = []                                      # dict(name, kind, soup)  kind: 'world' | 'near'

VOL = CV.Volume(margin=SIGHT_MARGIN)


# ------------------------------------------------------------------ field functions (metres)
def rect_dr(x, y):
    x, y = np.asarray(x, float), np.asarray(y, float)
    return np.hypot(np.maximum(0, np.abs(x) - MX), np.maximum(0, np.abs(y) - MY))


def hull_cap(x, y):
    zlo, _ = VOL.z_interval(np.atleast_1d(x), np.atleast_1d(y))
    return zlo


def cap_at(x, y, r=0.0):
    pts = [(x, y)] + [(x + r * math.cos(a), y + r * math.sin(a)) for a in np.linspace(0, math.tau, 12, endpoint=False)] if r > 0 else [(x, y)]
    pts = np.array(pts)
    return float(hull_cap(pts[:, 0], pts[:, 1]).min())


def hmax(x, y, r=0.0, extra=.3):
    return cap_at(x, y, r) - SIGHT_MARGIN - extra


def ground_fn(x, y):
    return 0.0


def light_field(x, y):
    """Baked ambient light (RGB multiplier before the shoulder): cold and dim, brighter around the board, a cyan spill from the Waystone,
    darker toward the outer hall."""
    x, y = np.asarray(x, float), np.asarray(y, float)
    dr = rect_dr(x, y)
    board = np.exp(-dr / 16.0)
    v = .52 + .30 * board
    cool = np.array((.84, .93, 1.06))
    spill = np.exp(-(((x) / 8.5) ** 2 + ((y - 13.0) / 6.5) ** 2))[..., None] * np.array((.10, .30, .42))
    return v[..., None] * cool + spill


def light_units(xy):
    xy = np.asarray(xy, float) * PV.SCALE
    return light_field(xy[..., 0], xy[..., 1])


# ------------------------------------------------------------------ small soup utilities
def finish_piece(sp_or_soup, name, kind='world', group=None, size=None):
    s = sp_or_soup.soup() if hasattr(sp_or_soup, 'soup') else sp_or_soup
    if not len(s['P']):
        return
    PIECES.append(dict(name=name, kind=kind, soup=s))
    if group:
        COUNTS['groups'][group] = COUNTS['groups'].get(group, 0) + 1
    if size in ('medium', 'small'):
        COUNTS[size] += 1


def shade_vertical(s, z0, height, lo=.55, hi=1.0):
    P = s['P']
    N = tri_normals(P)
    lf = light_field(P[..., 0], P[..., 1])
    g = lo + (hi - lo) * np.clip((P[..., 2] - z0) / max(height, 1e-3), 0, 1)
    up = (.88 + .12 * np.clip(N[:, 2], -1, 1))[:, None]
    s['C'] = lf * (g * up)[..., None]
    return s


def place(s, x, y, z=0.0, yaw=0.0):
    return L.xform(s, T(x, y, z) @ Rz(yaw))


def glow_to_one(s):
    gl = np.isin(s['mat'], GLOW_MATS)
    if s['C'] is not None:
        s['C'][gl] = 1.0
    return s


# ------------------------------------------------------------------ architecture in a canonical wall frame
# local frame: the wall runs along +x, its inner face is y = 0 (the board is on the -y side) and its body extends to y = WALL_T.
def niche_contents(r, cx, kind):
    """What stands in an alcove (local frame: the niche floor is z 0.85 after the plinth; the back is at y = 0.6)."""
    sp = Soup()
    shelf_z = .85
    sp.box('stone', (cx, .32, shelf_z + .03), (1.5, .7, .06), tile=1.0, top='stone', bottom=False)
    if kind == 'statue':
        merge = VL.merge
        merge(sp, VL.hooded_statue(r, (cx, .3, shelf_z + .06), h=1.85, yaw=-90))
        COUNTS['medium'] += 1
    elif kind == 'ossuary':
        for row in range(3):
            for k in range(5):
                merge = VL.merge
                merge(sp, VL.skull(r, (cx - .6 + k * .3 + r.uniform(-.03, .03), .3, shelf_z + .06 + row * .24), scale=r.uniform(.9, 1.1), yaw=-90 + r.uniform(-25, 25)))
                COUNTS['small'] += 1
        sp.box('stone', (cx, .32, shelf_z + .27), (1.5, .6, .035), tile=1.0, top='stone', bottom=False)
        sp.box('stone', (cx, .32, shelf_z + .51), (1.5, .6, .035), tile=1.0, top='stone', bottom=False)
    else:                                                   # a reliquary: urns, a skull, candles and a glowing crystal
        merge = VL.merge
        merge(sp, VL.urn(r, (cx - .45, .34, shelf_z + .06), h=r.uniform(.5, .8), r=r.uniform(.2, .26), yaw=r.uniform(0, 360)))
        merge(sp, VL.urn(r, (cx + .42, .3, shelf_z + .06), h=r.uniform(.4, .6), r=r.uniform(.17, .22), mat='iron', yaw=r.uniform(0, 360)))
        merge(sp, VL.crystal_cluster(r, (cx, .34, shelf_z + .06), h=r.uniform(.45, .65), n=4, yaw=r.uniform(0, 360)))
        merge(sp, VL.skull(r, (cx + .05, .55, shelf_z + .06), yaw=-90))
        for k in range(3):
            merge(sp, VL.candle_stub(r, (cx - .22 + k * .26, .16 + r.uniform(0, .1), shelf_z + .06)))
        COUNTS['medium'] += 2
        COUNTS['small'] += 5
    return sp.soup()


def wall_local(r, a0, a1, hfun, niches=(), apse=None, thick=WALL_T, seg=1.4, kinds=None, ext=0.0):
    """A broken ashlar wall run in the canonical frame.  niches: x centres of 1.5 m wide alcoves; apse: half width of a recess left open
    for the Waystone.  Returns (soup, list of (x, top))."""
    sp = Soup()
    holes = sorted([(c - .75, c + .75, 'n', c) for c in niches] + ([(-apse, apse, 'a', 0.0)] if apse else []))
    cur = a0
    solid = []
    for lo, hi, _k, _c in holes:
        if lo > cur:
            solid.append((cur, lo))
        cur = max(cur, hi)
    if a1 > cur:
        solid.append((cur, a1))
    tops = []
    for lo, hi in solid:
        n = max(1, int(round((hi - lo) / seg)))
        for i in range(n):
            x0, x1 = lo + (hi - lo) * i / n, lo + (hi - lo) * (i + 1) / n
            h = max(.3, hfun((x0 + x1) / 2) + r.uniform(-.2, .2))
            VL.ashlar_block(sp, x0, x1, 0.0, thick, 0.0, h)
            if h > 3.0:                                    # a cornice lip on the standing parts
                sp.box('stone', ((x0 + x1) / 2, thick / 2 - .02, h - .1), (x1 - x0 + .06, thick + .22, .2), tile=1.0, top='stone', bottom=False)
            tops.append(((x0 + x1) / 2, h))
    ni = -1
    for k, (lo, hi, kind, c) in enumerate(holes):
        if kind != 'n':
            continue
        ni += 1
        h = max(4.3, hfun(c))
        VL.ashlar_block(sp, lo, hi, 0.0, thick, 0.0, .85)                              # plinth under the alcove
        VL.ashlar_block(sp, lo, hi, .6, thick, .85, 3.35)                              # the alcove's back
        VL.convex_prism(sp, 'ashlar', [(lo, 3.35), (hi, 3.35), (hi, 3.65), (c, 4.2), (lo, 3.65)], 0.0, thick, tile=2.0, mat_cap='stone')
        if h > 4.25:
            VL.ashlar_block(sp, lo, hi, 0.0, thick, 4.0, h)
        sp.box('rune', (c, -.012, 3.42), (1.0, .022, .03), tile=1.0, bottom=False)        # a short cold-blue keystone line
        kind_ = kinds[ni] if kinds else 'relic'
        VL.merge(sp, niche_contents(r, c, kind_))
    return sp.soup(), tops


def pilasters_local(r, xs, hfun):
    out = Soup()
    for x in xs:
        h = min(hfun(x) - .15, 4.35)
        if h < 1.8:
            continue
        VL.merge(out, VL.pilaster(None, x, 0.0, -90, h))       # projects toward -y (the board side)
    return out.soup()


def waystone_local(r):
    """The Waystone and its apse (canonical back-wall frame, centred on x = 0): a three-step dais, a recess in the wall, the tapered monolith
    with cold-blue rune channels, a pyramidion; hooded statues kneel at the foot of the steps."""
    sp = Soup()
    ap = 2.3
    # apse shell behind the wall: side masses, back slab, lintel gable over the recess
    VL.ashlar_block(sp, ap, ap + 1.2, 0.0, 3.1, 0.0, 5.3)
    VL.ashlar_block(sp, -ap - 1.2, -ap, 0.0, 3.1, 0.0, 5.3)
    VL.ashlar_block(sp, -ap, ap, 2.4, 3.1, 0.0, 6.0)
    VL.convex_prism(sp, 'ashlar', [(-ap - 1.2, 5.3), (ap + 1.2, 5.3), (ap, 6.0), (0, 6.35), (-ap, 6.0)], 2.4, 3.1, tile=2.0, mat_cap='stone')
    VL.convex_prism(sp, 'ashlar', [(-ap, 4.0), (ap, 4.0), (ap, 4.5), (0, 5.4), (-ap, 4.5)], 0.0, WALL_T, tile=2.0, mat_cap='stone')
    VL.ashlar_block(sp, -ap, ap, 0.0, WALL_T, 4.0, 4.4)
    # dais: three steps toward the board
    for k, (w, y0, z1) in enumerate(((3.2, -1.1, .2), (2.8, -.7, .4), (2.4, -.3, .6))):
        sp.box('stone', (0, (y0 + 2.4) / 2, z1 / 2), (2 * w, 2.4 - y0, z1), tile=1.0, top='stone', bottom=False)
    # monolith
    c = (0.0, 1.45)
    rings = [(c[0], c[1], .6, .95, .62), (c[0], c[1], 2.4, .88, .56), (c[0], c[1], 4.6, .74, .48), (c[0], c[1], 5.65, .62, .4), (c[0], c[1], 6.0, .2, .14)]
    sp.loft('basalt', rings, 4, patch=(0, 0, 3, 8), phase=45, cap=(True, True))
    hw = lambda z: np.interp(z, [.6, 2.4, 4.6, 5.65], [.62, .56, .48, .4]) / math.sqrt(2)           # y half width of the tapered face
    for (cx, z0, z1, w) in ((0, 1.3, 5.0, .12), (-.34, 1.7, 3.6, .07), (.34, 1.7, 3.6, .07), (0, 5.15, 5.5, .3)):
        zc = (z0 + z1) / 2
        sp.box('rune', (cx, c[1] - hw(zc) - .012, zc), (w, .035, z1 - z0), tile=1.0, bottom=False)
    for z in (1.0, 2.2, 3.4, 4.6):                                      # carved bands (stone) around the shaft
        sp.box('stone', (0, c[1], z), (2 * hw(z) * math.sqrt(2) * .72 + .18, 2 * hw(z) + .12, .12), tile=1.0, top='stone', bottom=False)
    # a halo of twelve rune segments floating in front of the monolith's head: the landmark's far-zoom signature
    for k in range(12):
        a = math.tau * k / 12
        seg = Soup()
        seg.box('rune', (0, 0, 0), (.62, .07, .09), tile=1.0, top='rune', bottom=True)
        VL.merge(sp, L.xform(seg.soup(), T(1.5 * math.cos(a), c[1] - .85, 4.3 + 1.5 * math.sin(a)) @ Ry(math.degrees(a) + 90)))
    # rune ring inlaid in the top step
    for k in range(16):
        a = math.tau * k / 16
        sp.box('rune', (1.55 * math.cos(a) * 1.2, 1.2 + 1.2 * math.sin(a) * .9, .612), (.28, .06, .012), tile=1.0, bottom=False)
    s = sp.soup()
    # kneeling hooded statues on the second step
    for sx in (-1, 1):
        VL.merge(sp, VL.hooded_statue(r, (sx * 1.9, .35, .4), h=1.6, yaw=-90 + sx * 14))
        COUNTS['medium'] += 1
    COUNTS['medium'] += 1
    return sp.soup()


# ------------------------------------------------------------------ the near architecture, placed
def near_architecture():
    r = np.random.default_rng(2041)
    # ---- back wall (+Y): local frame origin (0, WALL_Y), yaw 0
    hb = lambda x: 4.6 - 1.7 * float(smoothstep(.55, 1.0, abs(x) / 17.8)) - .5 * float(vnoise(np.array([x]), np.array([0.0]), .35, 7)[0])
    niche_b = (-12.6, -7.7, 7.7, 12.6)
    sp, tops = wall_local(r, -17.9, 17.9, hb, niches=niche_b, apse=2.3, kinds=['relic', 'ossuary', 'statue', 'relic'])
    s = L.cat(sp, waystone_local(r), pilasters_local(r, (-15.4, -10.2, -5.3, 5.3, 10.2, 15.4), hb))
    s = place(s, 0.0, WALL_Y)
    s = shade_vertical(s, 0.0, 5.0, lo=.62, hi=1.0)
    finish_piece(s, 'wallB', 'near', 'architecture')
    COUNTS['groups']['waystone'] = 1
    COUNTS['groups']['reliquary_alcoves'] = 0
    # ---- side walls: the run declines toward the front; origin at the wall's inner face, yaw -90 (right) / +90 (left)
    def hs_right(x):          # local x = -global y for the right wall: x from -13.6 (back) to +1.5 (front end)
        yy = -x
        base = 4.6 if yy > 4.0 else 4.6 - 2.4 * float(smoothstep(4.0, -1.8, yy))
        return base - .5 * float(vnoise(np.array([yy]), np.array([5.0]), .4, 9)[0])
    for side, yaw, kinds in ((1, -90, ['statue', 'relic']), (-1, 90, ['relic', 'ossuary'])):
        if side == 1:
            a0, a1, fn, niches = -13.6, 1.7, hs_right, (-8.6, -3.8)
            pil = (-11.4, -6.2, -1.2)
        else:
            a0, a1, fn, niches = -1.7, 13.6, (lambda x: hs_right(-x)), (3.8, 8.6)
            pil = (1.2, 6.2, 11.4)
        sp, tops = wall_local(r, a0, a1, fn, niches=niches, kinds=kinds)
        s = L.cat(sp, pilasters_local(r, pil, fn))
        s = place(s, side * WALL_X, 0.0, 0.0, yaw)
        s = shade_vertical(s, 0.0, 5.0, lo=.62, hi=1.0)
        finish_piece(s, 'wallR' if side == 1 else 'wallL', 'near', 'architecture')
    COUNTS['groups']['reliquary_alcoves'] = 8
    # ---- corbel stubs: broken vault ribs springing from the pilaster capitals (they project about one metre, inside the wall zone)
    for x in (-15.4, -10.2, -5.3, 5.3, 10.2, 15.4):
        h = min(hb(x) - .15, 4.35)
        if h < 2.4:
            continue
        sp = Soup()
        VL.rib_arc(sp, (0, 0, 0), (1.0, 0, 0), .35, width=.34, depth=.4, n=3, broken=.7)
        s = place(sp.soup(), x, WALL_Y - .35, h - .45, -90)
        s = shade_vertical(s, 3.0, 1.5, lo=.8, hi=1.0)
        finish_piece(s, f'ribstub{int(x)}', 'near', 'collapsed_rib')
        STUBS.append((x, h - .45))


STUBS = []


def near_dressing():
    """Low dressing in the zone two to five cells outside the board, at the open front and in the front corners: sarcophagus rows, a
    collapsed vault rib, drums, chains, rubble, bones.  Pieces that rise into the sight volume are occluders (dither-faded)."""
    r = np.random.default_rng(2042)
    # sarcophagus rows in the front corners, long axis along Y (parallel to the vanished wall line)
    k = 0
    for sx in (-1, 1):
        for yy in (-4.6, -7.9, -11.2):
            sp = Soup()
            s_ = VL.sarcophagus(r, w=.95, l=2.2, h=.82, effigy=True, cracked=(k % 3 == 1))
            x = sx * (18.2 + r.uniform(-.2, .2))
            s = place(s_, x, yy + r.uniform(-.2, .2), 0.0, 90 + r.uniform(-6, 6))
            s = shade_vertical(s, 0.0, 1.0, lo=.62, hi=1.0)
            finish_piece(s, f'sarc{k}', 'near', 'sarcophagi', 'medium')
            k += 1
    # open sarcophagus with its lid slid aside (back left), a chest and urns among them
    sp = Soup()
    VL.merge(sp, VL.sarcophagus(r, w=1.0, l=2.3, h=.82, effigy=False))
    for g, (x, y, yaw) in enumerate(((-21.0, 3.5, 90), (21.0, 3.5, 90))):
        s = place(sp.soup(), x, y, 0, yaw)
        s = shade_vertical(s, 0, 1.0, lo=.62, hi=1.0)
        finish_piece(s, f'sarcB{g}', 'near', 'sarcophagi', 'medium')
    for g, (x, y, yaw) in enumerate(((-18.6, -2.0, 20), (19.0, -13.5, -35))):
        s = VL.chest(r, (x, y, 0), yaw)
        finish_piece(shade_vertical(s, 0, .6, lo=.7, hi=1.0), f'chest{g}', 'near', 'sarcophagi', 'medium')
    for g in range(8):
        sx = -1 if g % 2 == 0 else 1
        x, y = sx * r.uniform(16.9, 21.0), r.uniform(-12.8, -1.0)
        s = VL.urn(r, (x, y, 0), h=r.uniform(.5, .9), r=r.uniform(.2, .3), mat='stone' if g % 3 else 'iron', yaw=r.uniform(0, 360))
        finish_piece(shade_vertical(s, 0, .9, lo=.65, hi=1.0), f'urnF{g}', 'near', 'reliquary_dressing', 'medium')
    # the collapsed vault rib: three fallen arc segments and a capital drum, lying across the front-left corner
    sp = Soup()
    base = np.array((-19.5, -6.8, 0.0))
    for i, (dx, dy, yaw, ln) in enumerate(((0, 0, 28, 2.6), (2.1, 1.3, 36, 2.2), (4.0, 3.0, 62, 1.9))):
        blk = Soup()
        blk.box('stone', (0, 0, 0), (ln, .5, .52), tile=1.0, top='stone', bottom=True)
        VL.merge(sp, L.xform(blk.soup(), T(base[0] + dx, base[1] + dy, .27) @ Rz(yaw) @ Rx(r.uniform(-6, 6))))
    VL.merge(sp, VL.column_drum(r, (base[0] - 1.2, base[1] - .8), r=.5, h=.6, yaw=20, lean=0))
    VL.rubble_heap(sp, r, (base[0] + 1.5, base[1] + 1.2), r=1.4, h=.5, n=8)
    s = shade_vertical(sp.soup(), 0, .9, lo=.6, hi=1.0)
    finish_piece(s, 'ribfall0', 'near', 'collapsed_rib', 'medium')
    # drums, a snapped column and rubble across the front
    for g in range(6):
        x = r.uniform(-20, 20)
        y = r.uniform(-14.8, -12.3)
        s = VL.column_drum(r, (x, y), r=r.uniform(.4, .55), h=r.uniform(.5, .8), yaw=r.uniform(0, 360), lean=r.choice([0, 90]))
        finish_piece(shade_vertical(s, 0, 1.0, lo=.62, hi=1.0), f'drumF{g}', 'near', 'collapsed_rib', 'medium')
    # chains lying in loose runs between sarcophagi and hanging from the pilasters' corbels
    for g in range(5):
        sx = -1 if g % 2 == 0 else 1
        pts = []
        x0, y0 = sx * r.uniform(17.0, 19.5), r.uniform(-11, -2)
        for i in range(7):
            pts.append((x0 + r.uniform(-.25, .25) * (i > 0) + sx * .2 * i, y0 - .4 * i + r.uniform(-.2, .2), .035 + (0.0 if i else .0)))
        sp = Soup()
        VL.chain_run(sp, pts, link=.075)
        finish_piece(shade_vertical(sp.soup(), 0, .2, lo=.7, hi=1.0), f'chainF{g}', 'near', 'chains', 'small')
    # hanging chains from the back-wall corbels, down the face toward the board side (they stay in the wall zone)
    for g, (x, z0) in enumerate(STUBS):
        pts = [(x + .1, WALL_Y - .5, z0 - .02 - .3 * i) for i in range(max(3, int((z0 - .5) / .3)))]
        sp = Soup()
        VL.beaded_chain(sp, pts, bead=.04)
        s = shade_vertical(sp.soup(), 0, 4.0, lo=.62, hi=1.0)
        finish_piece(s, f'chainH{g}', 'near', 'chains', 'small')


def small_scatter():
    """Bones, skulls, candle stubs, crystal chips and rubble, 'world' kind (low): 100+ small props on the floor outside the board."""
    r = np.random.default_rng(2043)
    n_small = 0
    def ok(x, y, lo=2.0, hi=24.0):
        d = float(rect_dr(x, y))
        in_wall = (16.3 < abs(x) < 18.2 and y > -1.9) or (11.9 < y < 13.8 and abs(x) < 18.2) or (y > 11.9 and abs(x) < 3.6 and y < 15.4)
        return lo <= d <= hi and not in_wall
    tries = 0
    while n_small < 215 and tries < 3000:
        tries += 1
        a = r.uniform(0, math.tau)
        d = r.uniform(12, 27)
        x, y = d * math.cos(a) * 1.35, d * math.sin(a) * 1.0
        if abs(x) > 24 or y > 23 or y < -16 or not ok(x, y):
            continue
        kind = r.choice(['skull', 'bone', 'bone', 'rib', 'candle', 'candle', 'chip', 'rubble'])
        if kind == 'skull':
            s = VL.skull(r, (x, y, .03), scale=r.uniform(.9, 1.15), yaw=r.uniform(0, 360))
        elif kind == 'bone':
            s = VL.bone_long(r, (x, y, .03), length=r.uniform(.22, .42), yaw=r.uniform(0, 360), lean=r.uniform(-8, 8))
        elif kind == 'rib':
            s = VL.rib_cage(r, (x, y, .02), yaw=r.uniform(0, 360))
        elif kind == 'candle':
            s = VL.candle_stub(r, (x, y, .03), h=r.uniform(.06, .2))
        elif kind == 'chip':
            sp = Soup()
            VL.add_rock(sp, 'crystal', r, (x, y, .05), r.uniform(.05, .09), (1, 1, 1.5), tile=.3, detail=0, floor=.0)
            s = sp.soup()
        else:
            sp = Soup()
            VL.rubble_heap(sp, r, (x, y), r=.35, h=.25, n=3)
            s = sp.soup()
        finish_piece(shade_vertical(s, 0, .3, lo=.7, hi=1.0), f'small{n_small}', 'world', 'small_scatter', 'small')
        n_small += 1


# ------------------------------------------------------------------ the far hall (world shell)
def far_hall():
    r = np.random.default_rng(2044)
    HX0, HX1, HY0, HY1 = -31.0, 31.0, -36.0, 30.0
    # perimeter walls with buttresses, lancet windows glowing cold blue (the light from outside)
    runs = (('back', 0, (0.0, HY1 - 1.0), -31.0, 31.0), ('front', 180, (0.0, HY0 + 1.0), -31.0, 31.0),
            ('right', -90, (HX1 - 1.0, 0.0), -HY1, -HY0), ('left', 90, (HX0 + 1.0, 0.0), HY0, HY1))
    for name, yaw, origin, a0, a1 in runs:
        sp = Soup()
        length = a1 - a0
        segs = int(length / 3.0)
        M = T(origin[0], origin[1], 0) @ Rz(yaw)
        for i in range(segs):
            x0, x1 = a0 + i * length / segs, a0 + (i + 1) * length / segs
            xc = (x0 + x1) / 2
            gx, gy, _, _ = M @ np.array((xc, 0.0, 0.0, 1.0))
            h = min(9.0, hmax(float(gx), float(gy), 2.0))
            if h < 3.0:
                continue
            h = max(3.2, h - r.uniform(0, 1.8))
            VL.ashlar_block(sp, x0, x1, 0.0, 2.0, 0.0, h)
            sp.box('stone', (xc, 1.0, h - .12), (x1 - x0 + .1, 2.3, .24), tile=1.0, top='stone', bottom=False)
            if i % 2 == 0:
                VL.ashlar_block(sp, xc - .45, xc + .45, -1.0, 0.0, 0.0, h * .8)       # buttress
                if h > 6:
                    sp.box('rune', (xc, -.012, h * .52), (.5, .03, min(4.5, h * .55)), tile=1.0, bottom=False)       # lancet glow
        s = L.xform(sp.soup(), M)
        s = shade_vertical(s, 0, 8.0, lo=.55, hi=1.0)
        finish_piece(s, f'hall{name}', 'world', 'far_hall')
    # broken piers in a loose ring, capped under the sight volume
    for g in range(16):
        a = math.tau * g / 16 + r.uniform(-.1, .1)
        d = r.uniform(23, 27)
        x, y = d * math.cos(a) * 1.15, d * math.sin(a)
        y = float(np.clip(y, -28, 25))
        if abs(x) > 28:
            continue
        h = min(r.uniform(5.5, 9.0), hmax(x, y, 1.0))
        if h < 2.2:
            continue
        sp = Soup()
        sp.box('stone', (0, 0, .25), (1.7, 1.7, .5), tile=1.0, top='stone', bottom=False)
        sp.lathe('ashlar', (0, 0, .5), [(.62, 0), (.55, h * .3), (.5, h * .8 - .5)], 9, tile=1.5, cap=(False, False))
        sp.box('stone', (0, 0, h - .2), (1.2, 1.2, .35), tile=1.0, top='stone', bottom=False)
        s = shade_vertical(sp.soup(), 0, h, lo=.5, hi=.95)
        finish_piece(place(s, x, y), f'pier{g}', 'world', 'far_hall')


# ------------------------------------------------------------------ floor
ao_bvh_holder = {}


def build_ground():
    PV.configure(MX + ZONE, MY + ZONE, (MX, MY), (3.2, ZONE - .2))
    stones = PV.build_paving(light_units, ao_bvh_holder['bvh'])
    stones = L.xform(stones, L.S(PV.SCALE))
    for k, v in PV.STATS.items():
        STATS[k] = v
    n = int(EXTENT // 8)
    tris = []
    for i in range(-n, n):
        for j in range(-n, n):
            x0, y0 = i * 8.0, j * 8.0
            q = [(x0, y0), (x0 + 8, y0), (x0 + 8, y0 + 8), (x0, y0 + 8)]
            for t in ((0, 1, 2), (0, 2, 3)):
                tris.append([(q[k][0], q[k][1], 0.0) for k in t])
    s = tri_soup(tris, [[(0, 0)] * 3] * len(tris), np.ones((len(tris), 3, 3)), 'floor')

    def lim(mid):
        d = rect_dr(mid[..., 0], mid[..., 1])
        return 1.42 * np.where(d < 11.0, .75, np.where(d < 24.0, 1.5, 3.0))
    s = subdivide(s, lim)
    P = s['P'].copy()
    P[..., 2] = BASE_Z
    s['P'] = P
    f = tri_normals(P)[:, 2] < 0
    s['P'][f] = s['P'][f][:, [0, 2, 1]]
    ct, st_ = math.cos(math.radians(21)), math.sin(math.radians(21))
    uv = np.empty((len(P), 3, 2))
    dr = rect_dr(P[..., 0], P[..., 1])
    sc = FLOOR_TILE_M + 2.0 * smoothstep(14.0, 40.0, dr)
    uv[..., 0] = (P[..., 0] * ct - P[..., 1] * st_) / sc
    uv[..., 1] = (P[..., 0] * st_ + P[..., 1] * ct) / sc
    s['UV'] = uv
    xs, ys = P[..., 0], P[..., 1]
    damp = smoothstep(.45, .72, fbm(xs * .5 + 7, ys * .5 - 3, 5))
    tint = np.array((1.0, 1.02, 1.08)) * (1 - damp[..., None]) + np.array((.78, .92, 1.12)) * damp[..., None]
    shade = np.where(dr < 0.2, .9, 1.0)[..., None]
    s['C'] = light_field(xs, ys) * tint * shade
    STATS['base_tris'] = int(len(s['P']))
    return stones, s


def inlays():
    """Cold-blue inlaid rune strips set flush in the floor: a skirting line along the back wall and a ring around the dais.  Outside the board."""
    sp = Soup()
    z = .047
    def strip(x0, y0, x1, y1, w=.08):
        d = np.array((x1 - x0, y1 - y0)); ln = np.linalg.norm(d)
        n = np.array((-d[1], d[0])) / ln * w / 2
        a, b, c, e = (x0 - n[0], y0 - n[1], z), (x1 - n[0], y1 - n[1], z), (x1 + n[0], y1 + n[1], z), (x0 + n[0], y0 + n[1], z)
        sp.quad('rune', a, b, c, e, (.5, .5), (.5, .5), (.5, .5), (.5, .5))
    # ring of the dais: 24 short chords at radius 4.4 about the apse mouth (the dais is the pivot of the whole room)
    for k in range(24):
        if k % 3 == 2:
            continue
        a0, a1 = math.tau * k / 24, math.tau * (k + .8) / 24
        x0, y0 = 4.4 * math.cos(a0) * 1.25, WALL_Y + 1.4 + 4.4 * math.sin(a0) * .8
        x1, y1 = 4.4 * math.cos(a1) * 1.25, WALL_Y + 1.4 + 4.4 * math.sin(a1) * .8
        if y0 > WALL_Y and y1 > WALL_Y:
            continue
        if min(y0, y1) < MY - .05 + .8:
            continue
        strip(x0, y0, x1, y1)
    s = sp.soup()
    s['C'] = np.ones((len(s['P']), 3, 3))
    return s


# ------------------------------------------------------------------ classification against the sight volume
def classify():
    world, occ = [], []
    inside_counts = {}
    for p in PIECES:
        P = p['soup']['P']
        pts = np.concatenate([P.reshape(-1, 3), P.mean(1), ((P + P[:, [1, 2, 0]]) / 2).reshape(-1, 3)])
        n_in = int((VOL.contains(pts) & (pts[:, 2] > .15)).sum())
        if n_in and p['kind'] == 'near':
            occ.append(p)
            inside_counts[p['name']] = n_in
        elif n_in:
            STATS.setdefault('pieces_dropped_for_sight_volume', []).append(p['name'])
        else:
            world.append(p)
    return world, occ, inside_counts


def main():
    os.makedirs(OUT_WORLD, exist_ok=True)
    os.makedirs(OUT_OCC, exist_ok=True)
    os.makedirs(OUT_SRC, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    near_architecture()
    near_dressing()
    small_scatter()
    far_hall()
    kept, dropped = [], []
    for p in PIECES:
        P = p['soup']['P']
        c = np.concatenate([P.reshape(-1, 3), P.mean(1)])
        if ((np.abs(c[:, 0]) < FX + .25) & (np.abs(c[:, 1]) < FY + .25) & (c[:, 2] > .10)).any():
            dropped.append(p['name'])
        else:
            kept.append(p)
    PIECES[:] = kept
    STATS['pieces_dropped_for_footprint'] = dropped
    world_p, occ_p, inside = classify()
    print('pieces', len(PIECES), 'world', len(world_p), 'occluders', len(occ_p), inside)
    arch = L.cat(*[p['soup'] for p in PIECES if len(p['soup']['P'])])
    ao_bvh_holder['bvh'] = make_bvh(arch['P'])
    stones, base = build_ground()
    texs = VL.vault_textures()
    inl = inlays()
    # ---- subdivide the near architecture to give vertex light a gradient, then AO
    bvh_all = make_bvh(L.cat(stones, arch, base)['P'])
    ao_total = 0
    for p in PIECES:
        s = p['soup']
        if p['kind'] == 'near':
            s = subdivide(s, 1.0)
            p['soup'] = s
        if s['C'] is None:
            continue
        mid = s['P'].mean(1)
        if float(rect_dr(mid[:, 0].mean(), mid[:, 1].mean())) < 24.0 and len(s['P']) < 40000:
            s2, nao = bake_ao(s, bvh_all, strength=.55, dist=2.0, n=8)
            p['soup'] = s2
            ao_total += nao
    near_base = rect_dr(base['P'].mean(1)[:, 0], base['P'].mean(1)[:, 1]) < 14.0
    b_n, b_f = L.sub(base, near_base), L.sub(base, ~near_base)
    b_n, nb = bake_ao(b_n, bvh_all, strength=.35, dist=2.0, n=8)
    ao_total += nb
    base = L.cat(b_n, b_f)
    STATS['ao_samples'] = ao_total
    world_pieces = [p for p in PIECES if p in world_p]
    occ_pieces = [p for p in PIECES if p in occ_p]
    world = L.cat(stones, base, inl, *[p['soup'] for p in world_pieces])
    occl = L.cat(*[p['soup'] for p in occ_pieces]) if occ_pieces else None
    import re as _re
    grp = {}
    for p in PIECES:
        k = _re.sub(r'[0-9]+(_[0-9]+)?', '', p['name'])
        grp[k] = grp.get(k, 0) + len(p['soup']['P'])
    STATS['source_tris_by_group'] = grp
    STATS['pieces'] = dict(total=len(PIECES), world=len(world_pieces), occluders=len(occ_pieces), occluder_names=[p['name'] for p in occ_pieces])
    SECONDARY = ('reliquary_alcoves', 'sarcophagi', 'collapsed_rib', 'chains')
    STATS['content_counts'] = dict(landmark=1, secondary_groups=list(SECONDARY), groups=COUNTS['groups'], medium_props=COUNTS['medium'], small_props=COUNTS['small'],
                                       note='the small props are batched into the SM_World_Vault export as ordinary triangles (baked, one mesh); they are not runtime instances, and the game adds its own instanced scatter on top')
    for d in [x for x in (world, occl) if x is not None]:
        assert np.isfinite(d['C']).all(), 'non-finite vertex colour'
        d['C'] = L.knee(d['C'] * 1.05)
        gl = np.isin(d['mat'], GLOW_MATS)
        d['C'][gl] = GLOW_LIGHT        # not 1.0: with the far-zoom exposure lift, full-light rune bars clip to pure white in the game (apse peak luma, PR #197 review)
    # The glTF -> Unreal import mirrors Y (Blender +Y lands on the game's -Y, the camera's home side), so the design above (back wall and
    # Waystone on +Y) is exported mirrored: in game the Waystone stands on the far side of the home camera.  X is unchanged.
    MIRROR_Y = np.diag([1.0, -1.0, 1.0, 1.0])
    world = L.xform(world, MIRROR_Y)
    occl = L.xform(occl, MIRROR_Y) if occl is not None else None
    mats_all = list(dict.fromkeys(world['mat'].tolist() + (occl['mat'].tolist() if occl is not None else [])))
    dummy = tri_soup([[(0, 0, -9999), (1, 0, -9999), (0, 1, -9999)]] * len(mats_all), [[(.5, .5), (.51, .5), (.5, .51)]] * len(mats_all),
                     [[(1, 1, 1)] * 3] * len(mats_all), 'x')
    dummy['mat'] = np.array(mats_all, dtype=object)
    solid = [k for k in ('rune', 'crystal') if k in mats_all]
    A = None
    outs = []
    for d in (world, occl):
        if d is None:
            outs.append(None)
            continue
        d2, A_, place_ = L.build_atlas(L.cat(dummy, d), texs, size=ATLAS, pad=PAD, solid=solid, repeat={})
        keep_ = d2['P'][..., 2].max(1) > -5000
        outs.append(L.sub(d2, keep_))
        if A is not None:
            assert np.array_equal(A, A_), 'atlas differs between meshes'
        A = A_
    world, occl = outs
    used_px = sum((w + 2 * PAD) * (h + 2 * PAD) for (_, _, w, h) in place_.values())
    STATS['atlas'] = dict(size=ATLAS, regions=len(place_), pad_px=PAD, fill_fraction=round(used_px / ATLAS / ATLAS, 3), regions_px={k: [v[2], v[3]] for k, v in place_.items()}, regions_xywh={k: [int(v[0]), int(v[1]), int(v[2]), int(v[3])] for k, v in place_.items()})
    STATS['floor_texels_per_m'] = 512 / FLOOR_TILE_M
    STATS['paving_texels_per_m'] = 256
    bpy.ops.wm.read_factory_settings(use_empty=True)
    img = L.new_image('T_World_Vault_Atlas', A)
    o1, g1 = R.make_object_rgba('SM_World_Vault', world, img, glow_mats=GLOW_MATS, mat_name='M_World_Vault')
    p1 = R.export_glb(o1, os.path.join(OUT_WORLD, 'SM_World_Vault.glb'))
    o2 = g2 = p2 = None
    if occl is not None:
        o2, g2 = R.make_object_rgba('SM_World_Vault_Occluders', occl, img, glow_mats=GLOW_MATS, mat_name='M_World_Vault')
        p2 = R.export_glb(o2, os.path.join(OUT_OCC, 'SM_World_Vault_Occluders.glb'))
    L.save_png(A, os.path.join(OUT_SRC, 'T_World_Vault_Atlas.png'))
    for name, o, g, d, p in (('world', o1, g1, world, p1), ('occluders', o2, g2, occl, p2)):
        if d is None:
            continue
        P_ = d['P']
        dm = rect_dr(P_.mean(1)[:, 0], P_.mean(1)[:, 1])
        zc = P_.mean(1)[:, 2]
        ev = lambda m_: edge_lengths(P_[m_]).ravel() if m_.any() else np.zeros(1)
        e_ground = ev((dm < 8) & (zc < 1.0) & np.isin(d['mat'], ['floor']))
        e_stone = ev(np.isin(d['mat'], ['paveA', 'paveB', 'paveC', 'paveM']))
        e_dress = ev((dm < 14) & (zc >= .2) & ~np.isin(d['mat'], ['floor', 'paveA', 'paveB', 'paveC', 'paveM']))
        STATS[name] = dict(triangles=int(len(o.data.polygons)), glow_tris=g,
                           by_material={k: int((d['mat'] == k).sum()) for k in dict.fromkeys(d['mat'].tolist())},
                           edge_len_m_floor_within_8m_median_p90_max=[round(float(np.median(e_ground)), 3), round(float(np.percentile(e_ground, 90)), 3), round(float(e_ground.max()), 3)],
                           edge_len_m_stone_tops_median_p90=[round(float(np.median(e_stone)), 3), round(float(np.percentile(e_stone, 90)), 3)],
                           edge_len_m_dressing_above_20cm_within_14m_median_p90_max=[round(float(np.median(e_dress)), 3), round(float(np.percentile(e_dress, 90)), 3), round(float(e_dress.max()), 3)],
                           glb=R.glb_stats(p))
    STATS['footprint_m'] = [FX * 2, FY * 2]
    STATS['cell_m'] = 1.5
    STATS['wall_inner_faces_m'] = dict(sides_x=WALL_X, back_y=WALL_Y, cells_outside_18x12=[round((WALL_X - FX) / 1.5, 2), round((WALL_Y - FY) / 1.5, 2)],
                                       cells_outside_16x11=[round((WALL_X - 12.0) / 1.5, 2), round((WALL_Y - 8.25) / 1.5, 2)])
    with open(os.path.join(OUT_SRC, 'world_metrics.json'), 'w') as fh:
        json.dump(STATS, fh, indent=1)
    print('WORLD_METRICS', json.dumps({k: v for k, v in STATS.items() if k not in ('edge_orientation_histogram_10deg_bins_length_share', 'atlas')}))


main()
