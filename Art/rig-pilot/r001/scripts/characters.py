"""Character recipes for the #147 pilot: heavy and robed bodies on the shared skeleton, plus fighter, wizard and raider.

Each recipe is build_<name>(rig, M0) -> (Body, atlas_spec). M0 is the posed FK of the idle frame, used to place held items
(sword, shield, staff, axe) where the hand actually is in Idle; they are mapped back to the bind pose through
rig.to_bind so one skin works in every clip. rigkit.LOD is read through sd()/dn()/detail() for the LOD meshes.

Mesh conventions: facing +X, Y left, Z up, metres, pivot between the feet. Downward loft paths use vrange=(1, 0) so
the tile's bottom (hem) sits at the lower ring.
"""
import math

import numpy as np

import rigkit as K
from rigkit import (Body, JOINTS, blend_chain, box, deg, detail, dn, frame_matrix, loft, panel, prism, quad, sd, spike)

SIDES = ((1, 'L'), (-1, 'R'))


def rotx(a):
    return K.euler_matrix((a, 0, 0))


def roty(a):
    return K.euler_matrix((0, a, 0))


def rotz(a):
    return K.euler_matrix((0, 0, a))


def skirt_w(depth_from=.95, span=.75, strength=.8, spread=.12):
    """Cloth that hangs from the hips: pelvis at the waist, increasingly pulled by the thigh on its side towards the hem."""
    def f(p):
        t = float(np.clip((depth_from - p[2]) / span, 0, 1)) * strength
        sl = float(np.clip((p[1] + spread) / (2 * spread), 0, 1))
        w = {'pelvis': 1 - t}
        if t * sl > 1e-3:
            w['thigh.L'] = t * sl
        if t * (1 - sl) > 1e-3:
            w['thigh.R'] = t * (1 - sl)
        return w
    return f


# ------------------------------------------------------------------ heavy body (fighter, raider)
def heavy_body(B, rig, M0, plate='plate', trim='trim', leather='leather', fist='leather'):
    J = JOINTS
    # torso, waist, hips: blended rings at the waist and chest
    zs = [.94, 1.00, 1.08, 1.17, 1.26, 1.36, 1.44, 1.49]
    ra = [.175, .18, .165, .17, .21, .245, .25, .13]
    rb = [.12, .125, .115, .125, .15, .17, .165, .10]
    wt = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.06, .04), ('spine2', 1.18, .05), ('chest', 1.30, .05)])
    loft(B, [(0, 0, z) for z in zs], ra, rb, sd(12), plate, wt, dense=dn(2))
    loft(B, [(0, 0, 1.02), (0, 0, 1.07)], [.19, .19], [.135, .135], sd(12), trim, 'pelvis')               # belt
    box(B, (.14, 0, 1.045), (.03, .07, .07), trim, 'pelvis')                                             # buckle
    if detail(1):
        box(B, (.17, 0, 1.36), (.04, .05, .20), trim, 'chest')                                           # breastplate keel
    if detail(1):
        loft(B, [(0, 0, 1.46), (0, 0, 1.52)], [.15, .09], [.12, .08], sd(12), plate,
             blend_chain(lambda p: p[2], [('chest', 0, 0), ('neck', 1.50, .02)]))                       # gorget
    if detail(1):
        for sg in (1, -1):                                                                              # faulds, rigid on the pelvis
            box(B, (.15, sg * .08, .86), (.035, .13, .17), plate, 'pelvis', rot=roty(-12))
            box(B, (-.14, sg * .08, .86), (.035, .13, .17), plate, 'pelvis', rot=roty(12))
            box(B, (0, sg * .20, .87), (.12, .035, .16), plate, 'pelvis', rot=rotx(sg * 10))
    for sg in (1, -1) if detail(1) else ():
        for off in ((.0, .085) if detail(0) else (.0,)):
            box(B, (.10 + off * .2, sg * (.14 + off * .3), .76), (.05, .08, .13), plate, 'pelvis')    # tassets
    # arms
    for sg, s in SIDES:
        sh, el, wr = np.array(J[f'upperarm.{s}'][1]), np.array(J[f'upperarm.{s}'][2]), np.array(J[f'forearm.{s}'][2])
        d = (el - sh) / np.linalg.norm(el - sh)
        along = lambda p, sh=sh, d=d: float(np.dot(np.array(p) - sh, d))
        arm = blend_chain(along, [(f'upperarm.{s}', 0, 0), (f'forearm.{s}', .30, .035)])
        loft(B, [sh - d * .02, sh + d * .12, el, el + d * .14, wr], [.066, .06, .055, .055, .045], [.066, .06, .055, .055, .045],
             sd(10), plate, arm, vrange=(0, 1), dense=dn(3))
        loft(B, [el - d * .03, el + d * .01, el + d * .05], [.04, .075, .04], [.04, .075, .04], sd(6), trim, f'forearm.{s}')
        loft(B, [wr - d * .06, wr + d * .01], [.058, .058], [.058, .058], sd(8), trim, f'forearm.{s}')
        hc = wr + d * .06
        box(B, hc, (.085, .085, .11), fist, f'hand.{s}', rot=frame_matrix(d, (1, 0, 0), 2))
        if detail(0):
            for fi in range(4):
                box(B, hc + d * .055 + np.array((.026 * (fi - 1.5), 0, 0)), (.022, .02, .04), trim, f'hand.{s}',
                    rot=frame_matrix(d, (1, 0, 0), 2))
    # legs
    for sg, s in SIDES:
        hip, kn, an = (np.array(J[f'thigh.{s}'][1]), np.array(J[f'shin.{s}'][1]), np.array(J[f'foot.{s}'][1]))
        leg = blend_chain(lambda p: p[2], [(f'thigh.{s}', 0, 0), (f'shin.{s}', .50, .04)], sgn=-1)
        loft(B, [hip + (0, 0, .03), (hip[0], hip[1], .75), kn, (kn[0], kn[1], .28), an + (0, 0, .03)],
             [.10, .088, .075, .066, .052], [.10, .09, .078, .07, .055], sd(10), plate, leg, vrange=(0, 1), dense=dn(3))
        loft(B, [kn + (0, 0, .04), kn + (0, 0, -.01), kn + (0, 0, -.05)], [.05, .085, .05], [.06, .105, .06], sd(6), trim, f'shin.{s}')
        if detail(1):
            box(B, (kn[0] + .062, kn[1], .30), (.025, .09, .26), plate, f'shin.{s}', rot=roty(-4))
        wf = lambda p, s=s: {f'foot.{s}': 1.0} if p[0] < .15 else {f'toe.{s}': 1.0}
        loft(B, [(-.06, kn[1], .095), (.04, kn[1], .075), (.15, kn[1], .052), (.25, kn[1], .026)],
             [.058, .066, .062, .042], [.075, .066, .052, .03], sd(6), leather, wf, ref=(0, 0, 1))
        if detail(1):
            loft(B, [(.10, kn[1], .075), (.19, kn[1], .052), (.26, kn[1], .028)], [.052, .05, .036], [.04, .035, .022], sd(5),
                 plate, wf, ref=(0, 0, 1))


