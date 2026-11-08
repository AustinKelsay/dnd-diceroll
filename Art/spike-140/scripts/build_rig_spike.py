"""#140 spike: rigged fighter pilot -> Art/spike-140/exports/SK_Hero_FighterSpike.glb

Run from the repository root:  blender -b --python Art/spike-140/scripts/build_rig_spike.py

One shared humanoid skeleton (25 bones), a 180 cm low-poly fighter in the r001 fighter's visual style
(dark blue steel with pale scratch lines, gold trim, crimson cape, kite shield, longsword), one material
on a 1024x1024 atlas, and two scripted actions: Idle (2.0 s loop) and Walk (in place, 1.0 s loop), 30 fps.
Facing +X, Z up, metres, pivot between the feet (same convention as the r001 exports).

Weighting is rigid per segment (PS1 feel) with one blended ring at the elbows, knees, waist and neck and a
smoothly weighted cape; plate armour (pauldrons, faulds, cops, greaves, helm) is 100% rigid to one bone.
"""
import json
import math
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
OUT = os.path.join(ROOT, 'Art', 'spike-140', 'exports')
os.environ.setdefault('THEME_LOCAL', tempfile.mkdtemp(prefix='spike140_'))
sys.path.insert(0, os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r001', 'scripts'))

import bpy
import numpy as np
from mathutils import Euler, Matrix, Vector

import tlib as L

FPS = 30
ATLAS = 1024
TILE = 256

# ------------------------------------------------------------------ skeleton
# name: (parent, head, tail)  armature space = model space (X forward, Y left, Z up), metres
def build_joints():
    J = {}
    J['root'] = (None, (0, 0, 0), (0, 0, .15))
    J['pelvis'] = ('root', (0, 0, .95), (0, 0, 1.02))
    J['spine1'] = ('pelvis', (0, 0, 1.02), (0, 0, 1.16))
    J['spine2'] = ('spine1', (0, 0, 1.16), (0, 0, 1.30))
    J['chest'] = ('spine2', (0, 0, 1.30), (0, 0, 1.46))
    J['neck'] = ('chest', (0, 0, 1.46), (0, 0, 1.54))
    J['head'] = ('neck', (0, 0, 1.54), (0, 0, 1.76))
    for sg, s in ((1, 'L'), (-1, 'R')):
        d = np.array((0, sg * math.sin(math.radians(22)), -math.cos(math.radians(22))))
        sh = np.array((0, sg * .22, 1.42))
        el = sh + .30 * d
        wr = el + .27 * d
        J[f'clavicle.{s}'] = ('chest', (0, sg * .05, 1.42), (0, sg * .20, 1.44))
        J[f'upperarm.{s}'] = (f'clavicle.{s}', tuple(sh), tuple(el))
        J[f'forearm.{s}'] = (f'upperarm.{s}', tuple(el), tuple(wr))
        J[f'hand.{s}'] = (f'forearm.{s}', tuple(wr), tuple(wr + .10 * d))
        J[f'thigh.{s}'] = ('pelvis', (0, sg * .10, .93), (0, sg * .10, .50))
        J[f'shin.{s}'] = (f'thigh.{s}', (0, sg * .10, .50), (0, sg * .10, .09))
        J[f'foot.{s}'] = (f'shin.{s}', (0, sg * .10, .09), (.15, sg * .10, .04))
        J[f'toe.{s}'] = (f'foot.{s}', (.15, sg * .10, .04), (.24, sg * .10, .03))
    J['weapon_socket'] = ('hand.R', (0, -.47, .80), (.12, -.47, .86))
    J['shield_socket'] = ('hand.L', (0, .50, .82), (0, .58, .82))
    return J


JOINTS = build_joints()
BONE_ORDER = list(JOINTS)


# ------------------------------------------------------------------ atlas (procedural, numpy)
REGION = {  # name: (col, row) top-down, each TILE x TILE
    'steel': (0, 0), 'gold': (1, 0), 'cape': (2, 0), 'leather': (3, 0),
    'blade': (0, 1), 'glow': (1, 1), 'cloth': (2, 1), 'shield': (3, 1),
}


def smooth_noise(rng, n, cells):
    a = rng.random((cells, cells))
    a = np.kron(a, np.ones((n // cells, n // cells)))
    f = np.fft.fft2(a)
    fx = np.fft.fftfreq(n)[:, None]
    fy = np.fft.fftfreq(n)[None, :]
    k = np.exp(-(fx ** 2 + fy ** 2) * (n / cells) ** 2 * 6)
    return np.real(np.fft.ifft2(f * k))


def make_atlas():
    rng = np.random.default_rng(140)
    A = np.zeros((ATLAS, ATLAS, 4), np.float32)
    A[..., :3] = (.08, .07, .09)
    A[..., 3] = 1
    n = TILE
    yy, xx = np.mgrid[0:n, 0:n]
    tiles = {}
    nz = smooth_noise(rng, n, 16)
    nz = (nz - nz.min()) / (np.ptp(nz) + 1e-9)
    # steel: navy plate, pale horizontal bands (as the r001 cuirass lines), scratches
    t = np.ones((n, n, 4), np.float32)
    base = np.array((.105, .135, .215))
    t[..., :3] = base * (.8 + .4 * nz[..., None])
    for y0 in range(0, n, 64):
        t[y0:y0 + 2, :, :3] = np.array((.58, .66, .80)) * (.7 + .3 * nz[y0:y0 + 2, :, None])
        t[y0 + 2:y0 + 4, :, :3] *= .75
    for _ in range(40):
        x0, y0, ln = rng.integers(0, n), rng.integers(0, n), rng.integers(10, 40)
        t[y0 % n, x0:x0 + ln, :3] = np.minimum(1, t[y0 % n, x0:x0 + ln, :3] * 1.6)
    tiles['steel'] = t
    # gold trim with a scale pattern
    t = np.ones((n, n, 4), np.float32)
    cell = ((xx // 16 + yy // 16) % 2)[..., None]
    t[..., :3] = np.where(cell == 0, np.array((.80, .56, .20)), np.array((.62, .42, .14))) * (.85 + .3 * nz[..., None])
    tiles['gold'] = t
    # crimson cape with long vertical folds
    t = np.ones((n, n, 4), np.float32)
    fold = .5 + .5 * np.sin(xx * 2 * math.pi / 32 + 2.5 * nz)
    t[..., :3] = np.array((.52, .06, .08)) * (.55 + .6 * fold[..., None]) * (.9 + .2 * nz[..., None])
    tiles['cape'] = t
    # leather
    t = np.ones((n, n, 4), np.float32)
    t[..., :3] = np.array((.28, .16, .09)) * (.7 + .6 * nz[..., None])
    t[::24, :, :3] *= .6
    tiles['leather'] = t
    # blade: light steel with a ridge line down the middle
    t = np.ones((n, n, 4), np.float32)
    t[..., :3] = np.array((.58, .65, .76)) * (.85 + .25 * nz[..., None])
    t[:, n // 2 - 2:n // 2 + 2, :3] = .9
    t[:, :6, :3] *= .6
    t[:, -6:, :3] *= .6
    tiles['blade'] = t
    # glow (visor slit)
    t = np.ones((n, n, 4), np.float32)
    t[..., :3] = (1.0, .45, .12)
    tiles['glow'] = t
    # dark cloth
    t = np.ones((n, n, 4), np.float32)
    t[..., :3] = np.array((.12, .09, .11)) * (.7 + .6 * nz[..., None])
    tiles['cloth'] = t
    # shield face: navy field, gold border, red four-point star
    t = np.ones((n, n, 4), np.float32)
    t[..., :3] = np.array((.09, .12, .20)) * (.8 + .4 * nz[..., None])
    u = (xx + .5) / n * 2 - 1
    v = (yy + .5) / n * 2 - 1
    border = (np.abs(u) > .9) | (v < -.93) | (v > .93)
    t[border, :3] = (.80, .56, .20)
    star = (np.abs(u) ** .6 + np.abs(v - .0) ** .6) < .55
    t[star, :3] = (.70, .08, .09)
    tiles['shield'] = t
    for name, (c, r) in REGION.items():
        A[r * TILE:(r + 1) * TILE, c * TILE:(c + 1) * TILE] = tiles[name]
    return A


def region_uv(name, u, v):
    """u,v in 0..1 inside the region -> atlas uv (Blender bottom-up v); 2 px inset against bleeding."""
    c, r = REGION[name]
    inset = 2.0 / ATLAS
    x0, y0 = c * TILE / ATLAS + inset, r * TILE / ATLAS + inset
    w = TILE / ATLAS - 2 * inset
    return (x0 + np.clip(u, 0, 1) * w, 1 - (y0 + (1 - np.clip(v, 0, 1)) * w))


# ------------------------------------------------------------------ geometry accumulation
class Body:
    def __init__(self):
        self.V, self.UV, self.F, self.W = [], [], [], []   # UV per vertex (seams duplicated)

    def add(self, P, UV, F, W, convex=True):
        P = np.asarray(P, float)
        F = np.asarray(F, int)
        if convex:
            cen = P.mean(0)
            a, b, c = P[F[:, 0]], P[F[:, 1]], P[F[:, 2]]
            n = np.cross(b - a, c - a)
            out = ((a + b + c) / 3 - cen)
            flip = (n * out).sum(1) < 0
            F = F.copy()
            F[flip] = F[flip][:, [0, 2, 1]]
        base = len(self.V)
        self.V.extend(P.tolist())
        self.UV.extend(UV)
        self.W.extend(W)
        self.F.extend((F + base).tolist())


def weights_for(spec, p):
    return spec(p) if callable(spec) else {spec: 1.0}


def blend_chain(axis_fn, links, sgn=1):
    """links: [(bone, breakpoint, halfwidth)] along the parameter axis_fn(p), which increases from the first bone
    to the last when sgn=1 (use sgn=-1 when it decreases, e.g. height down a leg). The handover to each later
    bone is a linear ramp of width 2*halfwidth around its breakpoint; halfwidth 0 is a rigid split."""
    def f(p):
        t = sgn * axis_fn(p)
        k = []
        for _, bp, h in links[1:]:
            bp = sgn * bp
            k.append((1.0 if t >= bp else 0.0) if h == 0 else float(np.clip((t - bp + h) / (2 * h), 0, 1)))
        w = {}
        prev = 1.0
        for (b, _, _), ki in zip(links, k + [0.0]):
            w[b] = prev - ki
            prev = ki
        return {b: x for b, x in w.items() if x > 1e-4}
    return f


def frame(t, ref):
    t = t / np.linalg.norm(t)
    s = np.cross(t, ref)
    if np.linalg.norm(s) < 1e-6:
        s = np.cross(t, (0, 1, 0))
    s /= np.linalg.norm(s)
    f = np.cross(s, t)
    return s, f


def loft(body, path, ra, rb, sides, region, wspec, caps=(True, True), ref=(1, 0, 0), vrange=(0, 1), convex=True, dense=1):
    """Rings along a polyline. ra: radius along the side axis, rb along the forward axis.
    dense: insert dense-1 linearly interpolated rings between neighbours (more skinning resolution)."""
    path = [np.array(p, float) for p in path]
    if dense > 1:
        def up(a):
            a = list(a)
            out = []
            for x, y in zip(a, a[1:]):
                out += [x + (y - x) * i / dense for i in range(dense)]
            return out + [a[-1]]
        path, ra, rb = up(path), up(ra), up(rb)
    n = len(path)
    length = [0.0]
    for a, b in zip(path, path[1:]):
        length.append(length[-1] + np.linalg.norm(b - a))
    P, UV, W = [], [], []
    for i in range(n):
        tan = path[min(i + 1, n - 1)] - path[max(i - 1, 0)]
        s, f = frame(tan, np.array(ref, float))
        for k in range(sides + 1):
            a = 2 * math.pi * k / sides
            p = path[i] + f * rb[i] * math.cos(a) + s * ra[i] * math.sin(a)
            P.append(p)
            vv = vrange[0] + (vrange[1] - vrange[0]) * length[i] / (length[-1] or 1)
            UV.append(region_uv(region, k / sides, vv))
            W.append(weights_for(wspec, p))
    F = []
    row = sides + 1
    for i in range(n - 1):
        for k in range(sides):
            a, b, c, d = i * row + k, i * row + k + 1, (i + 1) * row + k + 1, (i + 1) * row + k
            F += [(a, b, c), (a, c, d)]
    for cap, idx in ((caps[0], 0), (caps[1], n - 1)):
        if cap:
            ci = len(P)
            P.append(path[idx])
            UV.append(region_uv(region, .5, .5))
            W.append(weights_for(wspec, path[idx]))
            for k in range(sides):
                F.append((ci, idx * row + k + 1, idx * row + k) if idx == 0 else (ci, idx * row + k, idx * row + k + 1))
    body.add(P, UV, F, W, False)


def box(body, c, size, region, wspec, rot=None, uvr=(0, 0, 1, 1)):
    sx, sy, sz = (np.array(size) / 2)
    c = np.array(c, float)
    R = np.eye(3) if rot is None else np.array(rot)
    faces = [((1, 0, 0), (0, 1, 0), (0, 0, 1)), ((-1, 0, 0), (0, -1, 0), (0, 0, 1)),
             ((0, 1, 0), (-1, 0, 0), (0, 0, 1)), ((0, -1, 0), (1, 0, 0), (0, 0, 1)),
             ((0, 0, 1), (1, 0, 0), (0, 1, 0)), ((0, 0, -1), (1, 0, 0), (0, -1, 0))]
    P, UV, W, F = [], [], [], []
    ext = np.array((sx, sy, sz))
    for nrm, ua, va in faces:
        nrm, ua, va = np.array(nrm), np.array(ua), np.array(va)
        for du, dv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            p = c + R @ (nrm * ext + ua * du * ext + va * dv * ext)
            P.append(p)
            UV.append(region_uv(region, uvr[0] + (du + 1) / 2 * (uvr[2] - uvr[0]), uvr[1] + (dv + 1) / 2 * (uvr[3] - uvr[1])))
            W.append(weights_for(wspec, p))
        b = len(P) - 4
        F += [(b, b + 1, b + 2), (b, b + 2, b + 3)]
    body.add(P, UV, F, W)


def spike(body, base, direction, length, r, region, wspec, sides=5):
    d = np.array(direction, float)
    d /= np.linalg.norm(d)
    loft(body, [base, np.array(base) + d * length], [r, 0.0], [r, 0.0], sides, region, wspec, caps=(True, False), ref=(0, 0, 1), vrange=(0, 1))


def build_mesh():
    B = Body()
    # --- torso, waist, hips
    zs = [.94, 1.00, 1.08, 1.17, 1.26, 1.36, 1.44, 1.49]
    ra = [.175, .18, .165, .17, .21, .245, .25, .13]
    rb = [.12, .125, .115, .125, .15, .17, .165, .10]
    wt = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.06, .04), ('spine2', 1.18, .05), ('chest', 1.30, .05)])
    loft(B, [(0, 0, z) for z in zs], ra, rb, 12, 'steel', wt, vrange=(0, 1), dense=2)
    # belt + buckle
    loft(B, [(0, 0, 1.02), (0, 0, 1.07)], [.19, .19], [.135, .135], 12, 'gold', 'pelvis', vrange=(0, 1))
    box(B, (.14, 0, 1.045), (.03, .07, .07), 'gold', 'pelvis')
    # chest plate keel and gorget
    box(B, (.17, 0, 1.36), (.04, .05, .20), 'gold', 'chest')
    loft(B, [(0, 0, 1.46), (0, 0, 1.52)], [.15, .09], [.12, .08], 12, 'steel', blend_chain(lambda p: p[2], [('chest', 0, 0), ('neck', 1.50, .02)]), vrange=(0, 1))
    # hip armour: faulds front/back/sides, rigid to the pelvis
    for sg in (1, -1):
        box(B, (.15, sg * .08, .86), (.035, .13, .17), 'steel', 'pelvis', rot=Euler((0, math.radians(-12), 0)).to_matrix())
        box(B, (-.14, sg * .08, .86), (.035, .13, .17), 'steel', 'pelvis', rot=Euler((0, math.radians(12), 0)).to_matrix())
        box(B, (0, sg * .20, .87), (.12, .035, .16), 'steel', 'pelvis', rot=Euler((math.radians(sg * 10), 0, 0)).to_matrix())
    # tassets (thigh plates) hang from the pelvis, rigid
    for sg in (1, -1):
        for off in (.0, .085):
            box(B, (.10 + off * .2, sg * (.14 + off * .3), .76), (.05, .08, .13), 'steel', 'pelvis')
    # --- head: helm, visor slit, crest horns
    hz = [1.50, 1.56, 1.63, 1.69, 1.73]
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, z) for z in hz], [.092, .105, .105, .085, .045], [.10, .115, .115, .09, .05], 12, 'steel', wh, vrange=(0, 1), dense=2)
    box(B, (.118, 0, 1.615), (.012, .15, .02), 'glow', 'head')
    box(B, (.119, 0, 1.585), (.010, .02, .06), 'steel', 'head')
    spike(B, (0, 0, 1.73), (0, 0, 1), .08, .02, 'gold', 'head')
    box(B, (.10, 0, 1.675), (.03, .19, .035), 'gold', 'head')                      # brow band
    for sg in (1, -1):
        box(B, (.045, sg * .10, 1.54), (.10, .025, .10), 'steel', 'head', rot=Euler((0, 0, math.radians(-sg * 12))).to_matrix())  # cheek guard
    for sg in (1, -1):
        loft(B, [(0, sg * .09, 1.66), (0, sg * .15, 1.70), (0, sg * .17, 1.76), (.0, sg * .13, 1.82)],
             [.022, .02, .014, 0], [.022, .02, .014, 0], 5, 'steel', 'head', caps=(True, False), ref=(1, 0, 0), vrange=(0, 1))
    # --- arms
    for sg, s in ((1, 'L'), (-1, 'R')):
        j = JOINTS
        sh, el, wr = np.array(j[f'upperarm.{s}'][1]), np.array(j[f'upperarm.{s}'][2]), np.array(j[f'forearm.{s}'][2])
        d = (el - sh) / np.linalg.norm(el - sh)
        along = lambda p, sh=sh, d=d: float(np.dot(np.array(p) - sh, d))
        arm = blend_chain(along, [(f'upperarm.{s}', 0, 0), (f'forearm.{s}', .30, .035)])
        path = [sh - d * .02, sh + d * .12, el, el + d * .14, wr]
        loft(B, path, [.066, .06, .055, .055, .045], [.066, .06, .055, .055, .045], 10, 'steel', arm, ref=(1, 0, 0), vrange=(0, 1), dense=3)
        # couter (elbow cop) and vambrace cuff, rigid on the forearm
        loft(B, [el - d * .03, el + d * .01, el + d * .05], [.04, .075, .04], [.04, .075, .04], 6, 'gold', f'forearm.{s}', caps=(True, True), vrange=(0, 1))
        loft(B, [wr - d * .06, wr + d * .01], [.058, .058], [.058, .058], 8, 'gold', f'forearm.{s}', vrange=(0, 1))
        # fist
        hc = wr + d * .06
        box(B, hc, (.085, .085, .11), 'leather', f'hand.{s}', rot=_rot_to_z(d))
        for fi in range(4):                                                           # fingers (stiff gauntlet plates)
            off = (np.array((.03, 0, 0)) * (fi - 1.5)) if False else np.array((0, 0, 0))
            box(B, hc + d * .055 + np.array((.026 * (fi - 1.5), 0, 0)), (.022, .02, .04), 'gold', f'hand.{s}', rot=_rot_to_z(d))
        # pauldron dome + swept spikes, rigid on the clavicle
        pc = np.array((0, sg * .245, 1.42))
        pz = [-.045, .0, .045, .085, .105]
        pr = [.135, .15, .125, .075, .03]
        loft(B, [pc + (0, 0, z) for z in pz], pr, [x * .9 for x in pr], 12, 'steel', f'clavicle.{s}', vrange=(0, 1), dense=2)
        loft(B, [pc + (0, 0, -.045), pc + (0, 0, -.06)], [.135, .125], [.12, .11], 12, 'gold', f'clavicle.{s}', vrange=(0, 1))
        for ang, up, ln in ((20, .4, .20), (-30, .4, .17), (65, .15, .17), (-75, .15, .15)):
            a = math.radians(ang)
            spike(B, pc + (0, 0, .05) + np.array((math.sin(a) * -.05, sg * .07 * math.cos(a), 0)),
                  (math.sin(a) * -.5 + .0, sg * math.cos(a) * .7, up + .5), ln, .028, 'steel', f'clavicle.{s}', sides=5)
        # --- legs
        hip = np.array(j[f'thigh.{s}'][1])
        kn = np.array(j[f'shin.{s}'][1])
        an = np.array(j[f'foot.{s}'][1])
        leg = blend_chain(lambda p: p[2], [(f'thigh.{s}', 0, 0), (f'shin.{s}', .50, .04)], sgn=-1)
        loft(B, [hip + (0, 0, .03), (hip[0], hip[1], .75), kn, (kn[0], kn[1], .28), an + (0, 0, .03)],
             [.10, .088, .075, .066, .052], [.10, .09, .078, .07, .055], 10, 'steel', leg, vrange=(0, 1), dense=3)
        # poleyn (knee cop) and a greave front plate, rigid on the shin
        loft(B, [kn + (.0, 0, .04), kn + (.0, 0, -.01), kn + (0, 0, -.05)], [.05, .085, .05], [.06, .105, .06], 6, 'gold', f'shin.{s}', vrange=(0, 1))
        box(B, (kn[0] + .062, kn[1], .30), (.025, .09, .26), 'steel', f'shin.{s}', rot=Euler((0, math.radians(-4), 0)).to_matrix())
        # boot and sabaton: heel on foot, toe cap on the toe bone
        wf = lambda p, s=s: {f'foot.{s}': 1.0} if p[0] < .15 else {f'toe.{s}': 1.0}
        loft(B, [(-.06, kn[1], .095), (.04, kn[1], .075), (.15, kn[1], .052), (.25, kn[1], .026)],
             [.058, .066, .062, .042], [.075, .066, .052, .03], 6, 'leather', wf, ref=(0, 0, 1), vrange=(0, 1))
        loft(B, [(.10, kn[1], .075), (.19, kn[1], .052), (.26, kn[1], .028)], [.052, .05, .036], [.04, .035, .022], 5, 'steel', wf, ref=(0, 0, 1), vrange=(0, 1))
    # --- cape: two sheets (front/back), smoothly weighted by height
    cw = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.0, .10), ('spine2', 1.18, .08), ('chest', 1.32, .06)])
    nx, nz_ = 5, 6
    P, UV, W, F = [], [], [], []
    for i in range(nz_ + 1):
        t = i / nz_
        z = 1.44 - t * .82
        half = .22 + t * .12
        for k in range(nx + 1):
            u = k / nx
            y = (u * 2 - 1) * half
            x = -.17 - t * .12 - .02 * math.sin(u * math.pi * 3) * t
            P.append((x, y, z))
            UV.append(region_uv('cape', u, t))
            W.append(weights_for(cw, (x, y, z)))
    for i in range(nz_):
        for k in range(nx):
            a = i * (nx + 1) + k
            F += [(a, a + 1, a + nx + 2), (a, a + nx + 2, a + nx + 1)]
    F = np.array(F)
    # outward normal faces -X; add the reverse for the inside
    B.add(P, UV, F, W, convex=False)                                   # outer sheet, normal -X
    B.add(np.array(P) + (.006, 0, 0), UV, F[:, [0, 2, 1]], W, convex=False)
    # --- kite shield on the left arm, facing outwards (+Y) in the bind pose; rigid on shield_socket
    outline = [(.0, .30), (.17, .20), (.17, -.05), (.09, -.30), (.0, -.46), (-.09, -.30), (-.17, -.05), (-.17, .20)]
    hc = np.array((0, .545, .80))
    P, UV, W, F = [], [], [], []
    frontc = len(outline)
    for (x, z) in outline:
        P.append(hc + (x, .0, z))
        UV.append(region_uv('shield', (x + .17) / .34, (z + .46) / .76))
    P.append(hc + (0, .045, .0))
    UV.append(region_uv('shield', .5, (0 + .46) / .76))
    for i in range(len(outline)):
        F.append((frontc, i, (i + 1) % len(outline)))
    backc = len(P)
    for (x, z) in outline:
        P.append(hc + (x * .96, -.035, z * .96))
        UV.append(region_uv('leather', (x + .17) / .34, (z + .46) / .76))
    P.append(hc + (0, -.035, 0))
    UV.append(region_uv('leather', .5, .5))
    for i in range(len(outline)):
        F.append((backc + len(outline), backc + (i + 1) % len(outline), backc + i))
    n_ = len(outline)
    for i in range(n_):
        j2 = (i + 1) % n_
        F += [(i, backc + i, backc + j2), (i, backc + j2, j2)]
    W = [{'shield_socket': 1.0}] * len(P)
    B.add(P, UV, F, W, convex=True)
    loft(B, [hc + (0, .035, 0), hc + (0, .062, 0), hc + (0, .075, 0)], [.05, .042, .0], [.05, .042, .0], 8, 'gold', 'shield_socket', caps=(True, False), ref=(1, 0, 0), vrange=(0, 1))   # boss
    box(B, hc + (0, .01, .29), (.20, .04, .03), 'gold', 'shield_socket')          # crown rail
    # --- longsword in the right hand, rigid on weapon_socket, blade pointing forward and up
    hr = np.array((0, -.47, .80))
    ax = np.array((math.cos(math.radians(20)), 0, math.sin(math.radians(20))))
    ws = 'weapon_socket'
    loft(B, [hr - ax * .09, hr + ax * .05], [.017, .017], [.017, .017], 6, 'leather', ws, ref=(0, 0, 1), vrange=(0, 1))
    loft(B, [hr - ax * .13, hr - ax * .09], [.0, .03], [.0, .03], 6, 'gold', ws, caps=(True, True), ref=(0, 0, 1), vrange=(0, 1))
    box(B, hr + ax * .06, (.025, .24, .03), 'gold', ws, rot=_rot_to_x(ax))
    loft(B, [hr + ax * .075, hr + ax * .50, hr + ax * .93], [.007, .006, 0], [.033, .028, 0], 4, 'blade', ws, caps=(True, False), ref=(0, 0, 1), vrange=(0, 1))
    return B


