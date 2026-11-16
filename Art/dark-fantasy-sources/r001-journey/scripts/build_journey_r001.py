"""The war-table journey map r001 (issue #248, spec #239): thirteen meshes for the route screen's journey map.

  blender -b --python-exit-code 1 --python Art/dark-fantasy-sources/r001-journey/scripts/build_journey_r001.py

Original and procedural, built on the r002 overview helpers (ov_lib, and the war table's prop builders in build_tabletop_r002); no outside content.

Conventions (as the overview set, so the game's placement code turns them the same way): metres, the asset FACES +X (the game turns it -90 degrees about Z
toward the camera), Z up, +Y is the viewer's right, so a table-top point (X, Y) in the game's cm is Blender (-Y / 100, -X / 100).  One material, one
baseColor atlas, COLOR_0 as RGBA: RGB baked light, A glow mask (or, for the fog card, opacity).

  SM_Journey_Table      a clean 6 m war table (no clutter in the middle), two candles at the back corners and a few props at the far edges
  SM_Journey_Map        the 3.65 x 2.32 m vellum chart of the five-stage road, the stage sockets, trailhead and the Vault painted where JourneyLayout.h puts them
  SM_Journey_Stage      a stage token (16 cm bronze disc on an oak foot), neutral so the game tints it per state
  SM_Journey_Emblem_*   the Root (leaf), Vigil (waves) and Vault (rune) medallions that stand on a token, tilted toward the camera
  SM_Journey_Pennant    a victory pennant pinned beside a cleared token
  SM_Journey_Lost       a broken sword driven into a lost token
  SM_Journey_Ink        a walked ink stroke, 1 m along +X from the origin (the game stretches it between stops)
  SM_Journey_InkDashed  the same, dashed: the road not yet walked
  SM_Journey_Fog        a low fog bank (translucent card: opacity in COLOR_0.a) that hides a stage until it is reached
  SM_Journey_Goal       the Vault keep at the end of the road, its Waystone shard glowing
  SM_Journey_Plinth     the oak and brass riser a raised diorama stands on (0.72 x 0.74 x 0.14 m at scale 1)

Outputs: Art/<asset id>/r001/<name>.glb, r001-journey/T_Journey_*.png (map only), r001-journey/journey_metrics.json; positions come from
r001-journey/journey_layout.json (the same numbers as Source/DnDDiceRoll/Public/Presentation/JourneyLayout.h).
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OV_SCRIPTS = os.path.abspath(os.path.join(HERE, '..', '..', 'r002-overview', 'scripts'))
sys.path.insert(0, OV_SCRIPTS)
import ov_lib as O
from ov_lib import L, R, bpy, np, T, Rz, Rx, Ry, Soup, cbox
import build_tabletop_r002 as TT

ROOT = O.ROOT
SRC = os.path.abspath(os.path.join(HERE, '..'))
LAYOUT = json.load(open(os.path.join(SRC, 'journey_layout.json')))


def game_to_blender(x_cm, y_cm):
    """Game table-top cm (X screen-left, Y away from the camera) to Blender metres (X toward the camera, Y to the viewer's right)."""
    return -y_cm / 100.0, -x_cm / 100.0


def add(sp, s):
    TT.add(sp, s)


def light_soup(s, base=.5, top=.5, warm=(1.0, .9, .78)):
    """Simple baked light for small props: a floor of `base` plus `top` times the upward facing, warm-tinted, knee-compressed."""
    n = O.tri_normals(s['P'])
    up = np.clip(n[:, 2], 0, 1) * .75 + np.clip(n[:, 0], 0, 1) * .25        # lit from above and a little from the camera side
    c = (base + top * up)[:, None, None] * np.ones((len(s['P']), 3, 3)) * np.array(warm)
    s['C'] = L.knee(c)
    return s


def export(name, asset_id, s, texs, atlas, glow=(), alpha=None, repeat=None, solid=()):
    s2, A, place = L.build_atlas(s, texs, size=atlas, pad=4, solid=list(solid) + [k for k in texs if texs[k].shape[0] <= 8], repeat=repeat or {})
    if alpha is not None:
        # build_atlas may split triangles across tiles; cards carry no tiled materials, so the triangle order is unchanged.
        if len(s2['P']) != len(alpha):
            raise RuntimeError(f'{name}: atlas changed the triangle count ({len(alpha)} -> {len(s2["P"])})')
    bpy.ops.wm.read_factory_settings(use_empty=True)
    img = L.new_image('T_' + name[3:], A)
    ob, glow_tris = R.make_object_rgba(name, s2, img, alpha=alpha, glow_mats=glow, mat_name='M_' + name[3:])
    path = R.export_glb(ob, os.path.join(ROOT, 'Art', asset_id, 'r001', name + '.glb'))
    st = R.glb_stats(path)
    print('EXPORT', name, st['triangles'], 'tris', st['size_m'], 'glow', glow_tris)
    return dict(st, asset_id=asset_id, file=os.path.relpath(path, ROOT), atlas_px=atlas, glow_tris=glow_tris), A


# ------------------------------------------------------------------ textures
def swatches():
    t = {}
    t['wood'] = O.plank_tile(512, 2.0, 301)
    t['dark'] = O.dark_wood_tile(256, 311)
    t['brass'] = O.metal_tile((.80, .60, .22), 64, 331)
    t['bronze'] = O.metal_tile((.78, .66, .46), 64, 335)
    t['steel'] = O.metal_tile((.66, .68, .72), 64, 332)
    t['iron'] = O.metal_tile((.20, .20, .22), 64, 334, streak=False)
    t['stone'] = O.stone_speckle(64, 321)
    t['slate'] = O.stone_speckle(64, 324, (.20, .21, .25))
    t['wax'] = O.wax_tile(32)
    t['leather'] = O.leather_tile(64, 381)
    t['cloth'] = O.cloth_tile(64, 391, (.45, .10, .09))
    t['bone'] = O.stone_speckle(32, 322, (.80, .76, .64))
    t['flame'] = L.swatch((1.0, .62, .25))
    t['paper'] = L.swatch((.80, .72, .55))
    t['glass'] = L.swatch((.55, .60, .50))
    t['crystal'] = L.swatch((.55, .95, 1.0))
    t['ink'] = L.swatch((.40, .08, .05))
    t['pencil'] = L.swatch((.30, .24, .17))
    t['gilt'] = L.swatch((.95, .78, .40))
    return t


def emblem_texture(kind, n=128, seed=401):
    """A medallion face: enamel field and a gilt or glowing device.  Root a leaf, Vigil three waves under an arch, Vault an angular rune."""
    rng = np.random.default_rng(seed)
    field = {'root': (.10, .26, .10), 'vigil': (.06, .12, .30), 'vault': (.06, .07, .10)}[kind]
    img = np.ones((n, n, 3)) * np.array(field) * (.85 + .3 * O.fbm_tile(n, n, 4, seed, 3)[..., None])
    acc = np.zeros((n, n))
    c = n / 2
    if kind == 'root':
        leaf = [(c, c - .40 * n)]
        for k in range(1, 12):
            t = k / 12
            w = math.sin(math.pi * t) * .22 * n
            leaf.append((c + w, c - .40 * n + t * .80 * n))
        leaf.append((c, c + .40 * n))
        for k in range(11, 0, -1):
            t = k / 12
            w = math.sin(math.pi * t) * .22 * n
            leaf.append((c - w, c - .40 * n + t * .80 * n))
        O.fill_poly(acc, leaf, 1.0)
        vein = np.zeros((n, n))
        O.stroke(vein, [(c, c - .34 * n), (c, c + .44 * n)], 1.2, 1.0)
        for k in range(4):
            y = c - .22 * n + k * .14 * n
            O.stroke(vein, [(c, y + .06 * n), (c - .13 * n, y)], .8, 1.0)
            O.stroke(vein, [(c, y + .06 * n), (c + .13 * n, y)], .8, 1.0)
        O.ink(img, acc, (.92, .76, .36))
        O.ink(img, vein, (.30, .22, .08), .9)
    elif kind == 'vigil':
        for k in range(3):
            y = c + .02 * n + k * .12 * n
            pts = [(c - .34 * n + i * .68 * n / 24, y + math.sin(i / 24 * math.tau * 1.5) * .035 * n) for i in range(25)]
            O.stroke(acc, pts, 2.0, 1.0)
        arch = [(c - .22 * n + .44 * n * i / 30, c - .06 * n - math.sin(math.pi * i / 30) * .28 * n) for i in range(31)]
        O.stroke(acc, arch, 2.2, 1.0)
        O.stroke(acc, [(c - .22 * n, c - .06 * n), (c - .22 * n, c + .02 * n)], 2.2, 1.0)
        O.stroke(acc, [(c + .22 * n, c - .06 * n), (c + .22 * n, c + .02 * n)], 2.2, 1.0)
        O.ink(img, acc, (.86, .90, .96))
    else:
        rune = [[(c, c - .36 * n), (c, c + .36 * n)], [(c, c - .18 * n), (c + .22 * n, c - .36 * n)], [(c, c - .18 * n), (c - .22 * n, c - .36 * n)],
                [(c, c + .06 * n), (c + .24 * n, c + .26 * n)], [(c, c + .06 * n), (c - .24 * n, c + .26 * n)]]
        for seg in rune:
            O.stroke(acc, seg, 2.6, 1.0)
        O.ink(img, acc, (.55, .98, 1.0))
    # a gilt ring at the rim
    ring = np.zeros((n, n))
    for ang in np.linspace(0, math.tau, 160):
        O.stamp(ring, c + .46 * n * math.cos(ang), c + .46 * n * math.sin(ang), 1.6, 1.0)
    O.ink(img, ring, (.90, .72, .32))
    return O.rgba(img)


def cloud_texture(n=128, seed=451):
    f = O.fbm_tile(n, n, 3, seed, 4)
    g = .70 + .45 * f
    return O.rgba(np.stack([g * .96, g * .94, g * .90], -1))


# ------------------------------------------------------------------ the map
MAP_X0, MAP_X1 = LAYOUT['map']['x_cm']           # game X of the left and right edges (screen-left is +X)
MAP_Y0, MAP_Y1 = LAYOUT['map']['y_cm']           # game Y of the far and near edges
MAP_PX = LAYOUT['map']['texture_px']


def map_uv(x_cm, y_cm):
    """Game table-top cm to the map texture's top-down (u, v) in 0..1: u left to right on screen, v far to near."""
    return (MAP_X0 - x_cm) / (MAP_X0 - MAP_X1), (MAP_Y0 - y_cm) / (MAP_Y0 - MAP_Y1)


def journey_map_texture(seed=461):
    """The chart: vellum, a coast and river, the forest the road starts in, the drowned marsh and its spires, the mountains and the crypt of the Vault, the stage sockets,
    the pencilled road through them, a compass rose, a ribboned cartouche (no lettering), a double border, folds and a scorched corner."""
    w, h = MAP_PX
    rng = np.random.default_rng(seed)
    img = O.vellum_base(w, h, seed)
    px = lambda x_cm, y_cm: (map_uv(x_cm, y_cm)[0] * w, map_uv(x_cm, y_cm)[1] * h)
    inkc, red, wash, green = (.16, .10, .06), (.45, .08, .06), (.40, .48, .52), (.14, .26, .12)
    s = w / 2048.0
    nodes = [px(*p) for p in LAYOUT['stages_cm']]
    trail, goal = px(*LAYOUT['trailhead_cm']), px(*LAYOUT['goal_cm'])
    # sea wash along the near-left corner, a river from the mountains down through the marsh to the sea
    acc = np.zeros((h, w))
    coast = [(0, h * .55), (w * .05, h * .58), (w * .09, h * .70), (w * .07, h * .82), (w * .13, h * .92), (w * .12, h), (0, h)]
    O.fill_poly(acc, coast, .55)
    acc *= .75 + .25 * O.pnoise(h, w, 10, 16, seed + 2)
    O.ink(img, acc, wash, .85)
    shore = np.zeros((h, w))
    O.stroke(shore, coast[1:-1], 1.6 * s, .9)
    O.ink(img, shore, inkc, .8)
    river = [(w * .86, h * .02), (w * .78, h * .14), (w * .66, h * .22), (w * .55, h * .30), (w * .44, h * .42), (w * .32, h * .55), (w * .20, h * .66), (w * .09, h * .74)]
    r_acc = np.zeros((h, w))
    O.stroke(r_acc, river, 4.0 * s, .8)
    O.ink(img, r_acc, wash, .9)
    # forest about the first two stages (left), marsh tufts and drowned spires about the third and fourth (centre), mountains about the fifth and the Vault (right)
    tree = np.zeros((h, w))
    for _ in range(170):
        cx, cy = rng.uniform(.03, .36) * w, rng.uniform(.04, .52) * h
        if min(math.hypot(cx - nx, cy - ny) for nx, ny in nodes[:2] + [trail]) < 70 * s:
            continue
        m = rng.uniform(10, 17) * s
        O.fill_poly(tree, [(cx - m * .6, cy), (cx, cy - m * 1.6), (cx + m * .6, cy)], .85)
        O.stroke(tree, [(cx, cy), (cx, cy + m * .6)], 1.4 * s, .9)
    O.ink(img, tree, green)
    marsh = np.zeros((h, w))
    for _ in range(140):
        cx, cy = rng.uniform(.38, .66) * w, rng.uniform(.05, .50) * h
        if min(math.hypot(cx - nx, cy - ny) for nx, ny in nodes[2:4]) < 70 * s:
            continue
        m = rng.uniform(5, 9) * s
        for k in (-1, 0, 1):
            O.stroke(marsh, [(cx + k * m * .4, cy), (cx + k * m * .9, cy - m)], .9 * s, .8)
    O.ink(img, marsh, (.24, .30, .26))
    spires = np.zeros((h, w))
    for (fx, fy, hh) in ((.47, .10, 52), (.50, .08, 70), (.53, .11, 46), (.60, .40, 40), (.63, .38, 56)):
        cx, cy, hh = fx * w, fy * h, hh * s
        O.fill_poly(spires, [(cx - 9 * s, cy + hh * .5), (cx - 9 * s, cy - hh * .2), (cx, cy - hh * .5), (cx + 9 * s, cy - hh * .2), (cx + 9 * s, cy + hh * .5)], .9)
    O.ink(img, spires, (.22, .24, .30))
    mt = np.zeros((h, w))
    for _ in range(80):
        cx, cy, m = rng.uniform(.68, .97) * w, rng.uniform(.04, .48) * h, rng.uniform(16, 30) * s
        if min(math.hypot(cx - nx, cy - ny) for nx, ny in nodes[4:] + [goal]) < 90 * s:
            continue
        O.stroke(mt, [(cx - m, cy + m * .8), (cx, cy - m * .3), (cx + m, cy + m * .8)], 1.6 * s, .9)
        O.stroke(mt, [(cx, cy - m * .3), (cx + m * .25, cy + m * .8)], 1.0 * s, .5)
    O.ink(img, mt, inkc)
    # the pencilled road (the game lays ink strokes over it as the party walks it)
    road = np.zeros((h, w))
    O.stroke(road, [trail] + nodes + [goal], 2.2 * s, .7, dash=(18 * s, 10 * s))
    O.ink(img, road, (.30, .24, .17), .75)
    # sockets: a double red ring at every stage, a crypt sketch behind the goal
    sock = np.zeros((h, w))
    for (x, y) in nodes:
        for r in (40 * s, 33 * s):
            for ang in np.linspace(0, math.tau, 120):
                O.stamp(sock, x + r * math.cos(ang), y + r * .62 * math.sin(ang), 1.4 * s, 1.0)
    O.ink(img, sock, red, .8)
    vt = np.zeros((h, w))
    gx, gy = goal
    O.fill_poly(vt, [(gx - 60 * s, gy - 10 * s), (gx - 60 * s, gy - 60 * s), (gx, gy - 92 * s), (gx + 60 * s, gy - 60 * s), (gx + 60 * s, gy - 10 * s)], .55)
    O.ink(img, vt, (.22, .24, .30), .8)
    th = np.zeros((h, w))
    tx, ty = trail
    for k in range(3):
        O.stroke(th, [(tx - 26 * s + k * 18 * s, ty + 16 * s), (tx - 18 * s + k * 18 * s, ty - 14 * s), (tx - 10 * s + k * 18 * s, ty + 16 * s)], 1.6 * s, .9)
    O.ink(img, th, inkc)
    # compass rose (near right) and a ribboned cartouche (near left), no lettering
    cr = np.zeros((h, w))
    cx, cy, R_ = w * .93, h * .86, 70 * s
    for ang, ln in ((0, 1.0), (90, .62), (180, .62), (270, .62)):
        a = math.radians(ang - 90)
        tip = (cx + math.cos(a) * R_ * ln, cy + math.sin(a) * R_ * ln)
        O.fill_poly(cr, [tip, (cx + math.cos(a + 1.5708) * 10 * s, cy + math.sin(a + 1.5708) * 10 * s), (cx + math.cos(a - 1.5708) * 10 * s, cy + math.sin(a - 1.5708) * 10 * s)], .95)
    for ang in np.linspace(0, math.tau, 200):
        O.stamp(cr, cx + R_ * .72 * math.cos(ang), cy + R_ * .72 * math.sin(ang), 1.2 * s, .9)
    O.ink(img, cr, inkc)
    rb = np.zeros((h, w))
    bx, by = w * .17, h * .90
    O.fill_poly(rb, [(bx - 130 * s, by - 22 * s), (bx + 130 * s, by - 22 * s), (bx + 150 * s, by), (bx + 130 * s, by + 22 * s), (bx - 130 * s, by + 22 * s), (bx - 150 * s, by)], .35)
    O.ink(img, rb, red, .6)
    O.stroke(rb, [(bx - 120 * s, by), (bx + 120 * s, by)], 1.2 * s, .6)
    O.ink(img, rb, inkc, .4)
    # border, folds, a scorched corner
    bd = np.zeros((h, w))
    for off, r in ((18 * s, 2.6 * s), (30 * s, 1.3 * s)):
        O.stroke(bd, [(off, off), (w - off, off), (w - off, h - off), (off, h - off), (off, off)], r, .85)
    O.ink(img, bd, inkc)
    fold = np.zeros((h, w))
    for x in (w / 4, w / 2, 3 * w / 4):
        O.stroke(fold, [(x + rng.normal(0, 2), 0), (x + rng.normal(0, 2), h)], 1.0, .3)
    O.stroke(fold, [(0, h / 2), (w, h / 2 + 4)], 1.0, .3)
    O.ink(img, fold, (.30, .20, .10))
    yy, xx = np.mgrid[0:h, 0:w]
    burn = np.clip(1.2 - np.hypot(xx - w, yy) / (120 * s), 0, 1) * (.6 + .4 * O.pnoise(h, w, 12, 18, seed + 7))
    O.ink(img, burn, (.06, .04, .03), .85)
    return O.rgba(img)


def map_point(u, v):
    """Map uv (top-down) to Blender metres: flat with a faint wave and a curl at the far-right corner."""
    x_cm = MAP_X0 + (MAP_X1 - MAP_X0) * u
    y_cm = MAP_Y0 + (MAP_Y1 - MAP_Y0) * v
    bx, by = game_to_blender(x_cm, y_cm)
    curl = .02 * max(0, 1 - math.hypot(u - 1.0, v - 0.0) / .12) ** 2
    wave = .0015 * math.sin(u * 9 + 1.3) * math.sin(v * 6)
    return bx, by, .004 + curl + wave


def build_map():
    texs = swatches()
    texs['vellum'] = journey_map_texture()
    sp = Soup()
    nu, nv = 36, 24
    for i in range(nu):
        for j in range(nv):
            q = [map_point(i / nu, j / nv), map_point((i + 1) / nu, j / nv), map_point((i + 1) / nu, (j + 1) / nv), map_point(i / nu, (j + 1) / nv)]
            # u runs to the viewer's right (+Y), v toward the viewer (+X): wound so the normal is +Z
            sp.quad('vellum', q[0], q[3], q[2], q[1], (i / nu, j / nv), (i / nu, (j + 1) / nv), ((i + 1) / nu, (j + 1) / nv), ((i + 1) / nu, j / nv))
    for (u0, v0, u1, v1) in ((0, 0, 1, 0), (1, 0, 1, 1), (1, 1, 0, 1), (0, 1, 0, 0)):
        steps = 24
        for k in range(steps):
            a = map_point(u0 + (u1 - u0) * k / steps, v0 + (v1 - v0) * k / steps)
            b = map_point(u0 + (u1 - u0) * (k + 1) / steps, v0 + (v1 - v0) * (k + 1) / steps)
            sp.quad('paper', a, b, (b[0], b[1], b[2] - .004), (a[0], a[1], a[2] - .004))
    for (u, v) in ((.02, .03), (.02, .97), (.98, .97)):
        x, y, z = map_point(u, v)
        TT.weight(sp, x, y)
    s = light_soup(sp.soup(), .78, .22)
    return export('SM_Journey_Map', 'journey-map', s, texs, 2048, solid=['paper', 'stone', 'brass'])


# ------------------------------------------------------------------ the table
def build_table():
    texs = dict(swatches(), **TT.textures()[0])          # the war table's own materials (books, d6, leather...) beside ours
    TT.X0, TT.X1, TT.HY = -0.60, 2.15, 3.0
    sp = Soup()
    TT.table_top(sp)
    rng = np.random.default_rng(2617)
    cx, cy = game_to_blender(*LAYOUT['candles_cm'][0][:2])
    TT.candlestick(sp, cx, cy)
    px_, py_ = game_to_blender(*LAYOUT['candles_cm'][1][:2])
    TT.pillar_candle(sp, px_, py_)
    # a few things at the far edges, outside the chart (seen on wide screens)
    TT.hourglass(sp, .35, -2.15)
    TT.book_stack(sp, 1.05, -2.3, rng)
    TT.coins(sp, 1.65, -2.05, 7, rng)
    TT.inkwell(sp, .15, 2.3)
    TT.scroll(sp, .85, 2.3, 80)
    TT.tankard(sp, 1.4, 2.45)
    s = sp.soup()
    s['mat'] = np.array(s['mat'], dtype=object)
    s = O.subdivide(s, lambda mid: np.where(mid[..., 2] < .02, .35, .6))
    # baked candle light: the two flames, a warm pool, and a dim cool floor
    n = O.tri_normals(s['P'])
    warm = np.zeros(s['P'].shape[:2])
    for (x_cm, y_cm, z_cm) in LAYOUT['candles_cm']:
        bx, by = game_to_blender(x_cm, y_cm)
        d = s['P'] - np.array((bx, by, z_cm / 100.0))
        dist2 = (d ** 2).sum(-1)
        lam = np.clip(-(d * n[:, None, :]).sum(-1) / np.sqrt(dist2 + 1e-6), 0, 1) * .6 + .4
        warm += lam * np.exp(-dist2 / (2 * 1.1 * 1.1)) * 1.0
    s['C'] = L.knee(np.ones((len(s['P']), 3, 3)) * (.34 * np.array((.6, .64, .86)))[None, None, :] + warm[..., None] * np.array((1.0, .72, .42)))
    gl = np.isin(s['mat'], ('flame', 'ember'))
    s['C'][gl] = 1.0
    texs['ember'] = L.swatch((1.0, .72, .30))
    texs['ale'] = L.swatch((.30, .15, .05))
    texs['ceramic'] = O.stone_speckle(32, 323, (.12, .12, .14))
    return export('SM_Journey_Table', 'journey-table', s, texs, 1024, glow=('flame', 'ember'))


# ------------------------------------------------------------------ tokens and markers
def build_stage():
    texs = swatches()
    sp = Soup()
    sp.lathe('dark', (0, 0, 0), [(.0, 0), (.088, 0), (.09, .006), (.084, .014), (.0, .015)], 20)           # oak foot
    sp.lathe('bronze', (0, 0, .014), [(.0, 0), (.080, 0), (.082, .006), (.079, .018), (.072, .022), (.060, .021), (.058, .016), (.0, .016)], 24)   # a rimmed disc
    for k in range(12):                                                                                       # studs on the rim
        a = k * math.tau / 12
        sp.lathe('bronze', (math.cos(a) * .070, math.sin(a) * .070, .034), [(.0, 0), (.006, 0), (.005, .004), (.0, .005)], 6)
    s = light_soup(sp.soup(), .62, .4)
    return export('SM_Journey_Stage', 'journey-stage', s, texs, 512)


def medallion(kind):
    """A 10 cm medallion on a short post, tilted back 35 degrees so it faces the camera (+X), standing on the token's top (z 0.03)."""
    texs = swatches()
    texs['face'] = emblem_texture(kind)
    sp = Soup()
    sp.lathe('brass', (0, 0, 0), [(.0, 0), (.012, 0), (.010, .006), (.006, .03), (.0, .032)], 8)
    sub = Soup()
    r, n = .05, 28
    for k in range(n):                                                                   # the face (in the YZ plane, normal +X), UV a disc in the texture
        a0, a1 = k * math.tau / n, (k + 1) * math.tau / n
        p0, p1 = (0, r * math.cos(a0), r * math.sin(a0)), (0, r * math.cos(a1), r * math.sin(a1))
        u = lambda a: (.5 + .5 * math.cos(a), .5 - .5 * math.sin(a))
        sub.tri('face', (0.0, 0.0, 0.0), p0, p1, (.5, .5), u(a0), u(a1))
    rim = Soup()
    rim.lathe('brass', (0, 0, 0), [(r, -.008), (r * 1.08, -.008), (r * 1.08, .003), (r, .003)], 28, cap=(False, False))
    s_ = L.cat(sub.soup(), L.xform(rim.soup(), np.array(Ry(90))))
    tilt = np.array(T(0, 0, .03 + .055)) @ np.array(Ry(-35))
    add(sp, L.xform(s_, tilt))
    s = sp.soup()
    s['mat'] = np.array(s['mat'], dtype=object)
    s = light_soup(s, .6, .4)
    glow = ('face',) if kind == 'vault' else ()
    name = 'SM_Journey_Emblem_' + kind.capitalize()
    return export(name, 'journey-emblem-' + kind, s, texs, 256, glow=glow, solid=['brass'])


def build_pennant():
    texs = swatches()
    sp = Soup()
    x, y = -.035, .06
    sp.lathe('brass', (x, y, 0), [(.0, 0), (.004, 0), (.003, .17), (.006, .172), (.006, .178), (.0, .18)], 6)
    # a swallow-tailed flag facing the camera (in the YZ plane), two-sided
    flag = [(0, 0), (.085, .004), (.072, .026), (.085, .048), (0, .052)]
    pts = [(x + .002, y + fy, .17 - fz) for (fy, fz) in flag]
    for (a, b, c) in ((0, 1, 2), (0, 2, 4), (2, 3, 4)):
        uvs = [((p[1] - y) / .09, (.17 - p[2]) / .06) for p in (pts[a], pts[b], pts[c])]
        sp.tri('cloth', pts[a], pts[b], pts[c], *uvs)
        sp.tri('cloth', pts[a], pts[c], pts[b], uvs[0], uvs[2], uvs[1])
    trim = [(x + .003, y + fy, .17 - fz) for (fy, fz) in ((0, 0), (.085, .004))]
    sp.tube('gilt', trim, [.0018, .0018], 5)
    texs['cloth'] = O.cloth_tile(64, 393, (.62, .14, .10))
    s = light_soup(sp.soup(), .66, .34)
    return export('SM_Journey_Pennant', 'journey-pennant', s, texs, 256, solid=['brass', 'gilt'])


def build_lost():
    texs = swatches()
    sp = Soup()
    blade = Soup()
    blade.box('steel', (0, 0, .055), (.004, .016, .07), tile=.05)                      # the stump of the blade, driven into the token
    # a jagged break at the top
    for k, (dy, dz) in enumerate(((-.008, .09), (-.002, .1), (.004, .094), (.008, .102))):
        blade.tri('steel', (0, -.008, .09), (0, dy, dz), (0, .008, .09), (0, 0), (.5, 1), (1, 0))
        blade.tri('steel', (0, -.008, .09), (0, .008, .09), (0, dy, dz), (0, 0), (1, 0), (.5, 1))
    blade.box('iron', (0, 0, .02), (.012, .07, .012), tile=.05)                         # crossguard
    blade.box('leather', (0, 0, -.012), (.012, .012, .05), tile=.05)                    # grip, below the guard (half sunk)
    s_ = blade.soup()
    M = np.array(T(-.01, -.015, .028)) @ np.array(Rx(18)) @ np.array(Rz(25))
    add(sp, L.xform(s_, M))
    # the sword's broken tip lying on the token
    tip = Soup()
    tip.box('steel', (0, 0, .002), (.045, .014, .004), tile=.05)
    add(sp, L.xform(tip.soup(), np.array(T(.03, .03, .034)) @ np.array(Rz(-30))))
    s = sp.soup()
    s['mat'] = np.array(s['mat'], dtype=object)
    s = light_soup(s, .55, .45)
    return export('SM_Journey_Lost', 'journey-lost', s, texs, 256)


def ink_stroke(dashed):
    """A 1 m ink stroke along +X from the origin, 2.6 cm wide, a hair above the map; widths waver (walked) or break into dashes (unwalked)."""
    texs = swatches()
    sp = Soup()
    mat = 'pencil' if dashed else 'ink'
    steps = 40
    rng = np.random.default_rng(471 if dashed else 472)
    wobble = rng.uniform(.85, 1.15, steps + 1)
    for k in range(steps):
        x0, x1 = k / steps, (k + 1) / steps
        if dashed and (k % 4) == 3:
            continue
        w0, w1 = .013 * (.7 if dashed else wobble[k]), .013 * (.7 if dashed else wobble[k + 1])
        z = .0008
        sp.quad(mat, (x0, -w0, z), (x1, -w1, z), (x1, w1, z), (x0, w0, z))
    s = sp.soup()
    s['C'] = np.ones((len(s['P']), 3, 3)) * .9
    name = 'SM_Journey_InkDashed' if dashed else 'SM_Journey_Ink'
    return export(name, 'journey-ink-dashed' if dashed else 'journey-ink', s, texs, 64)


def build_fog():
    """A fog bank 46 x 34 cm: layered soft discs and two low curved curtains.  Translucent card: opacity in COLOR_0.a, two-sided."""
    texs = {'mist': cloud_texture()}
    rng = np.random.default_rng(481)
    tris, uvs, al = [], [], []

    def disc(cx, cy, z, rx, ry, peak, rings=6, sides=24):
        for i in range(rings):
            for k in range(sides):
                r0, r1 = i / rings, (i + 1) / rings
                a0, a1 = k * math.tau / sides, (k + 1) * math.tau / sides
                def P(r, a):
                    j = 1 + .12 * math.sin(a * 3 + cx * 40) * r
                    return (cx + rx * r * j * math.cos(a), cy + ry * r * j * math.sin(a), z + .015 * (1 - r * r))
                def A(r):
                    return peak * float((1 - r * r) ** 1.6)
                q = [P(r0, a0), P(r1, a0), P(r1, a1), P(r0, a1)]
                aa = [A(r0), A(r1), A(r1), A(r0)]
                uu = [(.5 + .5 * p[0] / .3, .5 + .5 * p[1] / .3) for p in q]
                for t in ((0, 1, 2), (0, 2, 3)):
                    tris.append([q[x] for x in t]); uvs.append([uu[x] for x in t]); al.append([aa[x] for x in t])

    disc(0, 0, .01, .27, .22, 1.0)
    disc(.03, -.05, .04, .20, .17, .95)
    disc(-.05, .06, .065, .16, .13, .85)
    disc(.05, .04, .085, .12, .10, .75)
    disc(.0, .0, .11, .09, .08, .6)

    def curtain(cx, ang, width, height, peak):
        n, m = 10, 4
        ca, sa = math.cos(ang), math.sin(ang)
        for i in range(n):
            for j in range(m):
                def P(a, b):
                    t = a - .5
                    return (cx + -sa * t * width + ca * .03 * math.cos(t * 3), ca * t * width + sa * .03 * math.cos(t * 3), b * height)
                def A(a, b):
                    return peak * float(O.smoothstep(0, .35, min(a, 1 - a)) * (1 - b) ** 1.3)
                q = [P(i / n, j / m), P((i + 1) / n, j / m), P((i + 1) / n, (j + 1) / m), P(i / n, (j + 1) / m)]
                aa = [A(i / n, j / m), A((i + 1) / n, j / m), A((i + 1) / n, (j + 1) / m), A(i / n, (j + 1) / m)]
                uu = [(i / n, 1 - j / m), ((i + 1) / n, 1 - j / m), ((i + 1) / n, 1 - (j + 1) / m), (i / n, 1 - (j + 1) / m)]
                for t in ((0, 1, 2), (0, 2, 3)):
                    tris.append([q[x] for x in t]); uvs.append([uu[x] for x in t]); al.append([aa[x] for x in t])

    curtain(.10, 0.0, .40, .14, .55)
    curtain(-.06, .35, .32, .12, .4)
    s = dict(P=np.array(tris, float), UV=np.array(uvs, float), C=None, mat=np.array(['mist'] * len(tris), dtype=object))
    a = np.array(al)
    # the discs' centre quads collapse to triangles; drop the degenerate halves here so the opacity stays aligned with the triangles
    area = np.linalg.norm(np.cross(s['P'][:, 1] - s['P'][:, 0], s['P'][:, 2] - s['P'][:, 0]), axis=1)
    keep = area > 1e-10
    s, a = L.sub(s, keep), a[keep]
    f = L.flipped(s, .002)
    s = L.cat(s, f)
    a = np.concatenate([a, a[:, [0, 2, 1]]])
    s['C'] = np.ones((len(s['P']), 3, 3)) * np.array((.58, .56, .52))
    return export('SM_Journey_Fog', 'journey-fog', s, texs, 256, alpha=a)


def build_goal():
    """The Vault keep: a squat crypt on a stone plinth, a doorway facing the camera, and a floating Waystone shard that glows."""
    texs = swatches()
    sp = Soup()
    sp.lathe('stone', (0, 0, 0), [(.0, 0), (.10, 0), (.102, .008), (.096, .016), (.0, .016)], 16)
    sp.box('slate', (0, 0, .016 + .05), (.12, .14, .10), tile=.08)
    sp.box('slate', (0, 0, .116 + .03), (.09, .10, .06), tile=.08)
    for (dy, dx) in ((-.06, -.05), (.06, -.05), (-.06, .05), (.06, .05)):           # crenels
        sp.box('slate', (dx, dy, .116 + .012), (.024, .024, .024), tile=.08)
    sp.box('iron', (.061, 0, .016 + .03), (.004, .036, .06), tile=.05)                  # the door, facing +X
    sp.lathe('brass', (0, 0, .176), [(.0, 0), (.02, 0), (.016, .008), (.0, .01)], 8)
    shard = Soup()
    shard.lathe('crystal', (0, 0, 0), [(.0, 0), (.016, .02), (.012, .05), (.0, .07)], 6)
    add(sp, L.xform(shard.soup(), np.array(T(0, 0, .20)) @ np.array(Rz(15))))
    s = sp.soup()
    s['mat'] = np.array(s['mat'], dtype=object)
    s = light_soup(s, .55, .45, (.86, .9, 1.0))
    s['C'][s['mat'] == 'crystal'] = 1.0
    return export('SM_Journey_Goal', 'journey-goal', s, texs, 256, glow=('crystal',))


def build_plinth():
    """The riser a raised diorama stands on: an oak block 0.72 (toward the camera) x 0.74 m with a moulded lip, brass corner caps and a brass plate on its front."""
    texs = swatches()
    sp = Soup()
    D, Wd, H = .72, .74, .14
    sp.box('dark', (0, 0, .01), (D + .04, Wd + .04, .02), tile=.4)
    sp.box('dark', (0, 0, .02 + (H - .04) / 2), (D, Wd, H - .04), tile=.4)
    sp.box('dark', (0, 0, H - .01), (D + .03, Wd + .03, .02), tile=.4)
    for sx in (-1, 1):
        for sy in (-1, 1):
            sp.box('brass', (sx * (D / 2 + .005), sy * (Wd / 2 + .005), .02 + (H - .04) / 2), (.03, .03, H - .03), tile=.1)
    sp.box('brass', (D / 2 + .004, 0, .07), (.006, .30, .05), tile=.1)
    s = light_soup(sp.soup(), .5, .5)
    return export('SM_Journey_Plinth', 'journey-plinth', s, texs, 512, solid=['brass'])


def build():
    metrics = {}
    for fn in (build_map, build_table, build_stage, lambda: medallion('root'), lambda: medallion('vigil'), lambda: medallion('vault'), build_pennant, build_lost,
               lambda: ink_stroke(False), lambda: ink_stroke(True), build_fog, build_goal, build_plinth):
        m, A = fn()
        name = os.path.splitext(os.path.basename(m['file']))[0]
        metrics[name] = m
        if name == 'SM_Journey_Map':
            L.save_png(A, os.path.join(SRC, 'T_Journey_Map_Atlas.png'))
    json.dump(metrics, open(os.path.join(SRC, 'journey_metrics.json'), 'w'), indent=1)
    print('JOURNEY', {k: v['triangles'] for k, v in metrics.items()})


if __name__ == '__main__':
    TT.run_guarded(build)