def pauldrons(B, plate, trim, spk, spk_len=(.20, .17, .17, .15)):
    for sg, s in SIDES:
        pc = np.array((0, sg * .245, 1.42))
        pz = [-.045, .0, .045, .085, .105]
        pr = [.135, .15, .125, .075, .03]
        loft(B, [pc + (0, 0, z) for z in pz], pr, [x * .9 for x in pr], sd(12), plate, f'clavicle.{s}', dense=dn(2))
        loft(B, [pc + (0, 0, -.045), pc + (0, 0, -.06)], [.135, .125], [.12, .11], sd(12), trim, f'clavicle.{s}')
        for ang, up, ln in zip((20, -30, 65, -75), (.4, .4, .15, .15), spk_len):
            if not detail(1) and ang in (65, -75):
                continue
            a = math.radians(ang)
            spike(B, pc + (0, 0, .05) + np.array((math.sin(a) * -.05, sg * .07 * math.cos(a), 0)),
                  (math.sin(a) * -.5, sg * math.cos(a) * .7, up + .5), ln, .028, spk, f'clavicle.{s}', sides=5)


def rune_faces(B, c, n, u, wspec, half_u, half_v, off):
    """Glowing glyph quads on both broad faces of a thin plate whose face normal is +-n; u is an in-plane axis."""
    n = np.array(n, float) / np.linalg.norm(n)
    u = np.array(u, float) / np.linalg.norm(u)
    v = np.cross(n, u)                                    # u x v = n
    c = np.array(c, float)
    quad(B, c + n * off, u * half_u, v * half_v, 'rune', wspec)
    quad(B, c - n * off, u * half_u, -v * half_v, 'rune', wspec)


def hand_grip(rig, M0, side):
    h, t = rig.head(M0, f'hand.{side}'), rig.tail(M0, f'hand.{side}')
    return h + (t - h) * .6