def _rot_to_z(d):
    d = np.array(d, float) / np.linalg.norm(d)
    return _frame_matrix(d, (1, 0, 0), 2)


def _rot_to_x(d):
    d = np.array(d, float) / np.linalg.norm(d)
    return _frame_matrix(d, (0, 0, 1), 0)


def _frame_matrix(d, ref, axis):
    a = np.cross(ref, d)
    if np.linalg.norm(a) < 1e-6:
        return np.eye(3)
    a /= np.linalg.norm(a)
    b = np.cross(d, a)
    cols = [None] * 3
    cols[axis] = d
    others = [i for i in range(3) if i != axis]
    cols[others[0]], cols[others[1]] = (a, b) if axis == 0 else (b, a)
    M = np.array(cols).T
    if np.linalg.det(M) < 0:
        M[:, others[0]] *= -1
    return M


# ------------------------------------------------------------------ Blender objects
def make_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.fps = FPS
    A = make_atlas()
    img = L.new_image('T_FighterSpike', A)
    B = build_mesh()
    V = np.array(B.V)
    me = bpy.data.meshes.new('SK_Hero_FighterSpike')
    F = B.F
    me.from_pydata(V.tolist(), [], F)
    me.update()
    uvl = me.uv_layers.new(name='UVMap')
    uv = np.array(B.UV)
    loops_uv = uv[np.array(F).reshape(-1)]
    uvl.data.foreach_set('uv', loops_uv.reshape(-1).astype(np.float32))
    me.shade_flat()
    mat = bpy.data.materials.new('M_FighterSpike')
    nt = mat.node_tree
    bsdf = nt.nodes['Principled BSDF']
    bsdf.inputs['Roughness'].default_value = 1.0
    bsdf.inputs['Metallic'].default_value = 0.0
    tn = nt.nodes.new('ShaderNodeTexImage')
    tn.image = img
    tn.interpolation = 'Closest'
    nt.links.new(tn.outputs['Color'], bsdf.inputs['Base Color'])
    mat.use_backface_culling = True
    me.materials.append(mat)
    ob = bpy.data.objects.new('SK_Hero_FighterSpike', me)
    sc.collection.objects.link(ob)
    # armature
    arm = bpy.data.armatures.new('Skeleton_Humanoid')
    ao = bpy.data.objects.new('Skeleton_Humanoid', arm)
    sc.collection.objects.link(ao)
    bpy.context.view_layer.objects.active = ao
    bpy.ops.object.mode_set(mode='EDIT')
    for name in BONE_ORDER:
        par, h, t = JOINTS[name]
        eb = arm.edit_bones.new(name)
        eb.head, eb.tail = Vector(h), Vector(t)
    for name in BONE_ORDER:
        par = JOINTS[name][0]
        if par:
            arm.edit_bones[name].parent = arm.edit_bones[par]
    bpy.ops.object.mode_set(mode='OBJECT')
    # skin weights (<= 4 influences per vertex)
    groups = {n: ob.vertex_groups.new(name=n) for n in BONE_ORDER}
    acc = {n: ([], []) for n in BONE_ORDER}
    for vi, w in enumerate(B.W):
        items = sorted(w.items(), key=lambda kv: -kv[1])[:4]
        tot = sum(x for _, x in items)
        for b, x in items:
            acc[b][0].append(vi)
            acc[b][1].append(x / tot)
    for n, (idx, ws) in acc.items():
        for i, w in zip(idx, ws):
            groups[n].add([i], w, 'REPLACE')
    ob.parent = ao
    mod = ob.modifiers.new('Armature', 'ARMATURE')
    mod.object = ao
    return ob, ao


