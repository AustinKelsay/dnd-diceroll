"""Shared Root of the World r002 (issue #149) builders: textures and procedural geometry (roots, trunks, stumps, logs, boulders,
fungus, ferns, canopy).  Runs inside Blender 5.2 (numpy only).  All geometry here is original code in metres, +X forward, Z up.

Textures are procedural, periodic where they tile, and sized for the PS1 look at the r002 density: 512 px per 2 m (256 texels/m) for
ground and bark, 256 px per 1 m for stone.  Nothing is taken from outside the repository.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import r002_lib as R
from r002_lib import L, bpy, np, Vector, smoothstep, vnoise, fbm, tri_normals, tile_noise

from prims import Soup, rock


# ------------------------------------------------------------------ textures
def _rgba(rgb):
    t = np.ones((*rgb.shape[:2], 4), np.float32)
    t[..., :3] = np.clip(rgb, 0, 1)
    return t


def _wrap_dot(img, cx, cy, rx, ry, rgb, ang=0.0):
    """Paint a filled ellipse onto a periodic image."""
    n = img.shape[0]
    r = int(max(rx, ry)) + 2
    ys, xs = np.mgrid[-r:r + 1, -r:r + 1]
    ca, sa = math.cos(ang), math.sin(ang)
    u, v = xs * ca + ys * sa, -xs * sa + ys * ca
    m = (u / rx) ** 2 + (v / ry) ** 2 <= 1.0
    yy, xx = (cy + ys[m]) % n, (cx + xs[m]) % n
    img[yy, xx, :3] = rgb


def floor_tile(n=512, size_m=2.0, seed=71):
    """Forest floor: earth, moss and leaf litter, 512 px = 2 m (256 texels/m), seamless."""
    r = np.random.default_rng(seed)
    big, mid, fine = tile_noise(n, 3, seed), tile_noise(n, 9, seed + 1), tile_noise(n, 28, seed + 2)
    soil = np.array((.24, .18, .12)) * (.72 + .5 * big[..., None]) * (.88 + .24 * mid[..., None])
    moss_m = smoothstep(.48, .66, .6 * big + .4 * tile_noise(n, 6, seed + 3))
    moss = np.array((.15, .26, .10)) * (.7 + .6 * mid[..., None]) * (.9 + .2 * fine[..., None])
    img = soil * (1 - moss_m[..., None]) + moss * moss_m[..., None]
    t = _rgba(img)
    px = n / size_m
    leaf_cols = [(.34, .24, .11), (.38, .27, .12), (.26, .19, .09), (.22, .22, .09), (.44, .32, .13), (.18, .26, .09), (.20, .16, .09), (.16, .20, .08)]
    for _ in range(380):
        cx, cy = int(r.integers(0, n)), int(r.integers(0, n))
        s = r.uniform(.025, .06) * px
        _wrap_dot(t, cx, cy, s * r.uniform(1.0, 1.9), s, np.array(leaf_cols[r.integers(len(leaf_cols))]) * r.uniform(.8, 1.15), r.uniform(0, math.pi))
    for _ in range(90):                                            # twigs
        x0, y0 = float(r.integers(0, n)), float(r.integers(0, n))
        a = r.uniform(0, math.tau)
        for k in range(int(r.uniform(.12, .32) * px)):
            _wrap_dot(t, int(x0 + math.cos(a) * k), int(y0 + math.sin(a) * k), 1.4, 1.4, np.array((.18, .12, .08)))
    for _ in range(140):                                           # tiny pale flecks (roots, grit)
        _wrap_dot(t, int(r.integers(0, n)), int(r.integers(0, n)), 2.0, 1.6, np.array((.52, .46, .34)))
    return t


def bark_tile(n=512, size_m=2.0, seed=81):
    """Furrowed bark with vertical ridges, seamless, 512 px = 2 m."""
    r = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:n, 0:n]
    ridge = np.zeros((n, n))
    for k in range(14):                                            # meandering vertical furrows
        x0 = r.uniform(0, n)
        amp, ph = r.uniform(4, 14), r.uniform(0, math.tau)
        wob = x0 + amp * np.sin(yy * (math.tau / n) * r.integers(1, 4) + ph)
        d = np.abs(((xx - wob + n / 2) % n) - n / 2)
        ridge = np.maximum(ridge, np.exp(-(d / r.uniform(3, 7)) ** 2))
    stretch = tile_noise(n, 4, seed + 1)
    big = .5 * tile_noise(n, 6, seed + 2) + .5 * stretch
    base = np.array((.34, .24, .15)) * (.72 + .5 * big[..., None])
    base = base * (1 - .62 * ridge[..., None])
    base = base * (.9 + .2 * tile_noise(n, 40, seed + 3)[..., None])
    moss_m = smoothstep(.62, .78, tile_noise(n, 5, seed + 4)) * .6
    base = base * (1 - moss_m[..., None]) + np.array((.17, .27, .10)) * (.8 + .4 * tile_noise(n, 16, seed + 5)[..., None]) * moss_m[..., None]
    return _rgba(base)


def stone_tile(n=256, seed=91, tint=(.40, .39, .36)):
    big, mid = tile_noise(n, 4, seed), tile_noise(n, 14, seed + 1)
    col = np.array(tint) * (.72 + .5 * big[..., None]) * (.9 + .2 * mid[..., None])
    crack = smoothstep(.47, .5, tile_noise(n, 9, seed + 2)) * (1 - smoothstep(.5, .53, tile_noise(n, 9, seed + 2)))
    col = col * (1 - .4 * crack[..., None])
    lichen = smoothstep(.6, .72, tile_noise(n, 7, seed + 3))
    col = col * (1 - .5 * lichen[..., None]) + np.array((.34, .42, .22)) * .9 * lichen[..., None] * .5
    return _rgba(col)


def pave_tile(seed, base, crack, n=256):
    """One 1 m flagstone texture: soft noise, a couple of hairline cracks (no per-pixel grain)."""
    r = np.random.default_rng(seed)
    n1, n2, n3 = tile_noise(n, 5, seed), tile_noise(n, 14, seed + 9), tile_noise(n, 3, seed + 3)
    b = np.array(base)
    t = _rgba(b * (.80 + .30 * n1[..., None] + .14 * n2[..., None] - .06))
    t[..., :3] *= (1 - .20 * smoothstep(.62, .85, n3))[..., None]
    for _ in range(crack):
        x0, y0 = r.integers(40, n - 40, 2)
        ang = r.uniform(0, math.tau)
        for k in range(int(r.integers(30, 70))):
            x = int(x0 + math.cos(ang + .3 * math.sin(k * .15)) * k)
            y = int(y0 + math.sin(ang + .3 * math.sin(k * .15)) * k)
            if 2 <= x < n - 2 and 2 <= y < n - 2:
                t[y, x, :3] *= .5
    return t


def leaf_tile(n=256, seed=101):
    """Canopy leaf mass: layered dabs of deep green and gold on a dark base, seamless."""
    r = np.random.default_rng(seed)
    t = _rgba(np.broadcast_to(np.array((.10, .20, .07)), (n, n, 3)) * (.7 + .6 * tile_noise(n, 8, seed)[..., None]))
    cols = [(.16, .34, .09), (.22, .44, .12), (.30, .50, .14), (.50, .56, .16), (.58, .50, .14), (.12, .26, .08)]
    for _ in range(420):
        s = r.uniform(5, 12)
        _wrap_dot(t, int(r.integers(0, n)), int(r.integers(0, n)), s * 1.6, s, np.array(cols[r.integers(len(cols))]) * r.uniform(.85, 1.15), r.uniform(0, math.pi))
    return t


def heart_tile(n=128, seed=111):
    """Cut-wood face: annual rings around the centre of the tile."""
    yy, xx = np.mgrid[0:n, 0:n]
    d = np.hypot(xx - n / 2, yy - n / 2) + 3 * tile_noise(n, 4, seed)
    ring = .5 + .5 * np.sin(d * .9)
    col = np.array((.66, .50, .30)) * (.72 + .3 * ring[..., None])
    return _rgba(col)


def cap_tile(n=64, seed=121):
    r = np.random.default_rng(seed)
    t = _rgba(np.broadcast_to(np.array((.55, .20, .12)), (n, n, 3)) * (.8 + .3 * tile_noise(n, 4, seed)[..., None]))
    for _ in range(9):
        _wrap_dot(t, int(r.integers(0, n)), int(r.integers(0, n)), r.uniform(2, 4.5), r.uniform(2, 4), np.array((.92, .86, .70)))
    return t


def fern_tile():
    ramp = np.array([[.55, .72, .30], [.42, .60, .24], [.32, .48, .18], [.24, .36, .13], [.16, .26, .09], [.10, .17, .06]])
    t = np.ones((32, 8, 4), np.float32)
    for r_ in range(32):
        t[r_, :, :3] = ramp[min(5, r_ * 6 // 32)]
    t[:, ::4, :3] *= .85
    return t


def root_textures():
    t = {}
    t['floor'] = floor_tile()
    t['bark'] = bark_tile()
    t['stone'] = stone_tile()
    t['leaf'] = leaf_tile()
    t['heart'] = heart_tile()
    t['cap'] = cap_tile()
    t['fern'] = fern_tile()
    t['paveA'] = pave_tile(21, (.50, .45, .37), 2)
    t['paveB'] = pave_tile(31, (.55, .49, .41), 3)
    t['paveC'] = pave_tile(41, (.45, .44, .38), 1)
    p = pave_tile(51, (.48, .45, .38), 2)
    moss = .75 * smoothstep(.40, .75, tile_noise(256, 3, 52) * .7 + tile_noise(256, 7, 53) * .3)
    p[..., :3] = p[..., :3] * (1 - moss[..., None]) + np.array((.22, .36, .15)) * (.7 + .5 * tile_noise(256, 12, 54))[..., None] * moss[..., None]
    t['paveM'] = p
    t['glowcap'] = L.swatch((.34, .96, .86))
    t['veins'] = L.swatch((1.0, .78, .36))
    t['stem'] = L.swatch((.80, .74, .60))
    return t


# ------------------------------------------------------------------ small geometry helpers
def xform(s, M):
    return L.xform(s, M)


def Rz(deg):
    return L.Rz(deg)


def T(x=0, y=0, z=0):
    return L.T(x, y, z)


def Rx(deg):
    a = math.radians(deg)
    M = np.eye(4)
    M[1, 1], M[1, 2], M[2, 1], M[2, 2] = math.cos(a), -math.sin(a), math.sin(a), math.cos(a)
    return M


def Ry(deg):
    a = math.radians(deg)
    M = np.eye(4)
    M[0, 0], M[0, 2], M[2, 0], M[2, 2] = math.cos(a), math.sin(a), -math.sin(a), math.cos(a)
    return M


def planar_uv_rock(sp, mat, V, F, tile):
    for f in F:
        p = V[list(f)]
        n = np.cross(p[1] - p[0], p[2] - p[0])
        ax = int(np.argmax(np.abs(n)))
        a, b = [i for i in range(3) if i != ax]
        sp.tri(mat, *[tuple(q) for q in p], *[(q[a] / tile, -q[b] / tile) for q in p])


def add_rock(sp, mat, rng, c, r, squash=(1, 1, .7), tile=.5, detail=1, lean=0.0, sink=True, floor=0.0):
    V, F = rock(rng, c, r, squash, detail)
    if lean:
        V[:, 0] += (V[:, 2] - c[2]) * lean
    if sink:
        V[:, 2] = np.maximum(V[:, 2], floor)
    planar_uv_rock(sp, mat, V, F, tile)


def tube_path(sp, mat, pts, radii, sides, tile, cap=(True, True)):
    """sp.tube with the texture repeated around the tube in proportion to its girth (so bark keeps its texel density)."""
    u1 = max(1, round(math.tau * float(np.mean(radii)) / tile)) if tile else 1
    sp.tube(mat, pts, radii, sides, tile=tile, patch=(0, 0, u1, 1), cap=cap)


def root_path(rng, start, heading, length, r0, ground, n=9, wander=.5, hug=.55, taper=.9, stop=None):
    """A root creeping over the ground from `start` along `heading` (radians): points and radii.  `ground(x,y)` is the floor
    height; the tube centre rides at hug x radius above it, so the root half-sinks into the soil."""
    pts, rad = [], []
    x, y, a = start[0], start[1], heading
    seg = length / (n - 1)
    for i in range(n):
        t = i / (n - 1)
        r = max(r0 * (1 - taper * t), .018)
        if stop is not None and stop(x, y):
            break                                          # never creep into the board footprint
        pts.append((x, y, ground(x, y) + r * hug))
        rad.append(r)
        a += rng.uniform(-wander, wander) * (.6 + .6 * t)
        x += math.cos(a) * seg
        y += math.sin(a) * seg
    return pts, rad, a


def root_system(sp, rng, start, heading, length, r0, ground, depth=2, sides=6, tile=1.2, wander=.5, hug=.55, n=9, mat='bark', stop=None):
    """A branching root: main tube plus forks."""
    pts, rad, a = root_path(rng, start, heading, length, r0, ground, n=n, wander=wander, hug=hug, stop=stop)
    if len(pts) < 3:
        return pts, rad
    tube_path(sp, mat, pts, rad, sides, tile, cap=(False, True))
    if depth:
        for k in range(2, len(pts) - 2, max(2, n // 4)):
            if rng.random() < .8:
                local = math.atan2(pts[k + 1][1] - pts[k][1], pts[k + 1][0] - pts[k][0])
                root_system(sp, rng, (pts[k][0], pts[k][1]), local + rng.choice((-1, 1)) * rng.uniform(.5, 1.1),
                            length * rng.uniform(.35, .55), rad[k] * .75, ground, depth - 1, sides, tile, wander, hug, max(5, n - 3), mat, stop)
    return pts, rad


def trunk(sp, rng, c, height, r_base, r_top, lean=(0.0, 0.0), sides=14, tile=2.0, mat='bark', flare=2.2, rings_per_m=.5, broken=False, ground_z=0.0, jag=1.0, flat_top=False):
    """A tapering trunk with a flared, buttressed base and an optional jagged broken top.  Returns the top ring height."""
    n = max(3, int(height * rings_per_m))
    rings, jit = [], []
    for i in range(n + 1):
        t = i / n
        z = ground_z + height * t
        base_f = 1 + (flare - 1) * math.exp(-t * height / 3.0)         # flare in the lowest few metres
        r = (r_base + (r_top - r_base) * t) * base_f
        rings.append((c[0] + lean[0] * t * height, c[1] + lean[1] * t * height, z, r, r))
        flute = [1 + .10 * math.sin(k * 2.1 + i * .7) + (.22 * math.exp(-t * height / 4.0) if k % 3 == 0 else 0) + rng.uniform(-.04, .04) for k in range(sides)]
        jit.append(flute)
    sp.loft(mat, rings, sides, (0, 0, max(1, round(2 * math.pi * r_base / tile)), height / tile), 0.0, (False, False), jit)
    top = rings[-1]
    if flat_top:                                                   # sawn-off top: heartwood face
        rr = top[3]
        cen = (top[0], top[1], top[2])
        for k in range(sides):
            a0, a1 = math.radians(-90) + math.tau * k / sides, math.radians(-90) + math.tau * (k + 1) / sides
            p0 = (top[0] + rr * jit[-1][k] * math.cos(a0), top[1] + rr * jit[-1][k] * math.sin(a0), top[2])
            p1 = (top[0] + rr * jit[-1][(k + 1) % sides] * math.cos(a1), top[1] + rr * jit[-1][(k + 1) % sides] * math.sin(a1), top[2])
            sp.tri('heart', cen, p1, p0, (.5, .5), (.5 + (p1[0] - cen[0]) / (2 * rr + .01), .5 + (p1[1] - cen[1]) / (2 * rr + .01)), (.5 + (p0[0] - cen[0]) / (2 * rr + .01), .5 + (p0[1] - cen[1]) / (2 * rr + .01)))
    if broken:
        # splintered top: raised teeth around a lowered heartwood face
        r = top[3]
        teeth = [(top[0] + r * jit[-1][k] * math.cos(math.radians(-90) + math.tau * k / sides) * .93, top[1] + r * jit[-1][k] * math.sin(math.radians(-90) + math.tau * k / sides) * .93,
                  top[2] + (rng.uniform(.4, 2.6) if k % 2 == 0 else rng.uniform(.0, .5)) * jag) for k in range(sides)]
        ring = [(top[0] + r * jit[-1][k] * math.cos(math.radians(-90) + math.tau * k / sides), top[1] + r * jit[-1][k] * math.sin(math.radians(-90) + math.tau * k / sides), top[2]) for k in range(sides)]
        for k in range(sides):
            a, b, cc, d = ring[k], ring[(k + 1) % sides], teeth[(k + 1) % sides], teeth[k]
            sp.quad(mat, a, b, cc, d, (k / sides * 2, .9), ((k + 1) / sides * 2, .9), ((k + 1) / sides * 2, 1.0), (k / sides * 2, 1.0))
        cen = (top[0], top[1], top[2] - .35 * jag)
        for k in range(sides):
            a, b = teeth[k], teeth[(k + 1) % sides]
            sp.tri('heart', cen, b, a, (.5, .5), (.5 + (b[0] - cen[0]) / (2 * r + .01), .5 + (b[1] - cen[1]) / (2 * r + .01)), (.5 + (a[0] - cen[0]) / (2 * r + .01), .5 + (a[1] - cen[1]) / (2 * r + .01)))
    return top[2]


def buttress_roots(sp, rng, c, r_trunk, height_up, n, ground, mat='bark', reach=2.4, sides=5, tile=1.4):
    """Root fins sweeping out of a trunk's flare and down into the ground."""
    for k in range(n):
        a = math.tau * (k + rng.uniform(-.3, .3)) / n
        d = np.array((math.cos(a), math.sin(a)))
        pts = []
        rad = []
        m = 5
        for i in range(m):
            t = i / (m - 1)
            dist = r_trunk * (.85 + (reach - .85) * t ** .9)
            z0 = height_up * (1 - t) ** 1.6 + ground(c[0] + d[0] * dist, c[1] + d[1] * dist) * t
            rr = max(r_trunk * .16 * (1 - .7 * t), .05)
            pts.append((c[0] + d[0] * dist + rng.uniform(-.1, .1), c[1] + d[1] * dist + rng.uniform(-.1, .1), z0 + rr * .4))
            rad.append(rr)
        tube_path(sp, mat, pts, rad, sides, tile, cap=(False, True))