# ------------------------------------------------------------------ FIGHTER
def build_fighter(rig, M0):
    B = Body()
    heavy_body(B, rig, M0)
    pauldrons(B, 'plate', 'trim', 'plate')
    # helm: visor slit, brow band, cheek guards, pale plume, gold horns, amber brow gem
    hz = [1.50, 1.56, 1.63, 1.69, 1.73]
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, z) for z in hz], [.092, .105, .105, .085, .045], [.10, .115, .115, .09, .05], sd(12), 'plate', wh, dense=dn(2))
    box(B, (.118, 0, 1.615), (.012, .15, .02), 'dark', 'head')
    box(B, (.119, 0, 1.585), (.010, .02, .06), 'plate', 'head')
    box(B, (.10, 0, 1.675), (.03, .19, .035), 'trim', 'head')
    box(B, (.124, 0, 1.69), (.012, .03, .03), 'glow', 'head')                                           # brow gem
    if detail(1):
        for sg in (1, -1):
            box(B, (.045, sg * .10, 1.54), (.10, .025, .10), 'plate', 'head', rot=rotz(-sg * 12))
    for sg in (1, -1):
        loft(B, [(0, sg * .09, 1.66), (0, sg * .15, 1.70), (0, sg * .17, 1.76), (0, sg * .13, 1.82)],
             [.024, .022, .016, 0], [.024, .022, .016, 0], sd(5), 'trim', 'head', caps=(True, False))
    loft(B, [(.07, 0, 1.72), (.02, 0, 1.79), (-.05, 0, 1.80), (-.12, 0, 1.75), (-.16, 0, 1.66), (-.16, 0, 1.58)],
         [.03, .034, .034, .032, .028, .015], [.03, .05, .05, .045, .04, .02], sd(5), 'accent', 'head', ref=(0, 1, 0))
    # cape: smooth weights by height (crimson with a pale hem)
    cw = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.0, .10), ('spine2', 1.18, .08), ('chest', 1.32, .06)])
    nx, nz_ = (5, 6) if detail(0) else (3, 3)
    P, UV, W, F = [], [], [], []
    for i in range(nz_ + 1):
        t = i / nz_
        z = 1.44 - t * .82
        half = .22 + t * .12
        for k in range(nx + 1):
            u = k / nx
            p = (-.17 - t * .12 - .02 * math.sin(u * math.pi * 3) * t, (u * 2 - 1) * half, z)
            P.append(p)
            UV.append(K.region_uv('cloth', u, 1 - t))
            W.append(cw(p))
    for i in range(nz_):
        for k in range(nx):
            a = i * (nx + 1) + k
            F += [(a, a + 1, a + nx + 2), (a, a + nx + 2, a + nx + 1)]
    F = np.array(F)
    B.add(P, UV, F, W, 'cloth', False)
    B.add(np.array(P) + (.006, 0, 0), UV, F[:, [0, 2, 1]], W, 'cloth', False)
    # tabard: cream surcoat panels hanging over the hips, front with a crimson cross
    sk = skirt_w(1.06, .8, .6, .06)
    zt, zb = 1.06, (.50 if detail(0) else .52)
    hw = .115
    nzz = 3 if detail(0) else 2
    panel(B, (.172, -hw, zt), (.172, hw, zt), (.192, hw, zb), (.192, -hw, zb), 2, nzz, 'emblem', sk)
    panel(B, (-.152, hw, zt), (-.152, -hw, zt), (-.172, -hw, zb), (-.172, hw, zb), 2, nzz, 'emblem', sk)
    # kite shield on the left forearm (rigid on shield_socket), authored where the arm is in Idle
    el, wr = rig.head(M0, 'forearm.L'), rig.tail(M0, 'forearm.L')
    c = (el + wr) / 2 + np.array((.10, .06, .02))
    n = np.array((.82, .57, 0.0))
    n /= np.linalg.norm(n)
    u = np.array((-.14, 0, 1.0))
    u /= np.linalg.norm(u)
    r = np.cross(u, n)
    u = np.cross(n, r)
    B.push(rig.to_bind(M0, 'shield_socket'))
    outline = [(0, .27), (.15, .18), (.15, -.04), (.08, -.27), (0, -.38), (-.08, -.27), (-.15, -.04), (-.15, .18)]
    frontc = len(outline)
    P, UV, F = [], [], []
    for (x, z) in outline:
        P.append(c + r * x + u * z)
        UV.append(K.region_uv('emblem2', (x + .15) / .30, (z + .38) / .65))
    P.append(c + n * .04)
    UV.append(K.region_uv('emblem2', .5, .5))
    for i in range(len(outline)):
        F.append((frontc, i, (i + 1) % len(outline)))
    backc = len(P)
    for (x, z) in outline:
        P.append(c + r * x * .95 + u * z * .95 - n * .03)
        UV.append(K.region_uv('leather', (x + .15) / .30, (z + .38) / .65))
    P.append(c - n * .03)
    UV.append(K.region_uv('leather', .5, .5))
    for i in range(len(outline)):
        F.append((backc + len(outline), backc + (i + 1) % len(outline), backc + i))
    m = len(outline)
    for i in range(m):
        j = (i + 1) % m
        F += [(i, backc + i, backc + j), (i, backc + j, j)]
    B.add(P, UV, F, [{'shield_socket': 1.0}] * len(P), 'emblem2', True)
    loft(B, [c + n * .03, c + n * .06, c + n * .075], [.05, .042, .0], [.05, .042, .0], sd(8), 'trim', 'shield_socket', caps=(True, False))
    if detail(1):
        box(B, c + u * .26 + n * .01, (.2, .05, .03), 'trim', 'shield_socket', rot=np.column_stack((r, n, u)))
    B.pop()
    # longsword: grip through the right fist, blade up and forward, rune fuller glowing amber
    g = hand_grip(rig, M0, 'R')
    ax = np.array((math.cos(deg(60)), 0, math.sin(deg(60))))
    B.push(rig.to_bind(M0, 'weapon_socket'))
    ws = 'weapon_socket'
    loft(B, [g - ax * .10, g + ax * .06], [.018, .018], [.018, .018], sd(6), 'leather', ws, ref=(0, 0, 1))
    loft(B, [g - ax * .15, g - ax * .10], [.0, .032], [.0, .032], sd(6), 'trim', ws, ref=(0, 0, 1))
    box(B, g + ax * .075, (.026, .26, .03), 'trim', ws, rot=frame_matrix(ax, (0, 0, 1), 0))
    loft(B, [g + ax * .09, g + ax * .52, g + ax * .98], [.0075, .0065, 0], [.036, .03, 0], 4, 'blade', ws, caps=(True, False), ref=(0, 0, 1))
    for cfrac in ((.30, .55) if detail(0) else (.42,)):                                                 # rune fuller, both faces
        rune_faces(B, g + ax * (.09 + .88 * cfrac), (0, 1, 0), ax, ws, .045, .016, .0085)
    box(B, g - ax * .155, (.03, .03, .03), 'glow', ws)                                                   # pommel gem
    B.pop()
    return B, FIGHTER_ATLAS


