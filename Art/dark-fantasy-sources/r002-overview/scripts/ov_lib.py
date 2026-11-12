"""Shared helpers for the r002 overview set (issue #149): the war table and the three route dioramas.  Runs inside Blender 5.2 (numpy only).

Builds on the r002 helper library (r002_lib / vault_lib / lod_r002, copied under r002-vault/scripts and imported from there) and on
r001's prims.Soup.  Conventions of the overview assets (as SM_Tabletop r001, so the theme's placement code is unchanged): metres, the asset
FACES +X (the game turns it -90 degrees about Z toward the camera), Z up, +Y is the viewer's right, origin = the middle of the table top (tabletop) or
the middle of the underside of the tray (dioramas).  One material, one baseColor atlas (Linear, mipmapped), COLOR_0 as RGBA: RGB = baked light,
A = glow mask (1 on flames and route markers, 0 elsewhere).
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
SRC = os.path.join(ROOT, 'Art', 'dark-fantasy-sources')
VAULT_SCRIPTS = os.path.join(SRC, 'r002-vault', 'scripts')
sys.path.insert(0, VAULT_SCRIPTS)

import r002_lib as R
from r002_lib import L, bpy, np, Vector, make_bvh, tri_normals, smoothstep, subdivide, bake_ao, tri_soup
import vault_lib as VL
from vault_lib import T, Rz, Rx, Ry
from prims import Soup

OUT_SRC = os.path.join(SRC, 'r002-overview')


# ------------------------------------------------------------------ small image toolkit (top-down float RGBA, sRGB-ish values as r002)
def rgba(rgb, a=1.0):
    t = np.empty((*rgb.shape[:2], 4), np.float32)
    t[..., :3] = np.clip(rgb, 0, 1)
    t[..., 3] = a
    return t


def pnoise(h, w, ch, cw, seed):
    """Periodic value noise, ch x cw lattice cells over an h x w image (seamless in both directions)."""
    a = np.random.default_rng(seed).random((ch, cw))
    yy, xx = np.mgrid[0:h, 0:w]
    ys, xs = yy / h * ch, xx / w * cw
    y0, x0 = ys.astype(int) % ch, xs.astype(int) % cw
    fy, fx = ys - np.floor(ys), xs - np.floor(xs)
    fy, fx = fy * fy * (3 - 2 * fy), fx * fx * (3 - 2 * fx)
    y1, x1 = (y0 + 1) % ch, (x0 + 1) % cw
    return (a[y0, x0] * (1 - fx) + a[y0, x1] * fx) * (1 - fy) + (a[y1, x0] * (1 - fx) + a[y1, x1] * fx) * fy


def fbm_tile(h, w, base, seed, octaves=4):
    out, amp, tot = np.zeros((h, w)), 1.0, 0.0
    for o in range(octaves):
        out += amp * pnoise(h, w, base * 2 ** o, max(1, int(base * 2 ** o * w / h)), seed + o)
        tot += amp
        amp *= .5
    return out / tot


def stamp(acc, x, y, r, strength=1.0):
    """Adds a soft round dab to an accumulation buffer (clipped at the borders)."""
    h, w = acc.shape
    x0, x1 = max(0, int(x - r - 1)), min(w, int(x + r + 2))
    y0, y1 = max(0, int(y - r - 1)), min(h, int(y + r + 2))
    if x0 >= x1 or y0 >= y1:
        return
    yy, xx = np.mgrid[y0:y1, x0:x1]
    d = np.hypot(xx - x, yy - y)
    acc[y0:y1, x0:x1] = np.maximum(acc[y0:y1, x0:x1], strength * np.clip(r + .5 - d, 0, 1))


def stroke(acc, pts, r, strength=1.0, step=.6, dash=None):
    """Polyline stroke into an accumulation buffer; dash=(on, off) in pixels."""
    pts = np.asarray(pts, float)
    run = 0.0
    for a, b in zip(pts[:-1], pts[1:]):
        n = max(1, int(np.hypot(*(b - a)) / step))
        for i in range(n + 1):
            p = a + (b - a) * i / n
            if dash:
                on = (run % (dash[0] + dash[1])) < dash[0]
                run += step
                if not on:
                    continue
            stamp(acc, p[0], p[1], r, strength)


def fill_poly(acc, pts, strength=1.0):
    pts = np.asarray(pts, float)
    h, w = acc.shape
    x0, x1 = max(0, int(pts[:, 0].min())), min(w, int(pts[:, 0].max()) + 2)
    y0, y1 = max(0, int(pts[:, 1].min())), min(h, int(pts[:, 1].max()) + 2)
    yy, xx = np.mgrid[y0:y1, x0:x1]
    inside = np.zeros(xx.shape, bool)
    n = len(pts)
    j = n - 1
    for i in range(n):                                      # even-odd rule
        xi, yi, xj, yj = pts[i][0], pts[i][1], pts[j][0], pts[j][1]
        cond = ((yi > yy) != (yj > yy)) & (xx < (xj - xi) * (yy - yi) / (yj - yi + 1e-12) + xi)
        inside ^= cond
        j = i
    acc[y0:y1, x0:x1] = np.maximum(acc[y0:y1, x0:x1], strength * inside)


def ink(img, acc, color, k=1.0):
    a = np.clip(acc * k, 0, 1)[..., None]
    img[..., :3] = img[..., :3] * (1 - a) + np.asarray(color, np.float32) * a


# ------------------------------------------------------------------ textures
def plank_tile(n=512, size_m=2.0, seed=301, plank_cm=18, base=(.37, .22, .12)):
    """Oak planks running along u: seams, butt joints, grain, knots, ring stains and scratches; seamless, 512 px = 2 m (256 texels/m)."""
    rng = np.random.default_rng(seed)
    k = max(4, int(round(size_m * 100 / plank_cm)))
    cuts = np.sort(rng.random(k - 1))
    edges = np.concatenate([[0], cuts, [1]])
    edges = (n * (edges * .5 + np.linspace(0, 1, k + 1) * .5)).astype(int)         # plank widths vary but never collapse
    edges[-1] = n
    yy, xx = np.mgrid[0:n, 0:n]
    img = np.zeros((n, n, 3), np.float64)
    grain_a = fbm_tile(n, n, 2, seed + 3, 3)
    long_n = pnoise(n, n, 40, 3, seed + 4)
    fine = pnoise(n, n, 120, 6, seed + 5)
    for i in range(k):
        y0, y1 = edges[i], edges[i + 1]
        tone = rng.uniform(.8, 1.18)
        tint = np.array(base) * tone * np.array((1.0, rng.uniform(.93, 1.07), rng.uniform(.9, 1.1)))
        rows = slice(y0, y1)
        g = .55 + .45 * np.sin(yy[rows] * .9 + 7 * long_n[rows] + 3 * grain_a[rows])
        v = (.62 + .38 * g) * (.88 + .24 * long_n[rows]) * (.94 + .12 * fine[rows])
        img[rows] = tint[None, None, :] * v[..., None]
        # butt joint: a dark seam across the plank at a random u
        jx = int(rng.integers(0, n))
        for d, f in ((0, .45), (1, .6), (-1, .8)):
            img[rows, (jx + d) % n] *= f
        # knot
        if rng.random() < .55 and y1 - y0 > 14:
            cx, cy = rng.integers(0, n), (y0 + y1) // 2
            r = rng.uniform(3, 6)
            m = np.hypot(xx[rows] - cx, (yy[rows] - cy) * 1.4) < r
            img[rows][m] *= .55
    for e in edges[:-1]:                                    # plank seams: dark gap with a lit lip
        img[e % n] *= .35
        img[(e + 1) % n] *= .8
        img[(e - 1) % n] *= 1.12
    # ring stains and scratches
    acc = np.zeros((n, n))
    for _ in range(5):
        cx, cy, r = rng.uniform(0, n), rng.uniform(0, n), rng.uniform(12, 20)
        ring = np.abs(np.hypot(xx - cx, yy - cy) - r)
        acc = np.maximum(acc, np.clip(1.6 - ring, 0, 1) * rng.uniform(.25, .5))
    ink(img, acc, (.10, .06, .04))
    sc = np.zeros((n, n))
    for _ in range(70):
        x, y = rng.uniform(0, n), rng.uniform(0, n)
        a = rng.normal(0, .12)
        l = rng.uniform(6, 26)
        stroke(sc, [(x, y), (x + math.cos(a) * l, y + math.sin(a) * l)], .55, rng.uniform(.15, .4))
    ink(img, sc, (.62, .46, .30))
    return rgba(img * np.array((1.0, 1.0, 1.0)))


def dark_wood_tile(n=256, seed=311):
    return plank_tile(n, 1.0, seed, plank_cm=11, base=(.16, .10, .07))


def stone_speckle(n=64, seed=321, base=(.40, .40, .42)):
    f = pnoise(n, n, 6, 6, seed) * .6 + pnoise(n, n, 18, 18, seed + 1) * .4
    return rgba(np.array(base) * (.7 + .5 * f[..., None]))


def metal_tile(base, n=64, seed=331, streak=True):
    f = pnoise(n, n, 3, 16 if streak else 6, seed)
    g = np.linspace(.85, 1.1, n)[:, None]
    return rgba(np.array(base) * (.72 + .5 * f[..., None]) * g[..., None])


def book_spines(n=256, seed=341):
    """4 x 4 cells of 64 px: leather spines (oxblood, bottle green, navy, umber, black, ochre) with gilt bands and a title block."""
    rng = np.random.default_rng(seed)
    pal = [(.30, .07, .08), (.09, .22, .14), (.09, .13, .28), (.30, .17, .09), (.07, .06, .07), (.40, .28, .10), (.22, .09, .18), (.16, .20, .20)]
    img = np.zeros((n, n, 3))
    c = n // 4
    for j in range(4):
        for i in range(4):
            base = np.array(pal[int(rng.integers(0, len(pal)))]) * rng.uniform(.8, 1.2)
            cell = np.empty((c, c, 3))
            f = pnoise(c, c, 8, 4, seed + 10 * j + i)
            cell[:] = base * (.75 + .5 * f[..., None])
            for y in (4, 8, c - 9, c - 5):                  # gilt bands
                cell[y:y + 2] = np.array((.78, .60, .24)) * rng.uniform(.8, 1.0)
            cell[c // 2 - 8:c // 2 + 8, 14:c - 14] = np.array((.74, .64, .42)) * rng.uniform(.6, .9)   # title label
            cell[c // 2 - 4:c // 2 + 4, 18:c - 18:3] *= .45
            img[j * c:(j + 1) * c, i * c:(i + 1) * c] = cell
    return rgba(img)


def vellum_base(w, h, seed=351):
    """Cream vellum: fibres, mottling, darker edges and a few stains.  Returns RGB in [0,1]."""
    f1 = fbm_tile(h, w, 3, seed, 4)
    f2 = pnoise(h, w, 40, 60, seed + 9)
    yy, xx = np.mgrid[0:h, 0:w]
    ex = np.minimum(xx, w - 1 - xx) / (w * .5)
    ey = np.minimum(yy, h - 1 - yy) / (h * .5)
    edge = np.clip(np.minimum(ex, ey) * 5.5, 0, 1)
    base = np.array((.80, .68, .46))[None, None, :] * (.82 + .26 * f1[..., None]) * (.94 + .1 * f2[..., None])
    base *= (.62 + .38 * edge[..., None]) * np.array((1.0, .96, .88))[None, None, :] ** (1 - edge[..., None])
    return base


def war_map(w=640, h=384, seed=361):
    """The route, drawn: coast and river, forest, mountains, the ruined watchtower, the vault and a dashed route through five waystations
    with a fork between the second and fourth.  Returns (rgba texture, nodes in 0..1 u,v coordinates) - the nodes carry 3D markers."""
    rng = np.random.default_rng(seed)
    img = vellum_base(w, h, seed)
    s = w / 640.0
    inkc, red, wash = (.16, .10, .06), (.45, .08, .06), (.42, .50, .52)
    # sea wash along the left edge, a river from the mountains to the sea
    acc = np.zeros((h, w))
    coast = [(0, 0), (60 * s, 0), (82 * s, 60 * s), (62 * s, 120 * s), (74 * s, 200 * s), (48 * s, 280 * s), (64 * s, 340 * s), (46 * s, h), (0, h)]
    fill_poly(acc, coast, .55)
    n1 = pnoise(h, w, 10, 14, seed + 2)
    acc *= .75 + .25 * n1
    ink(img, acc, wash, .85)
    shore = np.zeros((h, w))
    stroke(shore, coast[1:-1], 1.1, .9)
    ink(img, shore, inkc, .8)
    river = [(520 * s, 20 * s), (470 * s, 90 * s), (430 * s, 150 * s), (380 * s, 190 * s), (310 * s, 240 * s), (240 * s, 270 * s), (170 * s, 300 * s), (82 * s, 330 * s)]
    r_acc = np.zeros((h, w))
    stroke(r_acc, river, 2.4 * s, .8)
    ink(img, r_acc, wash, .9)
    # mountains (top right), forest (lower left), the watchtower ruins (centre)
    mt = np.zeros((h, w))
    for _ in range(26):
        cx, cy, m = rng.uniform(330, 610) * s, rng.uniform(20, 110) * s, rng.uniform(9, 17) * s
        stroke(mt, [(cx - m, cy + m * .8), (cx, cy - m * .3), (cx + m, cy + m * .8)], 1.0, .9)
        stroke(mt, [(cx, cy - m * .3), (cx + m * .25, cy + m * .8)], .6, .5)
    ink(img, mt, inkc)
    tree = np.zeros((h, w))
    for _ in range(46):
        cx, cy = rng.uniform(100, 300) * s, rng.uniform(210, 360) * s
        m = rng.uniform(6, 10) * s
        fill_poly(tree, [(cx - m * .6, cy), (cx, cy - m * 1.5), (cx + m * .6, cy)], .85)
        stroke(tree, [(cx, cy), (cx, cy + m * .6)], .9, .9)
    ink(img, tree, (.14, .26, .12))
    ruin = np.zeros((h, w))
    tx, ty = 300 * s, 150 * s
    fill_poly(ruin, [(tx - 9 * s, ty + 24 * s), (tx - 9 * s, ty - 14 * s), (tx - 5 * s, ty - 14 * s), (tx - 5 * s, ty - 19 * s), (tx - 1 * s, ty - 19 * s), (tx - 1 * s, ty - 14 * s),
                     (tx + 3 * s, ty - 14 * s), (tx + 3 * s, ty - 19 * s), (tx + 7 * s, ty - 19 * s), (tx + 7 * s, ty - 14 * s), (tx + 9 * s, ty - 14 * s), (tx + 9 * s, ty + 24 * s)], .9)
    ink(img, ruin, (.26, .22, .20))
    # the vault: a squat crypt with a doorway, top right
    vx, vy = 548 * s, 190 * s
    vt = np.zeros((h, w))
    fill_poly(vt, [(vx - 22 * s, vy + 18 * s), (vx - 22 * s, vy - 6 * s), (vx, vy - 20 * s), (vx + 22 * s, vy - 6 * s), (vx + 22 * s, vy + 18 * s)], .9)
    ink(img, vt, (.22, .24, .30))
    door = np.zeros((h, w))
    fill_poly(door, [(vx - 6 * s, vy + 18 * s), (vx - 6 * s, vy + 2 * s), (vx, vy - 4 * s), (vx + 6 * s, vy + 2 * s), (vx + 6 * s, vy + 18 * s)], .95)
    ink(img, door, (.05, .06, .09))
    # route: five waystations; a solid dashed ink route and the alternative road (fork between 2 and 4)
    nodes = [(112, 300), (190, 238), (300, 150), (400, 232), (548, 190)]
    alt = [(190, 238), (244, 296), (352, 312), (400, 232)]
    main = [(112, 300), (150, 262), (190, 238), (236, 200), (300, 150), (352, 176), (400, 232), (468, 236), (548, 190)]
    a_acc = np.zeros((h, w))
    stroke(a_acc, [(x * s, y * s) for x, y in alt], 1.0, .75, dash=(7, 5))
    ink(img, a_acc, inkc, .75)
    m_acc = np.zeros((h, w))
    stroke(m_acc, [(x * s, y * s) for x, y in main], 1.5, .95, dash=(10, 5))
    ink(img, m_acc, red)
    mk = np.zeros((h, w))
    for x, y in nodes:
        for r in (7.5, 6.2):
            ring = np.zeros((h, w))
            for ang in np.linspace(0, math.tau, 40):
                stamp(ring, x * s + r * s * math.cos(ang), y * s + r * s * math.sin(ang), .8, 1.0)
            mk = np.maximum(mk, ring)
    ink(img, mk, red)
    # compass rose (lower right) and a double border
    cr = np.zeros((h, w))
    cx, cy, R_ = 585 * s, 320 * s, 24 * s
    for ang, ln in ((0, 1.0), (90, .62), (180, .62), (270, .62)):
        a = math.radians(ang - 90)
        tip = (cx + math.cos(a) * R_ * ln, cy + math.sin(a) * R_ * ln)
        fill_poly(cr, [tip, (cx + math.cos(a + 1.5708) * 4 * s, cy + math.sin(a + 1.5708) * 4 * s), (cx + math.cos(a - 1.5708) * 4 * s, cy + math.sin(a - 1.5708) * 4 * s)], .95)
    ring = np.zeros((h, w))
    for ang in np.linspace(0, math.tau, 90):
        stamp(ring, cx + R_ * .72 * math.cos(ang), cy + R_ * .72 * math.sin(ang), .7, .9)
    ink(img, np.maximum(cr, ring), inkc)
    bd = np.zeros((h, w))
    for off, r in ((8 * s, 1.5), (13 * s, .8)):
        stroke(bd, [(off, off), (w - off, off), (w - off, h - off), (off, h - off), (off, off)], r, .85)
    ink(img, bd, inkc)
    # creases (a fold grid), light stains, a burnt corner
    cr2 = np.zeros((h, w))
    for x in (w / 3, 2 * w / 3):
        stroke(cr2, [(x + rng.normal(0, 1.5), 0), (x + rng.normal(0, 1.5), h)], .6, .35)
    stroke(cr2, [(0, h / 2), (w, h / 2 + 3)], .6, .35)
    ink(img, cr2, (.30, .20, .10))
    yy, xx = np.mgrid[0:h, 0:w]
    burn = np.clip(1.2 - np.hypot(xx - w, yy - h) / (46 * s), 0, 1) * (.6 + .4 * pnoise(h, w, 12, 18, seed + 7))
    ink(img, burn, (.06, .04, .03), .85)
    # nodes in uv (top-down v)
    uvn = [(x * s / w, y * s / h) for x, y in nodes]
    return rgba(img), uvn


def die_pips(n=96):
    """d6 texture: 3 x 2 cells of 32 px, ivory with dark pips (1..6)."""
    c = n // 3
    img = np.empty((2 * c, n, 3))
    img[:] = np.array((.86, .82, .70))
    f = pnoise(2 * c, n, 4, 6, 77)
    img *= (.9 + .15 * f[..., None])
    layouts = {1: [(.5, .5)], 2: [(.3, .3), (.7, .7)], 3: [(.28, .28), (.5, .5), (.72, .72)], 4: [(.3, .3), (.7, .3), (.3, .7), (.7, .7)],
              5: [(.3, .3), (.7, .3), (.5, .5), (.3, .7), (.7, .7)], 6: [(.3, .25), (.7, .25), (.3, .5), (.7, .5), (.3, .75), (.7, .75)]}
    acc = np.zeros((2 * c, n))
    for k, pips in layouts.items():
        j, i = (k - 1) // 3, (k - 1) % 3
        for px, py in pips:
            stamp(acc, i * c + px * c, j * c + py * c, 2.3, 1.0)
    ink(img, acc, (.10, .05, .04))
    return rgba(img)


def die_glyph(base, mark, n=64, seed=81):
    """d20/d8 face texture: a resin tone with a gilt triangle glyph in the middle."""
    f = pnoise(n, n, 4, 4, seed)
    img = np.empty((n, n, 3))
    img[:] = np.array(base) * (.82 + .3 * f[..., None])
    acc = np.zeros((n, n))
    fill_poly(acc, [(n * .5, n * .26), (n * .7, n * .64), (n * .3, n * .64)], .9)
    fill_poly(acc, [(n * .5, n * .40), (n * .6, n * .58), (n * .4, n * .58)], -.6)
    ink(img, np.clip(acc, 0, 1), mark)
    return rgba(img)


def wax_tile(n=32, seed=371):
    f = pnoise(n, n, 4, 4, seed)
    return rgba(np.array((.88, .80, .62)) * (.86 + .22 * f[..., None]))


def leather_tile(n=64, seed=381, base=(.28, .16, .09)):
    f = pnoise(n, n, 12, 12, seed) * .6 + pnoise(n, n, 28, 28, seed + 1) * .4
    return rgba(np.array(base) * (.7 + .6 * f[..., None]))


def cloth_tile(n=64, seed=391, base=(.40, .34, .26)):
    f = pnoise(n, n, 16, 16, seed)
    weave = .92 + .08 * (np.indices((n, n)).sum(0) % 2)
    return rgba(np.array(base) * (.78 + .4 * f[..., None]) * weave[..., None])


# ------------------------------------------------------------------ geometry helpers (Soup builders, metres)
def uvquad(sp, mat, a, b, c, d, rect):
    u0, v0, u1, v1 = rect
    sp.quad(mat, a, b, c, d, (u0, v0), (u1, v0), (u1, v1), (u0, v1))


def cbox(sp, c, s, mats, rects=None, bottom=True):
    """Box centred at c, size s; mats: one name or a dict face -> name (px, nx, py, ny, top, bot); rects: face -> (u0, v0, u1, v1) (default 0..1)."""
    cx, cy, cz = c
    sx, sy, sz = s[0] / 2, s[1] / 2, s[2] / 2
    fx, fy, fz = 2 * sx, 2 * sy, 2 * sz
    faces = {
        'ny': ((cx - sx, cy - sy, cz - sz), (fx, 0, 0), (0, 0, fz)),
        'py': ((cx + sx, cy + sy, cz - sz), (-fx, 0, 0), (0, 0, fz)),
        'px': ((cx + sx, cy - sy, cz - sz), (0, fy, 0), (0, 0, fz)),
        'nx': ((cx - sx, cy + sy, cz - sz), (0, -fy, 0), (0, 0, fz)),
        'top': ((cx - sx, cy - sy, cz + sz), (fx, 0, 0), (0, fy, 0)),
        'bot': ((cx - sx, cy + sy, cz - sz), (fx, 0, 0), (0, -fy, 0)),
    }
    for name, (o, du, dv) in faces.items():
        if name == 'bot' and not bottom:
            continue
        mat = mats if isinstance(mats, str) else mats[name]
        r = (rects or {}).get(name, (0, 0, 1, 1))
        o, du, dv = np.array(o, float), np.array(du, float), np.array(dv, float)
        uvquad(sp, mat, tuple(o), tuple(o + du), tuple(o + du + dv), tuple(o + dv), r)


def place(sp_soup, M):
    return L.xform(sp_soup, M)


def prop_soup(builder):
    sp = Soup()
    builder(sp)
    return sp.soup()


def pot(sp, mat, c, prof, sides=10, tile=None):
    sp.lathe(mat, c, prof, sides, tile=tile)


def die6(c, size, yaw=0.0, mat='d6'):
    """A cube die (returns a soup): each face uses one of the 3 x 2 pip cells."""
    h = size / 2
    M = np.array(Rz(yaw))
    cells = {'px': 0, 'ny': 1, 'top': 2, 'bot': 3, 'nx': 4, 'py': 5}
    sub = Soup()
    rects = {k: ((i % 3) / 3, (i // 3) / 2, (i % 3 + 1) / 3, (i // 3 + 1) / 2) for k, i in cells.items()}
    cbox(sub, (0, 0, 0), (size, size, size), mat, rects)
    return L.xform(sub.soup(), np.array(T(c[0], c[1], c[2] + h)) @ M)


def tri_faces(sp, mat, V, F, uv=((.5, .08), (.92, .9), (.08, .9))):
    for f in F:
        a, b, c = (tuple(V[i]) for i in f)
        sp.tri(mat, a, b, c, uv[0], uv[1], uv[2])


def icosa(r):
    t = (1 + 5 ** .5) / 2
    V = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t), (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
    F = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
         (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    V = np.array([np.array(v, float) / np.linalg.norm(v) for v in V]) * r
    return V, F


def octa(r):
    V = np.array([(1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1)], float) * r
    F = [(0, 2, 4), (2, 1, 4), (1, 3, 4), (3, 0, 4), (2, 0, 5), (1, 2, 5), (3, 1, 5), (0, 3, 5)]
    return V, F


def rest_on_floor(V, z0=0.0):
    V = np.array(V, float)
    V[:, 2] += z0 - V[:, 2].min()
    return V


# ------------------------------------------------------------------ atlas for composed (imported) pieces
class Atlas:
    """Shelf-packs whole images (with their own padding) into a square atlas and remaps top-down (u, v) in 0..1 of a source image."""

    def __init__(self, size, bg=(.06, .05, .05)):
        self.size = size
        self.img = np.zeros((size, size, 4), np.float32)
        self.img[..., :3] = bg
        self.img[..., 3] = 1
        self.x = self.y = self.row = 0
        self.regions = {}

    def add(self, key, arr, down=1, pad=4, repeat=(1, 1)):
        a = np.asarray(arr, np.float32)
        if down > 1:
            h, w = a.shape[:2]
            a = a[:h // down * down, :w // down * down].reshape(h // down, down, w // down, down, a.shape[2]).mean((1, 3))
        if repeat != (1, 1):
            a = np.tile(a, (repeat[1], repeat[0], 1))
        h, w = a.shape[:2]
        W, H = w + 2 * pad, h + 2 * pad
        if self.x + W > self.size:
            self.x, self.y, self.row = 0, self.y + self.row, 0
        if self.y + H > self.size:
            raise ValueError(f'atlas overflow at {key}')
        self.img[self.y:self.y + H, self.x:self.x + W] = np.pad(a, ((pad, pad), (pad, pad), (0, 0)), mode='wrap')
        self.regions[key] = (self.x + pad, self.y + pad, w, h)
        self.x += W
        self.row = max(self.row, H)
        self.img[..., 3] = 1
        return self.regions[key]

    def remap(self, key, uv_topdown, base=(1, 1)):
        x0, y0, w, h = self.regions[key]
        u, v = uv_topdown[..., 0], uv_topdown[..., 1]
        out = np.empty_like(uv_topdown)
        out[..., 0] = (x0 + u / base[0] * w) / self.size
        out[..., 1] = 1 - (y0 + v / base[1] * h) / self.size
        return out

    def fill_fraction(self, pad=4):
        return sum((w + 2 * pad) * (h + 2 * pad) for (_, _, w, h) in self.regions.values()) / self.size ** 2


# ------------------------------------------------------------------ glb in, attribute arrays out
def read_glb_arrays(path):
    """Like lod_r002.read_glb but returns the atlas as a float RGBA array too: P (n,3,3) Blender axes, UV (n,3,2) top-down (v as stored in the
    glTF), COL (n,3,4) linear float, image (h, w, 4) float 0..1."""
    import lod_r002 as LOD
    sys.path.insert(0, os.path.join(SRC, 'r002-rose', 'scripts'))
    import rose_lib
    P, UV, COL, png_path = LOD.read_glb(path)
    UV = np.stack([UV[..., 0], 1.0 - UV[..., 1]], -1)
    img = rose_lib.png_decode(open(png_path, 'rb').read()).astype(np.float32) / 255
    return P, UV, COL, img


def clip_attr(tris, x0, x1, y0, y1):
    """Clips triangles to an XY rectangle, carrying every column of the (n, 3, D) array (position first) through Sutherland-Hodgman."""
    P = tris[..., :3]
    inside_all = np.all((P[..., 0] >= x0) & (P[..., 0] <= x1) & (P[..., 1] >= y0) & (P[..., 1] <= y1), 1)
    outside = (np.all(P[..., 0] <= x0, 1) | np.all(P[..., 0] >= x1, 1) | np.all(P[..., 1] <= y0, 1) | np.all(P[..., 1] >= y1, 1))
    out = [tris[inside_all]]
    for i in np.nonzero(~inside_all & ~outside)[0]:
        poly = list(tris[i])
        for axis, k, less in ((0, x0, False), (0, x1, True), (1, y0, False), (1, y1, True)):
            poly = L._clip_poly(poly, axis, k, less)
            if len(poly) < 3:
                break
        if len(poly) >= 3:
            out.append(np.array([(poly[0], poly[j], poly[j + 1]) for j in range(1, len(poly) - 1)]))
    return np.concatenate(out) if len(out) > 1 else out[0]