# ------------------------------------------------------------------ animation
def deg(x):
    return math.radians(x)


def ease_pose(ao, pose, loc=None):
    pass


def apply_frame(ao, f, rot, loc):
    """rot: bone -> (rx, ry, rz) degrees about model axes (X fwd, Y left, Z up), applied in the parent's
    rest-aligned frame; loc: bone -> metres in model axes. Keys the pose bones at frame f."""
    for b, e in rot.items():
        pb = ao.pose.bones[b]
        Rr = pb.bone.matrix_local.to_3x3()
        D = Euler((deg(e[0]), deg(e[1]), deg(e[2])), 'XYZ').to_matrix()
        q = (Rr.inverted() @ D @ Rr).to_quaternion()
        pb.rotation_mode = 'QUATERNION'
        pb.rotation_quaternion = q
        pb.keyframe_insert('rotation_quaternion', frame=f)
    for b, d in loc.items():
        pb = ao.pose.bones[b]
        Rr = pb.bone.matrix_local.to_3x3()
        pb.location = Rr.inverted() @ Vector(d)
        pb.keyframe_insert('location', frame=f)


def idle(t):
    """t in [0,1). Breathing, a slow weight shift, the sword arm resting, shield arm braced."""
    p = 2 * math.pi * t
    s, c = math.sin(p), math.cos(p)
    rot = {
        'pelvis': (1.2 * s, 0, 1.5 * s),
        'spine1': (-.6 * s, 1.0 * c, -1.0 * s),
        'spine2': (-.4 * s, 1.5 * s, -.8 * s),
        'chest': (-.4 * s, 1.8 * s, 0),
        'neck': (0, -1.0 * s, 1.5 * s),
        'head': (0, 1.0 * s, -1.5 * s),
        'clavicle.L': (0, 0, 0), 'clavicle.R': (0, 0, 0),
        'upperarm.R': (3 + 1.0 * s, -8 - 1.5 * s, 0),
        'forearm.R': (0, -38 - 2 * s, 0),
        'hand.R': (0, 6 * s, 0),
        'upperarm.L': (-6 - 1.0 * s, -10 - 1.0 * s, 0),
        'forearm.L': (0, -50 - 1.5 * s, 0),
        'hand.L': (0, 0, 0),
        'thigh.L': (-3, -3 + 1.0 * s, 0), 'shin.L': (0, 6 - 1.0 * s, 0), 'foot.L': (0, -3 + 0 * s, 0),
        'thigh.R': (3, 2 - 1.0 * s, 0), 'shin.R': (0, 5 + 1.0 * s, 0), 'foot.R': (0, -2, 0),
    }
    loc = {'pelvis': (0, .008 * s, -.004 + .004 * math.cos(2 * p))}
    return rot, loc