FIGHTER_ATLAS = {
    'plate': ('bands', dict(base=(.40, .48, .68), line=(.86, .91, 1.0), spacing=64)),
    'trim': ('scales', dict(a=(1.0, .80, .32), b=(.88, .62, .22))),
    'cloth': ('folds', dict(base=(.84, .13, .16), period=32, hem=(.97, .93, .82), hem2=(.88, .66, .22))),
    'leather': ('weave', dict(base=(.50, .33, .20))),
    'blade': ('blade', dict(base=(.84, .90, .98))),
    'glow': ('flat', dict(base=(1.0, .72, .28))),
    'rune': ('rune', dict(bg=(.30, .16, .06), fg=(1.0, .78, .30))),
    'skin': ('flat', dict(base=(.78, .62, .50))),
    'bone': ('flat', dict(base=(.92, .88, .76))),
    'accent': ('flat', dict(base=(.96, .97, 1.0))),
    'dark': ('flat', dict(base=(.10, .10, .14))),
    'emblem': ('emblem', dict(base=(.94, .90, .78), border=(.95, .72, .26), mark=(.80, .12, .15), symbol='cross')),
    'emblem2': ('emblem', dict(base=(.18, .26, .52), border=(1.0, .80, .32), mark=(.88, .16, .18), symbol='star')),
}


# ------------------------------------------------------------------ WIZARD (robed body, smooth cloth weights)
ROBE_Z = [1.00, .92, .80, .60, .40, .20, .04]
ROBE_RA = [.185, .21, .245, .29, .325, .355, .38]
ROBE_RB = [.14, .17, .20, .23, .25, .26, .27]


def robe_rb(z):
    return float(np.interp(-z, [-x for x in ROBE_Z], ROBE_RB))


def robe_ra(z):
    return float(np.interp(-z, [-x for x in ROBE_Z], ROBE_RA))