def fungus_cluster(sp, rng, centre, normal_up=True, n=4, scale=1.0, glow=True, mat_cap='glowcap', ground_z=None):
    """A cluster of small mushrooms: lathe stems and caps.  Glow caps carry alpha 1 downstream (material 'glowcap')."""
    cx, cy, cz = centre
    for _ in range(n):
        x, y = cx + rng.normal(0, .16 * scale), cy + rng.normal(0, .16 * scale)
        h = rng.uniform(.10, .28) * scale
        r = rng.uniform(.06, .14) * scale
        z = cz if ground_z is None else ground_z(x, y)
        sp.lathe('stem', (x, y, z), [(r * .22, 0), (r * .15, h)], 4, tile=None, cap=(False, False), patch=(0, 0, 1, 1))
        cap = [(r * 1.0, h * .98), (r * .8, h * 1.1), (r * .25, h * 1.2)]
        sp.lathe(mat_cap if glow else 'cap', (x, y, z), cap, 5, tile=None, cap=(False, True), patch=(0, 0, 1, 1))


def fern_clump(rng, c, h=.9, w=1.1, blades=6, ground_z=0.0):
    """Cutout-free fern: tapered blade strips (3 tris each, double sided), vertical gradient texture 'fern'."""
    parts = []
    base = np.array((c[0], c[1], ground_z))
    for b in range(blades):
        a = b / blades * math.tau + rng.uniform(0, .6)
        reach = w * .5 * rng.uniform(.6, 1.0)
        tall = h * rng.uniform(.65, 1.0)
        d = np.array((math.cos(a), math.sin(a), 0))
        side = np.array((-d[1], d[0], 0)) * .09 * w
        mid = base + d * reach * .45 + np.array((0, 0, tall * .8))
        tip = base + d * reach + np.array((0, 0, tall * .55))
        p0, p1 = base - side, base + side
        m0, m1 = mid - side * .6, mid + side * .6
        for t, uv in (((p0, p1, m1), ((.5, .95), (.5, .95), (.5, .45))), ((p0, m1, m0), ((.5, .95), (.5, .45), (.5, .45))), ((m0, m1, tip), ((.5, .45), (.5, .45), (.5, .05)))):
            one = dict(P=np.array([t]), UV=np.array([uv]), C=None, mat=np.array(['fern'], dtype=object))
            parts.append(one)
            parts.append(L.flipped(one, .004))
    return L.cat(*parts)


def canopy_blob(sp, rng, c, r, squash=(1, 1, .6), detail=1, mat='leaf', tile=5.0):
    V, F = rock(rng, c, r, squash, detail)
    planar_uv_rock(sp, mat, V, F, tile)


def sap_veins(sp, rng, c, r_trunk, z0, z1, n=5, mat='veins', width=.07):
    """Glowing sap lines running up a trunk (alpha 1 downstream via material 'veins')."""
    for k in range(n):
        a0 = math.tau * k / n + rng.uniform(-.2, .2)
        pts, rad = [], []
        m = 12
        for i in range(m):
            t = i / (m - 1)
            a = a0 + .5 * math.sin(t * 5 + k)
            r = r_trunk * (1.0 + .02) * (1 + (1.1 - 1) * math.exp(-t * (z1 - z0) / 4.0))
            pts.append((c[0] + r * math.cos(a), c[1] + r * math.sin(a), z0 + (z1 - z0) * t))
            rad.append(width * (1 - .5 * t))
        sp.tube(mat, pts, rad, 4, tile=None, cap=(False, False))