def walk(t):
    """t in [0,1): one full stride cycle (two steps) in place."""
    p = 2 * math.pi * t
    s, c = math.sin(p), math.cos(p)
    rot = {}
    for sg, bone in ((1, 'L'), (-1, 'R')):
        ph = p if sg == 1 else p + math.pi
        sw, cw = math.sin(ph), math.cos(ph)
        thigh = -28 * sw                    # negative ry swings the leg forward
        knee = 8 + 48 * max(0.0, cw) ** 1.3
        rot[f'thigh.{bone}'] = (3 * sg * cw, thigh - 4, 0)
        rot[f'shin.{bone}'] = (0, knee, 0)
        rot[f'foot.{bone}'] = (0, -(thigh - 4 + knee) * .85, 0)
        rot[f'toe.{bone}'] = (0, 12 * max(0.0, -cw) if sw < 0 else 0.0, 0)
    rot['pelvis'] = (2 * c, 0, 7 * s)
    rot['spine1'] = (-1.5 * c, 2.5, -3 * s)
    rot['spine2'] = (-1 * c, 1.5, -3 * s)
    rot['chest'] = (-1 * c, 1.0, -1.5 * s)
    rot['neck'] = (0, -1, 3 * s)
    rot['head'] = (0, -1, 2 * s)
    rot['clavicle.L'] = (0, 0, 0)
    rot['clavicle.R'] = (0, 0, 0)
    # arms swing against the legs (left leg forward at sin > 0 -> right arm forward)
    rot['upperarm.R'] = (3, -10 - 12 * s, 0)
    rot['forearm.R'] = (0, -42 - 6 * s, 0)
    rot['hand.R'] = (0, 4 * s, 0)
    rot['upperarm.L'] = (-6, -8 + 10 * s, 0)
    rot['forearm.L'] = (0, -52 + 3 * s, 0)
    rot['hand.L'] = (0, 0, 0)
    loc = {'pelvis': (0, .022 * s, .022 * math.cos(2 * p) - .012)}
    return rot, loc