def build_wizard(rig, M0):
    B = Body()
    J = JOINTS
    sk = skirt_w(.98, .85, .85, .09)
    # robe skirt: smooth hips-to-hem, pale rune hem
    loft(B, [(0, 0, z) for z in ROBE_Z], ROBE_RA, ROBE_RB, sd(24), 'cloth', sk, caps=(False, False), vrange=(1, 0), dense=dn(3, 2))
    # bodice
    zs = [.96, 1.04, 1.14, 1.26, 1.38, 1.46, 1.50]
    ra = [.185, .17, .16, .175, .215, .24, .12]
    rb = [.14, .125, .12, .13, .16, .165, .09]
    wt = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.08, .08), ('spine2', 1.20, .09), ('chest', 1.32, .09)])
    loft(B, [(0, 0, z) for z in zs], ra, rb, sd(16), 'cloth', wt, vrange=(1, 0), dense=dn(3, 2))
    # sash, buckle and the hanging emblem panels
    loft(B, [(0, 0, 1.00), (0, 0, 1.09)], [.2, .2], [.15, .15], sd(16), 'trim', 'pelvis')
    box(B, (.158, 0, 1.05), (.03, .07, .07), 'accent', 'pelvis')
    for sgn, xf in ((1, .14), (-1, -.13)):
        zt, zb = 1.04, .30 if detail(0) else .34
        xt = xf + sgn * .03
        xb = sgn * (robe_rb(zb) + .012)
        pts = ((xt, -.07, zt), (xt, .07, zt), (xb, .07, zb), (xb, -.07, zb)) if sgn > 0 else ((xt, .07, zt), (xt, -.07, zt), (xb, -.07, zb), (xb, .07, zb))
        panel(B, *pts, 2, 3 if detail(0) else 2, 'emblem', sk, vrange=(0, 1))
    # glowing rune glyphs around the hem
    nr = 12 if detail(0) else 8 if detail(1) else 6
    for k in range(nr):
        a = 2 * math.pi * (k + .5) / nr
        z = .13
        rr_a, rr_b = robe_ra(z) + .012, robe_rb(z) + .012
        c = np.array((rr_b * math.cos(a), rr_a * math.sin(a), z))
        tan = np.array((-rr_b * math.sin(a), rr_a * math.cos(a), 0))
        tan /= np.linalg.norm(tan)
        quad(B, c, tan * .055, np.array((0, 0, .055)), 'rune', sk)
    # mantle: wide shoulder cape with a pale rim, and a back drape lined in pale lavender
    loft(B, [(0, 0, 1.52), (0, 0, 1.43), (0, 0, 1.35)], [.13, .27, .31], [.10, .19, .22], sd(14), 'cloth', 'chest', caps=(True, False), vrange=(1, 0), dense=dn(2))
    loft(B, [(0, 0, 1.372), (0, 0, 1.345)], [.307, .312], [.217, .222], sd(14), 'accent', 'chest', caps=(False, False))
    cw = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.0, .10), ('spine2', 1.18, .08), ('chest', 1.32, .06)])
    nx, nz_ = (6, 7) if detail(0) else (4, 4)
    P, UV, W, F = [], [], [], []
    for i in range(nz_ + 1):
        t = i / nz_
        z = 1.40 - t * 1.22
        half = .24 + t * .16
        for k in range(nx + 1):
            u = k / nx
            p = (-.21 - t * .17 - .02 * math.sin(u * math.pi * 3) * t, (u * 2 - 1) * half, z)
            P.append(p)
            UV.append(K.region_uv('cloth', u, 1 - t))
            W.append(skirt_w(.98, .85, .5, .09)(p) if z < .98 else cw(p))
    for i in range(nz_):
        for k in range(nx):
            a = i * (nx + 1) + k
            F += [(a, a + 1, a + nx + 2), (a, a + nx + 2, a + nx + 1)]
    F = np.array(F)
    B.add(P, UV, F, W, 'cloth', False)
    B.add(np.array(P) + (.006, 0, 0), UV, F[:, [0, 2, 1]], W, 'accent', False)
    # sleeves: bell sleeves, smooth along the arm
    for sg, s in SIDES:
        sh, el, wr = np.array(J[f'upperarm.{s}'][1]), np.array(J[f'upperarm.{s}'][2]), np.array(J[f'forearm.{s}'][2])
        d = (el - sh) / np.linalg.norm(el - sh)
        along = lambda p, sh=sh, d=d: float(np.dot(np.array(p) - sh, d))
        arm = blend_chain(along, [(f'upperarm.{s}', 0, 0), (f'forearm.{s}', .30, .10)])
        loft(B, [sh - d * .02, sh + d * .15, el, el + d * .15, wr + d * .02], [.065, .07, .075, .09, .11], [.065, .07, .075, .09, .11],
             sd(10), 'cloth', arm, vrange=(1, 0), dense=dn(4, 2))
        loft(B, [wr - d * .01, wr + d * .04], [.113, .115], [.113, .115], sd(10), 'accent', f'forearm.{s}', caps=(False, False))
        hc = wr + d * .075
        box(B, hc, (.06, .06, .10), 'skin', f'hand.{s}', rot=frame_matrix(d, (1, 0, 0), 2))
        if detail(0):
            for fi in range(3):
                box(B, hc + d * .06 + np.array((.02 * (fi - 1), 0, 0)), (.016, .016, .04), 'skin', f'hand.{s}', rot=frame_matrix(d, (1, 0, 0), 2))
    # head: pale mask, dark eyes, beard, hair, hat
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, 1.46), (0, 0, 1.52)], [.07, .06], [.07, .06], sd(8), 'skin', wh)
    loft(B, [(0, 0, 1.52), (0, 0, 1.60), (0, 0, 1.67), (0, 0, 1.73)], [.082, .098, .095, .06], [.09, .105, .10, .06], sd(14), 'skin', wh, dense=dn(2))
    for sg in (1, -1):
        box(B, (.106, sg * .045, 1.625), (.012, .05, .03), 'dark', 'head')
        if detail(1):
            box(B, (.104, sg * .045, 1.655), (.012, .05, .008), 'accent', 'head')
    box(B, (.112, 0, 1.585), (.024, .02, .05), 'skin', 'head')
    loft(B, [(.075, 0, 1.56), (.10, 0, 1.46), (.08, 0, 1.36), (.05, 0, 1.22)], [.055, .06, .045, 0], [.03, .045, .04, 0], sd(8), 'accent', 'head', caps=(True, False), ref=(0, 1, 0), dense=dn(2))
    loft(B, [(-.02, 0, 1.70), (-.10, 0, 1.62), (-.14, 0, 1.46)], [.09, .10, .07], [.05, .06, .04], sd(8), 'accent', 'head', caps=(False, True), ref=(0, 1, 0))     # long hair
    loft(B, [(0, 0, 1.655), (0, 0, 1.675)], [.20, .20], [.20, .20], sd(14), 'cloth', 'head', caps=(True, True))
    loft(B, [(0, 0, 1.675), (-.01, 0, 1.75), (-.06, 0, 1.83)], [.12, .08, .0], [.12, .08, .0], sd(12), 'cloth', 'head', caps=(False, False), vrange=(1, 0), dense=dn(2))
    loft(B, [(0, 0, 1.68), (0, 0, 1.705)], [.123, .123], [.123, .123], sd(12), 'accent', 'head', caps=(False, False))
    loft(B, [(0, 0, 1.652), (0, 0, 1.658)], [.205, .205], [.205, .205], sd(14), 'accent', 'head', caps=(False, False))
    box(B, (.122, 0, 1.693), (.012, .035, .035), 'glow', 'head')                                       # hat gem
    # satchel-side kit: spellbook and amulet gem
    if detail(1):
        for sg, nm in ((1, 'book'),):
            box(B, (.02, .22, .90), (.14, .05, .19), 'leather', 'pelvis', rot=K.euler_matrix((0, 0, 0)))
            box(B, (.02, .248, .90), (.12, .006, .17), 'emblem', 'pelvis')
            quad(B, (.02, .253, .90), np.array((.04, 0, 0)), np.array((0, 0, -.04)), 'rune', 'pelvis')
        box(B, (.158, 0, 1.30), (.02, .04, .05), 'glow', 'chest')                                      # amulet
        box(B, (.152, 0, 1.345), (.012, .012, .08), 'trim', 'chest')
    # boots peek out at the hem
    for sg, s in SIDES:
        kn = np.array(J[f'shin.{s}'][1])
        wf = lambda p, s=s: {f'foot.{s}': 1.0} if p[0] < .15 else {f'toe.{s}': 1.0}
        loft(B, [(.02, kn[1], .09), (.12, kn[1], .06), (.25, kn[1], .03)], [.06, .06, .04], [.07, .06, .03], sd(6), 'leather', wf, ref=(0, 0, 1))
    # staff in the right hand, upright in Idle, glowing crystal head
    g = hand_grip(rig, M0, 'R')
    B.push(rig.to_bind(M0, 'weapon_socket'))
    ws = 'weapon_socket'
    ax = np.array((.05, 0, 1.0))
    ax /= np.linalg.norm(ax)
    foot_pt = g - ax * (g[2] / ax[2] - .02)
    top = g + ax * ((1.80 - g[2]) / ax[2])
    loft(B, [foot_pt, g, top], [.021, .024, .02], [.021, .024, .02], sd(8), 'leather', ws, ref=(0, 1, 0), dense=dn(2))
    for off in ((0.0, .06, .40, .80) if detail(1) else (0.0,)):
        loft(B, [top - ax * (.03 + off), top - ax * (.0 + off)], [.034, .034], [.034, .034], sd(8), 'trim', ws, caps=(False, False), ref=(0, 1, 0))
    for sg in (1, -1):                                                                                 # prongs cradling the crystal
        loft(B, [top, top + (0, sg * .05, .08), top + (0, sg * .06, .20)], [.018, .014, 0], [.018, .014, 0], sd(5), 'trim', ws, caps=(True, False), ref=(1, 0, 0))
    cp = top + (0, 0, .10)
    loft(B, [cp - (0, 0, .09), cp, cp + (0, 0, .11)], [0, .052, 0], [0, .052, 0], 4 if K.LOD else 6, 'glow', ws, caps=(False, False), ref=(1, 0, 0))
    if detail(1):
        rune_faces(B, g + ax * .40, (0, 1, 0), ax, ws, .05, .02, .0235)
    B.pop()
    return B, WIZARD_ATLAS


