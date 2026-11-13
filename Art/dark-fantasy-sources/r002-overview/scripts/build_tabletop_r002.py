"""The candlelit war table, r002 (issue #149, parent #138): SM_Overview_Tabletop.

  blender -b --python-exit-code 1 --python Art/dark-fantasy-sources/r002-overview/scripts/build_tabletop_r002.py       (shared host: under the machine lock)

What the route overview and the start screen show behind the HUD: oak planks at 256 texels/m, a bookcase of leather spines, and a war table
deep enough that the near half of the frame is table, not floor.  On it: a hand-drawn vellum map of the five-stop route (the Vigil road, the
forest, the vault) held by stone weights, five glowing route markers, a candelabra, a pillar candle and a candle cluster (vertex-alpha glow on
the flames), a tankard, a dagger, dice, coins, an inkwell and quill, scrolls and a hourglass.  Everything is procedural; no outside content.

Conventions (see ov_lib): metres, the asset faces +X (the game turns it -90 degrees about Z), +Y is the viewer's right, origin = middle of the
table top.  The three runtime candle lights are placed at the candle flames: LIGHTS below, also written to tabletop_metrics.json.
Outputs: Art/overview-tabletop/r001/SM_Overview_Tabletop.glb, r002-overview/T_Overview_Tabletop_Atlas.png, r002-overview/tabletop_metrics.json.
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ov_lib as O
from ov_lib import L, R, bpy, np, T, Rz, Rx, Ry, Soup, cbox, uvquad

ROOT, OUT_SRC = O.ROOT, O.OUT_SRC
ATLAS, PAD = 2048, 8
RNG = np.random.default_rng(2611)

# table (metres): back edge, front edge, half width, thickness
X0, X1, HY, TH = -0.60, 2.05, 1.70, 0.09
# the candles: (name, x, y, flame z) - the three runtime lights sit on these flames
LIGHTS = {'candelabra': (-0.30, 1.00, 0.43), 'pillar': (-0.34, -1.00, 0.30), 'cluster': (0.90, 0.55, 0.12)}
MAP_C, MAP_W, MAP_D, MAP_YAW = (1.66, -0.02), 1.05, 0.63, -3.0
GLOW = ('flame', 'ember')
# shelf ornaments by row (y positions along the case); books skip them
ORN = {0: [-1.9, -0.7, 0.55, 1.75, 2.45], 1: [-2.35, -1.2, 0.0, 1.2, 2.2], 2: []}


def textures():
    t = {}
    t['wood'] = O.plank_tile(512, 2.0, 301)
    t['dark'] = O.dark_wood_tile(256, 311)
    t['vellum'], nodes = O.war_map(640, 384, 361)
    t['books'] = O.book_spines(256, 341)
    t['stone'] = O.stone_speckle(64, 321)
    t['brass'] = O.metal_tile((.80, .60, .22), 64, 331)
    t['steel'] = O.metal_tile((.62, .64, .68), 64, 332)
    t['pewter'] = O.metal_tile((.50, .52, .55), 64, 333)
    t['iron'] = O.metal_tile((.20, .20, .22), 64, 334, streak=False)
    t['wax'] = O.wax_tile(32)
    t['leather'] = O.leather_tile(64, 381)
    t['cloth'] = O.cloth_tile(64, 391, (.45, .10, .09))
    t['bone'] = O.stone_speckle(32, 322, (.80, .76, .64))
    t['d6'] = O.die_pips(96)
    t['d20'] = O.die_glyph((.46, .07, .09), (.88, .72, .30), 64, 81)
    t['d8'] = O.die_glyph((.10, .18, .42), (.86, .82, .70), 64, 82)
    t['ceramic'] = O.stone_speckle(32, 323, (.12, .12, .14))
    t['flame'] = L.swatch((1.0, .62, .25))
    t['ember'] = L.swatch((1.0, .72, .30))
    t['paper'] = L.swatch((.80, .72, .55))
    t['ale'] = L.swatch((.30, .15, .05))
    t['glass'] = L.swatch((.55, .60, .50))
    return t, nodes


# ------------------------------------------------------------------ the table
def table_top(sp):
    W, Ln = 2 * HY, X1 - X0
    sp.grid('wood', (X1, -HY, 0), (0, W, 0), (-Ln, 0, 0), 1, 1, (W / 2.0, Ln / 2.0))             # planks run left-right, 256 texels/m
    sp.grid('dark', (X1, -HY, -TH), (0, W, 0), (0, 0, TH), 1, 1, (W / 1.0, TH / 1.0))             # front edge
    sp.grid('dark', (X1, HY, -TH), (-Ln, 0, 0), (0, 0, TH), 1, 1, (Ln / 1.0, TH / 1.0))           # right edge
    sp.grid('dark', (X0, -HY, -TH), (Ln, 0, 0), (0, 0, TH), 1, 1, (Ln / 1.0, TH / 1.0))           # left edge
    sp.box('dark', (X1 - 0.07, 0, -TH - 0.10), (0.05, W - 0.30, 0.20), tile=1.0)                 # front apron under the lip
    for sx in (-1, 1):
        sp.box('dark', ((X0 + X1) / 2, sx * (HY - 0.07), -TH - 0.10), (Ln - 0.30, 0.05, 0.20), tile=1.0)


def wall_and_books(sp, rng):
    """Back wall (dark panelling) and a bookcase: boards, dividers, leather spines with gilt bands, a few ornaments."""
    xw = -1.00
    sp.grid('dark', (xw, -3.1, -0.35), (0, 6.2, 0), (0, 0, 1.9), 1, 1, (6.2 / 1.0, 1.9 / 1.0))
    boards = [0.06, 0.32, 0.58, 0.84]
    bx = -0.89
    for z in boards:
        sp.box('dark', (bx, 0, z - 0.012), (0.24, 6.2, 0.024), tile=0.8)
    for y in np.arange(-3.0, 3.01, 1.0):
        sp.box('dark', (bx, y, (boards[0] + boards[-1]) / 2 - 0.012), (0.24, 0.035, boards[-1] - boards[0]), tile=0.8)
    sp.box('dark', (bx - 0.01, 0, boards[-1] + 0.05), (0.30, 6.2, 0.07), tile=0.8)                  # cornice
    cells = [(i, j) for j in range(4) for i in range(4)]
    spine_x = -0.80
    for row, z0 in enumerate(boards[:-1]):
        gap = boards[row + 1] - z0 - 0.024
        y = -3.0
        while y < 3.0:
            if any(abs(y + .03 - o) < .085 for o in ORN[row]):                 # an ornament stands here
                y += .05
                continue
            if rng.random() < .045:                                           # a gap, with a leaning book next to it
                y += rng.uniform(.06, .16)
            w, h, d = rng.uniform(.026, .056), gap * rng.uniform(.62, .93), rng.uniform(.14, .2)
            i, j = cells[int(rng.integers(0, 16))]
            r = ((i + .03) / 4, (j + .03) / 4, (i + .97) / 4, (j + .97) / 4)
            sx = spine_x + rng.uniform(-.012, .006)
            M = np.array(T(sx - d / 2, y + w / 2, z0 + h / 2))
            sub = Soup()
            cbox(sub, (0, 0, 0), (d, w, h), {'px': 'books', 'nx': 'books', 'py': 'books', 'ny': 'books', 'top': 'paper', 'bot': 'paper'},
                 {k: r for k in ('px', 'nx', 'py', 'ny')}, bottom=False)
            sp_soup = sub.soup()
            if rng.random() < .07:                                           # a book leaning on its neighbour
                lean = float(rng.uniform(8, 16))
                M = M @ np.array(Rx(lean * rng.choice([-1, 1])))
            s_ = L.xform(sp_soup, M)
            sp.P += [tuple(map(tuple, tri)) for tri in s_['P'].tolist()]
            sp.UV += [tuple(map(tuple, uv)) for uv in s_['UV'].tolist()]
            sp.M += list(s_['mat'])
            y += w + rng.uniform(.0, .004)
        # ornaments standing on the board of this row (candle jars, a globe, hourglass, scroll bundles)
    return sp


def add(sp, s):
    sp.P += [tuple(map(tuple, tri)) for tri in s['P'].tolist()]
    sp.UV += [tuple(map(tuple, uv)) for uv in s['UV'].tolist()]
    sp.M += list(s['mat'])


def piece(builder, M):
    sub = Soup()
    builder(sub)
    return L.xform(sub.soup(), np.array(M))


def flame_at(sp, x, y, z, h=.06, r=.013):
    sub = Soup()
    prof = [(.001, 0), (r * .8, h * .08), (r, h * .26), (r * .78, h * .5), (r * .4, h * .74), (r * .12, h * .92), (.001, h)]
    sub.lathe('flame', (x, y, z), prof, 6, cap=(True, True))
    s = sub.soup()
    s['UV'][..., 0] = .5
    s['UV'][..., 1] = .5
    add(sp, s)


def candlestick(sp, x, y):
    """Tall brass candlestick, 26 cm of brass and a 15 cm candle; the flame sits at LIGHTS['candelabra']."""
    sub = Soup()
    prof = [(.0, 0), (.075, 0), (.078, .008), (.06, .02), (.03, .035), (.016, .05), (.026, .075), (.016, .1), (.012, .13), (.03, .16), (.016, .185), (.028, .21), (.05, .235), (.052, .25), (.0, .252)]
    sub.lathe('brass', (0, 0, 0), prof, 12)
    sub.lathe('wax', (0, 0, .25), [(.0, 0), (.021, 0), (.022, .02), (.02, .12), (.021, .15), (.0, .152)], 10)
    for k in range(3):                                                     # drips
        a = k * 2.1
        sub.lathe('wax', (math.cos(a) * .021, math.sin(a) * .021, .25 + .08), [(.0, 0), (.007, 0), (.006, .06), (.0, .066)], 5)
    add(sp, L.xform(sub.soup(), np.array(T(x, y, 0))))
    flame_at(sp, x, y, .25 + .152, .06, .0125)


def pillar_candle(sp, x, y):
    sub = Soup()
    sub.lathe('iron', (0, 0, 0), [(.0, 0), (.09, 0), (.095, .008), (.075, .018), (.0, .02)], 12)
    sub.lathe('wax', (0, 0, .02), [(.0, 0), (.036, 0), (.04, .01), (.036, .12), (.038, .2), (.034, .22), (.0, .222)], 12)
    for k in range(5):
        a = k * 1.3 + .4
        sub.lathe('wax', (math.cos(a) * .037, math.sin(a) * .037, .02 + .15 - k * .018), [(.0, 0), (.009, 0), (.008, .07 + k * .01), (.0, .078 + k * .01)], 5)
    # a puddle of wax on the dish
    sub.lathe('wax', (0, 0, .02), [(.0, 0), (.07, 0), (.062, .006), (.0, .008)], 12)
    add(sp, L.xform(sub.soup(), np.array(T(x, y, 0))))
    flame_at(sp, x, y, .02 + .222, .06, .014)


def candle_cluster(sp, x, y, rng):
    sub = Soup()
    sub.lathe('brass', (0, 0, 0), [(.0, 0), (.11, 0), (.115, .008), (.10, .016), (.0, .018)], 14)
    flames = []
    for k, (dx, dy, h, r) in enumerate(((0.0, 0.0, .11, .024), (.045, .02, .075, .02), (-.04, .03, .05, .022), (.02, -.05, .09, .019))):
        sub.lathe('wax', (dx, dy, .018), [(.0, 0), (r, 0), (r * 1.05, .01), (r * .95, h), (.0, h + .003)], 9)
        flames.append((dx, dy, .018 + h))
    add(sp, L.xform(sub.soup(), np.array(T(x, y, 0))))
    for dx, dy, z in flames[:2]:
        flame_at(sp, x + dx, y + dy, z, .05, .011)
    return flames


def weight(sp, x, y):
    sub = Soup()
    sub.lathe('stone', (0, 0, 0), [(.0, 0), (.033, 0), (.037, .004), (.035, .02), (.028, .028), (.0, .03)], 10)
    sub.lathe('brass', (0, 0, .027), [(.0, 0), (.029, 0), (.03, .004), (.02, .01), (.0, .012)], 10)
    add(sp, L.xform(sub.soup(), np.array(T(x, y, .004))))


def tankard(sp, x, y):
    sub = Soup()
    prof = [(.0, 0), (.054, 0), (.062, .008), (.06, .02), (.055, .12), (.059, .124), (.0, .124)]
    sub.lathe('pewter', (0, 0, 0), prof, 14)
    sub.lathe('ale', (0, 0, .104), [(.0, 0), (.05, 0), (.0, .002)], 14, cap=(False, False))
    path = [(.058, 0, .105), (.098, 0, .102), (.112, 0, .07), (.095, 0, .03), (.06, 0, .02)]
    sub.tube('steel', path, [.009] * 5, 6)
    sub.lathe('steel', (0, 0, .117), [(.058, 0), (.064, .003), (.058, .008), (.052, .004)], 14, cap=(False, False))      # rim band
    add(sp, L.xform(sub.soup(), np.array(T(x, y, 0)) @ np.array(Rz(35))))


def dagger(sp, x, y, yaw):
    sub = Soup()
    bl, bw, th = .22, .03, .005
    # blade: a flat kite, ridge in the middle
    a, b, c, d, tip = (0, -bw / 2, th), (0, bw / 2, th), (bl * .85, bw / 2, th), (bl * .85, -bw / 2, th), (bl, 0, th * .7)
    ridge0, ridge1 = (0, 0, th * 1.6), (bl * .9, 0, th * 1.4)
    sub.tri('steel', a, ridge0, ridge1, (0, 0), (0, 1), (1, 1))
    sub.tri('steel', a, ridge1, d, (0, 0), (1, 1), (1, 0))
    sub.tri('steel', ridge0, b, c, (0, 1), (1, 1), (1, 0))
    sub.tri('steel', ridge0, c, ridge1, (0, 1), (1, 0), (1, 1))
    sub.tri('steel', d, ridge1, tip, (0, 0), (1, 1), (1, 0))
    sub.tri('steel', ridge1, c, tip, (1, 1), (1, 0), (1, 0))
    cbox(sub, (-.008, 0, .007), (.016, .1, .014), 'brass')                                       # guard
    cbox(sub, (-.065, 0, .011), (.095, .022, .022), 'leather')                                   # grip
    cbox(sub, (-.125, 0, .011), (.02, .03, .026), 'brass')                                       # pommel
    add(sp, L.xform(sub.soup(), np.array(T(x, y, 0)) @ np.array(Rz(yaw))))


def coins(sp, x, y, n, rng):
    for k in range(n):
        ang, d = rng.uniform(0, math.tau), rng.uniform(0, .07)
        sub = Soup()
        sub.lathe('brass', (0, 0, 0), [(.0, 0), (.0135, 0), (.0135, .0025), (.0, .0025)], 10)
        z = .0025 * (k % 3) if rng.random() < .5 else 0
        add(sp, L.xform(sub.soup(), np.array(T(x + math.cos(ang) * d, y + math.sin(ang) * d, z)) @ np.array(Rz(float(rng.uniform(0, 90))))))


def pouch(sp, x, y):
    sub = Soup()
    sub.lathe('leather', (0, 0, 0), [(.0, 0), (.045, 0), (.06, .025), (.055, .06), (.03, .085), (.036, .1), (.014, .108), (.0, .108)], 10)
    sub.lathe('cloth', (0, 0, .085), [(.026, 0), (.034, .005), (.028, .012)], 8, cap=(False, False))
    add(sp, L.xform(sub.soup(), np.array(T(x, y, 0)) @ np.array(Rz(20))))


def inkwell(sp, x, y):
    sub = Soup()
    sub.lathe('ceramic', (0, 0, 0), [(.0, 0), (.04, 0), (.046, .01), (.044, .04), (.03, .058), (.02, .06), (.0, .06)], 12)
    sub.tube('paper', [(0, .015, .06), (.012, .035, .15), (.03, .06, .22)], [.003, .003, .002], 4)
    for k in range(5):                                                   # feather vane
        z = .12 + k * .018
        sub.tri('paper', (.012 + k * .003, .035 + k * .006, z), (.012 + k * .003 + .028, .035 + k * .006 + .01, z + .012), (.012 + k * .003, .035 + k * .006, z + .022), (0, 0), (1, 0), (0, 1))
    add(sp, L.xform(sub.soup(), np.array(T(x, y, 0))))


def scroll(sp, x, y, yaw, length=.3):
    sub = Soup()
    sub.lathe('paper', (0, 0, 0), [(.0, 0), (.021, 0), (.022, .02), (.021, length - .02), (.02, length), (.0, length)], 10)
    sub.lathe('cloth', (0, 0, length * .3), [(.0227, 0), (.0227, .018)], 10, cap=(False, False))
    sub.lathe('cloth', (0, 0, length * .7), [(.0227, 0), (.0227, .018)], 10, cap=(False, False))
    add(sp, L.xform(sub.soup(), np.array(T(x, y, .022)) @ np.array(Rz(yaw)) @ np.array(Ry(90))))


def hourglass(sp, x, y):
    sub = Soup()
    sub.lathe('brass', (0, 0, 0), [(.0, 0), (.05, 0), (.05, .012), (.0, .012)], 12)
    sub.lathe('glass', (0, 0, .012), [(.0, 0), (.034, .01), (.032, .05), (.006, .09), (.032, .13), (.034, .17), (.0, .18)], 10)
    sub.lathe('brass', (0, 0, .192), [(.0, 0), (.05, 0), (.05, .012), (.0, .012)], 12)
    for k in range(3):
        a = k * 2.094
        sub.lathe('brass', (math.cos(a) * .045, math.sin(a) * .045, .012), [(.0, 0), (.005, 0), (.005, .18), (.0, .18)], 5)
    add(sp, L.xform(sub.soup(), np.array(T(x, y, 0))))


def globe_and_jars(sp, rng, z_board, ys):
    for k, y in enumerate(ys):
        sub = Soup()
        if k % 3 == 0:                                                     # a candle jar: glow flame inside
            sub.lathe('glass', (0, 0, 0), [(.0, 0), (.04, 0), (.043, .03), (.038, .08), (.0, .08)], 10)
            sub.lathe('wax', (0, 0, 0), [(.0, 0), (.025, 0), (.026, .05), (.0, .05)], 8)
            add(sp, L.xform(sub.soup(), np.array(T(-.86, y, z_board))))
            flame_at(sp, -.86, y, z_board + .05, .045, .009)
        elif k % 3 == 1:                                                   # a globe on a brass stand
            sub.lathe('brass', (0, 0, 0), [(.0, 0), (.05, 0), (.012, .015), (.012, .08), (.0, .08)], 10)
            sub.lathe('leather', (0, 0, .09), [(.0, 0), (.05, .02), (.07, .07), (.05, .13), (.0, .15)], 12)
            add(sp, L.xform(sub.soup(), np.array(T(-.86, y, z_board))))
        else:                                                              # a bundle of scroll tubes
            for j in range(3):
                sub2 = Soup()
                sub2.lathe('paper', (0, 0, 0), [(.0, 0), (.02, 0), (.02, .26), (.0, .26)], 8)
                add(sp, L.xform(sub2.soup(), np.array(T(-.86 + rng.uniform(-.03, .03), y + j * .04, z_board + .02)) @ np.array(Rx(float(rng.uniform(-6, 6))))))


def book_stack(sp, x, y, rng):
    z = 0.0
    for k, (w, d, h, cell) in enumerate(((.20, .27, .038, 3), (.18, .25, .032, 6), (.16, .22, .03, 10))):
        i, j = cell % 4, cell // 4
        r = ((i + .03) / 4, (j + .03) / 4, (i + .97) / 4, (j + .97) / 4)
        sub = Soup()
        cbox(sub, (0, 0, 0), (d, w, h), {'px': 'books', 'nx': 'books', 'py': 'books', 'ny': 'books', 'top': 'leather', 'bot': 'leather'}, {f: r for f in ('px', 'nx', 'py', 'ny')}, bottom=False)
        add(sp, L.xform(sub.soup(), np.array(T(x, y, z + h / 2)) @ np.array(Rz(float(rng.uniform(-18, 18))))))
        z += h


def spyglass(sp, x, y):
    sub = Soup()
    sub.lathe('brass', (0, 0, 0), [(.018, 0), (.021, .02), (.017, .12), (.021, .125), (.015, .25), (.02, .255), (.014, .34), (.0, .34)], 10)
    add(sp, L.xform(sub.soup(), np.array(T(x, y, .021)) @ np.array(Rz(63)) @ np.array(Ry(90))))


def letter(sp, x, y, yaw):
    sub = Soup()
    sub.quad('paper', (-.07, -.05, .002), (-.07, .05, .002), (.07, .05, .002), (.07, -.05, .002))
    sub.lathe('cloth', (.0, .0, .002), [(.0, 0), (.013, 0), (.011, .006), (.0, .006)], 8)
    add(sp, L.xform(sub.soup(), np.array(T(x, y, 0)) @ np.array(Rz(yaw))))


def map_point(u, v):
    """Map uv (top-down: v = 0 at the far edge) to table coordinates and height (the sheet is gently curled at its near-left and far-right corners)."""
    a = math.radians(MAP_YAW)
    lx, ly = (u - .5) * MAP_W, (v - .5) * MAP_D            # lx along the width (y), ly toward the viewer (x)
    x, y = MAP_C[0] + ly * math.cos(a) - lx * math.sin(a), MAP_C[1] + lx * math.cos(a) + ly * math.sin(a)
    curl = .016 * max(0, 1 - math.hypot(u - 0.0, v - 1.0) / .17) ** 2 + .012 * max(0, 1 - math.hypot(u - 1.0, v - 0.0) / .15) ** 2
    wave = .0016 * math.sin(u * 7 + 1.3) * math.sin(v * 5)
    return x, y, .003 + curl + wave


def map_sheet(sp, nodes):
    nu, nv = 24, 14
    for i in range(nu):
        for j in range(nv):
            q = [map_point(i / nu, j / nv), map_point((i + 1) / nu, j / nv), map_point((i + 1) / nu, (j + 1) / nv), map_point(i / nu, (j + 1) / nv)]
            # viewed from above: u goes right (+y), v goes toward the viewer (+x): order so the normal is +Z
            sp.quad('vellum', q[0], q[3], q[2], q[1], (i / nu, j / nv), (i / nu, (j + 1) / nv), ((i + 1) / nu, (j + 1) / nv), ((i + 1) / nu, j / nv))
    # a thin paper edge so the sheet has thickness
    for (u0, v0, u1, v1) in ((0, 0, 1, 0), (1, 0, 1, 1), (1, 1, 0, 1), (0, 1, 0, 0)):
        steps = 12
        for k in range(steps):
            a = map_point(u0 + (u1 - u0) * k / steps, v0 + (v1 - v0) * k / steps)
            b = map_point(u0 + (u1 - u0) * (k + 1) / steps, v0 + (v1 - v0) * (k + 1) / steps)
            sp.quad('paper', (a[0], a[1], a[2]), (b[0], b[1], b[2]), (b[0], b[1], b[2] - .004), (a[0], a[1], a[2] - .004))
    marks = []
    for (u, v) in nodes:
        x, y, z = map_point(u, v)
        sub = Soup()
        sub.lathe('brass', (0, 0, 0), [(.0, 0), (.019, 0), (.02, .004), (.016, .007), (.0, .007)], 10)
        sub.lathe('ember', (0, 0, .006), [(.0, 0), (.013, .001), (.0125, .005), (.008, .01), (.0, .012)], 8)
        add(sp, L.xform(sub.soup(), np.array(T(x, y, z))))
        marks.append((round(x, 3), round(y, 3)))
    return marks


# ------------------------------------------------------------------ bake
def lighting():
    """(position, intensity, radius, warm tint) of the baked light: the three flames plus faint jar candles."""
    out = [((x, y, z), 1.0, .85) for (x, y, z) in LIGHTS.values()]
    return out


def build():
    rng = np.random.default_rng(2611)
    texs, nodes = textures()
    sp = Soup()
    table_top(sp)
    wall_and_books(sp, rng)
    marks = map_sheet(sp, nodes)
    # weights on three corners of the map
    for (u, v) in ((.035, .06), (.965, .94)):
        x, y, _ = map_point(u, v)
        weight(sp, x, y)
    candlestick(sp, LIGHTS['candelabra'][0], LIGHTS['candelabra'][1])
    pillar_candle(sp, LIGHTS['pillar'][0], LIGHTS['pillar'][1])
    candle_cluster(sp, LIGHTS['cluster'][0], LIGHTS['cluster'][1], rng)
    tankard(sp, 0.70, 1.00)
    dagger(sp, 1.12, .92, 34)
    for (x, y, yaw) in ((1.00, .78, 12), (1.08, .68, 51)):
        add(sp, O.die6((x, y, 0), .022, yaw))
    V, F = O.icosa(.019)
    V = O.rest_on_floor(V + np.array((.95, .84, 0)))
    sub = Soup(); O.tri_faces(sub, 'd20', V, F); add(sp, sub.soup())
    V, F = O.octa(.018)
    V = O.rest_on_floor(V + np.array((1.04, .60, 0)))
    sub = Soup(); O.tri_faces(sub, 'd8', V, F); add(sp, sub.soup())
    coins(sp, 0.78, .74, 9, rng)
    pouch(sp, 0.62, 1.18)
    inkwell(sp, 0.82, -1.02)
    scroll(sp, 1.00, -.80, 78)
    scroll(sp, 0.92, -.62, 96, .26)
    hourglass(sp, .22, -1.32)
    book_stack(sp, .55, -1.10, rng)
    spyglass(sp, 1.10, -.98)
    letter(sp, 1.02, -.34, 14)
    scroll(sp, .45, 1.15, 8, .34)
    # shelf ornaments
    globe_and_jars(sp, rng, .06, ORN[0])
    globe_and_jars(sp, rng, .32, ORN[1])
    s = sp.soup()
    s['mat'] = np.array(s['mat'], dtype=object)

    # ---- bake: subdivide (fine near the props and the map), ambient occlusion, warm candle light
    zones = [(.70, 1.00, .20), (1.12, .92, .22), (1.02, .72, .20), (.78, .74, .15), (.62, 1.18, .12), (.82, -1.02, .12), (.96, -.72, .30), (.22, -1.32, .10), (1.10, -.98, .25), (.55, -1.10, .25),
             (LIGHTS['cluster'][0], LIGHTS['cluster'][1], .20), (LIGHTS['candelabra'][0], LIGHTS['candelabra'][1], .20), (LIGHTS['pillar'][0], LIGHTS['pillar'][1], .20)]

    def lim(mid):
        d = np.full(mid.shape[:-1], 1e9)
        for (zx, zy, zr) in zones:
            d = np.minimum(d, np.hypot(mid[..., 0] - zx, mid[..., 1] - zy) - zr)
        onmap = (np.hypot((mid[..., 0] - MAP_C[0]) / (MAP_D * .75), (mid[..., 1] - MAP_C[1]) / (MAP_W * .75)) < 1.0) & (mid[..., 2] < .05)
        out = np.where(d < 0, .035, np.where(onmap, .10, np.where(mid[..., 2] < .08, .30, .45)))
        return out
    s = O.subdivide(s, lim)
    g = O.tri_soup([[(-6, -6, -.3), (6, -6, -.3), (6, 6, -.3)], [(-6, -6, -.3), (6, 6, -.3), (-6, 6, -.3)]], [[(0, 0)] * 3] * 2, None, 'g')
    bvh = O.make_bvh(L.cat(dict(s, C=None), dict(g, C=None))['P'])
    s['C'] = np.ones((len(s['P']), 3, 3))
    s, _ = bake_ao_(s, bvh)
    N = O.tri_normals(s['P'])
    cen = s['P']
    warm = np.zeros(cen.shape[:2])
    for (pos, k, rad) in lighting():
        d = cen - np.array(pos)
        dist2 = (d ** 2).sum(-1)
        lam = np.clip(-(d * N[:, None, :]).sum(-1) / np.sqrt(dist2 + 1e-6) , 0, 1) * .65 + .35
        warm += k * lam * np.exp(-dist2 / (2 * rad * rad)) * 1.1
    base = .30 + .0 * warm
    tint = np.array((1.0, .72, .42))
    s['C'] = s['C'] * (base[..., None] * np.array((.55, .62, .90)) + warm[..., None] * tint)
    z = cen[..., 2]
    s['C'] *= (.88 + .12 * np.clip((z + .1) / .5, 0, 1))[..., None]
    s['C'] = L.knee(s['C'])
    gl = np.isin(s['mat'], GLOW)
    s['C'][gl] = 1.0

    solid = [k for k in texs if texs[k].shape[0] <= 8]
    s2, A, place = L.build_atlas(s, texs, size=ATLAS, pad=PAD, solid=solid, repeat={'wood': (2, 2)})
    used_px = sum((w + 2 * PAD) * (h + 2 * PAD) for (_, _, w, h) in place.values())
    bpy.ops.wm.read_factory_settings(use_empty=True)
    img = L.new_image('T_Overview_Tabletop', A)
    ob, glow = R.make_object_rgba('SM_Overview_Tabletop', s2, img, glow_mats=GLOW, mat_name='M_Overview_Tabletop')
    path = R.export_glb(ob, os.path.join(ROOT, 'Art', 'overview-tabletop', 'r001', 'SM_Overview_Tabletop.glb'))
    L.save_png(A, os.path.join(OUT_SRC, 'T_Overview_Tabletop_Atlas.png'))
    st = R.glb_stats(path)
    m = dict(st, glow_tris=glow, atlas_px=ATLAS, atlas_fill=round(used_px / ATLAS / ATLAS, 3), regions_px={k: [v[2], v[3]] for k, v in place.items()},
             lights_m={k: list(v) for k, v in LIGHTS.items()}, route_marker_xy_m=marks, map=dict(centre_m=list(MAP_C), width_m=MAP_W, depth_m=MAP_D, yaw_deg=MAP_YAW),
             table=dict(x_back=X0, x_front=X1, half_width=HY),
             texel_density={'table top (planks)': 256, 'vellum map': round(640 / MAP_W), 'bookcase spines': round(64 / .06)})
    os.makedirs(OUT_SRC, exist_ok=True)
    json.dump(m, open(os.path.join(OUT_SRC, 'tabletop_metrics.json'), 'w'), indent=1)
    print('TABLETOP', st['triangles'], 'tris', st['size_m'], 'glow', glow, 'atlas fill', m['atlas_fill'])


def bake_ao_(s, bvh):
    return O.bake_ao(s, bvh, strength=.7, dist=.5, n=10)



def run_guarded(entry):
    """Blender exits 0 when a script raises; every way out of these scripts but success must be non-zero, so an unexpected exception is printed and exits 1.
    (Run Blender with --python-exit-code 1 as well: it covers an error before this point, such as a failed import.)"""
    try:
        entry()
    except SystemExit:
        raise
    except BaseException:
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    run_guarded(build)