def author(ao, name, fn, frames):
    act = bpy.data.actions.new(name)
    act.use_fake_user = True
    ao.animation_data_create()
    ao.animation_data.action = act
    bpy.context.view_layer.objects.active = ao
    bpy.ops.object.mode_set(mode='POSE')
    for pb in ao.pose.bones:
        pb.rotation_mode = 'QUATERNION'
    for f in range(frames + 1):
        t = (f / frames) % 1.0 if f < frames else 0.0       # last key repeats the first: seamless loop
        rot, loc = fn(t)
        apply_frame(ao, f, rot, loc)
    bpy.ops.object.mode_set(mode='OBJECT')
    return act


def export(ob, ao):
    os.makedirs(OUT, exist_ok=True)
    for o in bpy.context.scene.objects:
        o.select_set(False)
    ob.select_set(True)
    ao.select_set(True)
    bpy.context.view_layer.objects.active = ao
    path = os.path.join(OUT, 'SK_Hero_FighterSpike.glb')
    # NLA tracks give each action its own named clip
    ao.animation_data.action = None
    for name in ('Idle', 'Walk'):
        tr = ao.animation_data.nla_tracks.new()
        tr.name = name
        st = tr.strips.new(name, 0, bpy.data.actions[name])
        st.name = name
    bpy.ops.export_scene.gltf(
        filepath=path, export_format='GLB', use_selection=True, export_yup=True, export_apply=False,
        export_texcoords=True, export_normals=True, export_materials='EXPORT', export_image_format='AUTO',
        export_skins=True, export_def_bones=False, export_animations=True, export_animation_mode='NLA_TRACKS',
        export_frame_range=False, export_force_sampling=True, export_optimize_animation_size=False,
        export_morph=False, export_cameras=False, export_lights=False, export_vertex_color='NONE')
    return path