WIZARD_ATLAS = {
    'plate': ('bands', dict(base=(.50, .46, .80), line=(.90, .88, 1.0), spacing=64)),
    'trim': ('scales', dict(a=(1.0, .82, .36), b=(.88, .64, .24))),
    'cloth': ('folds', dict(base=(.40, .34, .86), period=36, hem=(.93, .90, 1.0), hem2=(.98, .84, .38))),
    'leather': ('weave', dict(base=(.46, .30, .20))),
    'blade': ('blade', dict(base=(.84, .90, .98))),
    'glow': ('flat', dict(base=(.62, .95, 1.0))),
    'rune': ('rune', dict(bg=(.10, .10, .34), fg=(.55, .95, 1.0))),
    'skin': ('flat', dict(base=(.86, .84, .88))),
    'bone': ('flat', dict(base=(.92, .88, .76))),
    'accent': ('flat', dict(base=(.95, .95, 1.0))),
    'dark': ('flat', dict(base=(.12, .12, .30))),
    'emblem': ('emblem', dict(base=(.84, .80, 1.0), border=(1.0, .82, .36), mark=(.34, .28, .78), symbol='sigil')),
    'emblem2': ('emblem', dict(base=(.18, .26, .52), border=(1.0, .80, .32), mark=(.88, .16, .18), symbol='star')),
}


# ------------------------------------------------------------------ RAIDER (heavy body, hide and bone)
def build_raider(rig, M0):
    B = Body()
    heavy_body(B, rig, M0, plate='plate', trim='trim', leather='leather', fist='leather')
    pauldrons(B, 'plate', 'trim', 'bone', spk_len=(.30, .26, .24, .20))
    # skull-bucket helm: eye slits painted ember (no glow), bone brow ridge, nasal bar, tusks
    hz = [1.50, 1.56, 1.63, 1.69, 1.73]
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, z) for z in hz], [.095, .11, .11, .09, .05], [.105, .12, .12, .095, .055], sd(12), 'dark', wh, dense=dn(2))
    box(B, (.122, 0, 1.62), (.012, .15, .028), 'trim', 'head')
    box(B, (.115, 0, 1.69), (.03, .20, .035), 'bone', 'head')
    box(B, (.126, 0, 1.58), (.012, .02, .08), 'bone', 'head')
    for sg in (1, -1):
        spike(B, (.12, sg * .06, 1.52), (.6, sg * .25, -.8), .11, .02, 'bone', 'head', sides=4)
    # antlers: two beams with tines
    for sg in (1, -1):
        beam = [(0, sg * .09, 1.70), (-.02, sg * .16, 1.78), (-.03, sg * .23, 1.88), (-.03, sg * .31, 1.97)]
        K.loft(B, beam, [.024, .02, .016, .0], [.024, .02, .016, .0], sd(5), 'bone', 'head', caps=(True, False), ref=(1, 0, 0))
        for base, dirn, ln in (((-.02, sg * .16, 1.78), (.5, sg * .6, .6), .24), ((-.03, sg * .23, 1.88), (-.5, sg * .5, .7), .22),
                               ((-.03, sg * .23, 1.88), (.5, sg * .7, .5), .18)):
            if detail(1) or ln > .2:
                spike(B, base, dirn, ln, .016, 'bone', 'head', sides=4)
    # axe in the right hand: shaft, bearded blade, back spike, glowing rune
    g = hand_grip(rig, M0, 'R')
    ax = np.array((math.cos(deg(62)), 0, math.sin(deg(62))))
    fw = np.array((math.sin(deg(62)), 0, -math.cos(deg(62))))            # forward-down, perpendicular to the shaft
    B.push(rig.to_bind(M0, 'weapon_socket'))
    ws = 'weapon_socket'
    loft(B, [g - ax * .20, g + ax * .10, g + ax * .75], [.022, .022, .026], [.022, .022, .026], sd(6), 'leather', ws, ref=(0, 1, 0))
    top = g + ax * .66
    outline = [(.02, .13), (.12, .14), (.22, .20), (.29, .08), (.30, -.08), (.25, -.22), (.12, -.13), (.02, -.11)]
    prism(B, outline, top, fw, ax, np.cross(fw, ax), .014, 'blade', 'blade', ws, taper=.55)
    box(B, top - fw * .02, (.07, .05, .12), 'dark', ws, rot=np.column_stack((fw, (0, 1, 0), ax)))
    spike(B, top - fw * .05, -fw, .20, .028, 'bone', ws, sides=4)
    spike(B, top + ax * .05, ax, .16, .022, 'bone', ws, sides=4)
    rune_faces(B, top + fw * .15 + ax * .03, (0, 1, 0), fw, ws, .05, .06, .0165)
    B.pop()
    # tattered cloth: back cape and loincloth panels
    cw = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.0, .10), ('spine2', 1.18, .08), ('chest', 1.32, .06)])
    nx, nz_ = (4, 4) if detail(0) else (3, 3)
    P, UV, W, F = [], [], [], []
    for i in range(nz_ + 1):
        t = i / nz_
        z = 1.42 - t * .66
        half = .20 + t * .08
        for k in range(nx + 1):
            u = k / nx
            jag = .06 * (1 if (k % 2) else 0) * t
            p = (-.17 - t * .10, (u * 2 - 1) * half, z - jag)
            P.append(p)
            UV.append(K.region_uv('cloth', u, 1 - t))
            W.append(cw(p))
    for i in range(nz_):
        for k in range(nx):
            a = i * (nx + 1) + k
            F += [(a, a + 1, a + nx + 2), (a, a + nx + 2, a + nx + 1)]
    F = np.array(F)
    B.add(P, UV, F, W, 'cloth', False)
    B.add(np.array(P) + (.006, 0, 0), UV, F[:, [0, 2, 1]], W, 'cloth', False)
    sk = skirt_w(1.04, .8, .6, .06)
    hw = .12
    panel(B, (.17, -hw, 1.04), (.17, hw, 1.04), (.20, hw, .52), (.20, -hw, .52), 2, 3 if detail(0) else 2, 'cloth', sk)
    panel(B, (-.15, hw, 1.04), (-.15, -hw, 1.04), (-.17, -hw, .56), (-.17, hw, .56), 2, 2, 'cloth', sk)
    # war-brand rune on the chest plate
    quad(B, (.188, 0, 1.36), np.array((0, .045, 0)), np.array((0, 0, .06)), 'rune', 'chest')
    return B, RAIDER_ATLAS