def main():
    ob, ao = make_scene()
    tris = len(ob.data.polygons)
    zs = np.array([v.co.z for v in ob.data.vertices])
    xs = np.array([v.co.x for v in ob.data.vertices])
    author(ao, 'Idle', idle, 60)
    author(ao, 'Walk', walk, 30)
    # bind-pose check: the pose evaluates to the rest mesh when no action plays
    path = export(ob, ao)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'SK_Hero_FighterSpike_source.blend'), compress=True)
    metrics = dict(
        triangles=tris, vertices=len(ob.data.vertices), bones=len(BONE_ORDER), bone_names=BONE_ORDER,
        height_m=round(float(zs.max() - zs.min()), 3), z_range=[round(float(zs.min()), 3), round(float(zs.max()), 3)],
        helm_top_without_horns_m=1.74, atlas=ATLAS, materials=1,
        actions={'Idle': {'frames': [0, 60], 'seconds': 2.0}, 'Walk': {'frames': [0, 30], 'seconds': 1.0}},
        glb_bytes=os.path.getsize(path), fps=FPS)
    with open(os.path.join(OUT, 'rig_metrics.json'), 'w') as fh:
        json.dump(metrics, fh, indent=1)
    print('RIG_METRICS', json.dumps({k: v for k, v in metrics.items() if k != 'bone_names'}))


main()