RAIDER_ATLAS = {
    'plate': ('bands', dict(base=(.62, .44, .30), line=(.96, .88, .68), spacing=64)),
    'trim': ('scales', dict(a=(1.0, .56, .20), b=(.80, .36, .12))),
    'cloth': ('folds', dict(base=(.78, .16, .12), period=28, hem=(.96, .82, .56), hem2=(.60, .12, .08))),
    'leather': ('weave', dict(base=(.52, .34, .20))),
    'blade': ('blade', dict(base=(.80, .84, .90))),
    'glow': ('flat', dict(base=(1.0, .62, .22))),
    'rune': ('rune', dict(bg=(.32, .10, .05), fg=(1.0, .62, .22))),
    'skin': ('flat', dict(base=(.74, .54, .42))),
    'bone': ('flat', dict(base=(.96, .92, .80))),
    'accent': ('flat', dict(base=(.96, .93, .84))),
    'dark': ('flat', dict(base=(.24, .18, .15))),
    'emblem': ('emblem', dict(base=(.62, .44, .30), border=(1.0, .56, .20), mark=(.96, .88, .68), symbol='sigil')),
    'emblem2': ('emblem', dict(base=(.62, .44, .30), border=(1.0, .56, .20), mark=(.96, .88, .68), symbol='star')),
}

CHARACTERS = {   # key: (builder, name, atlas seed)
    'fighter': (build_fighter, 'SK_Hero_Fighter', 1471),
    'wizard': (build_wizard, 'SK_Hero_Wizard', 1472),
    'raider': (build_raider, 'SK_Foe_Raider', 1473),
}
