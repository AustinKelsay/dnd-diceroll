"""Character recipes for the #150 rigged cast, batch 1: the agile body, the rogue and the cleric.

Same skeleton, atlas layout, LOD helpers and conventions as the r001 pilot (Art/rig-pilot/r001/README.md): every character
exports the same 25-bone armature, one material on its own 1024x1024 atlas of 13 tiles, RGBA vertex colour with the glow
mask in alpha (runes and gems only). rigkit.py, clips.py and the r001 helpers are imported, never copied or edited, so
r001 stays reproducible.

Each recipe is build_<name>(rig, M0) -> (Body, atlas_spec); M0 is the posed FK of Idle frame 0, used to author held items
where the hand is in Idle and map them back to the bind pose (rig.to_bind), exactly as in r001.
"""
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
R001 = os.path.abspath(os.path.join(HERE, '..', '..', 'r001', 'scripts'))
sys.path.insert(0, R001)

import rigkit as K                                                              # noqa: E402
from rigkit import (Body, JOINTS, blend_chain, box, deg, detail, dn, frame_matrix, loft, panel, prism, quad, sd)   # noqa: E402
import characters as R1                                                         # noqa: E402  (r001 recipes, for shared helpers)

SIDES = R1.SIDES
rotx, roty, rotz = R1.rotx, R1.roty, R1.rotz
skirt_w, hand_grip, rune_faces = R1.skirt_w, R1.hand_grip, R1.rune_faces


def unit(v):
    v = np.array(v, float)
    return v / np.linalg.norm(v)


def arc_loft(body, path, ra, rb, sides, region, wspec, a0=55, a1=305, ref=(1, 0, 0), vrange=(0, 1), flip=False):
    """An open shell around a vertical path: a loft over the angle range a0..a1 (degrees, 0 = straight ahead, +X) only, so the
    front stays open (a hood or coif around a visible face). flip=True winds the faces inward, for a lining."""
    path = [np.array(p, float) for p in path]
    n = len(path)
    length = [0.0]
    for a, b in zip(path, path[1:]):
        length.append(length[-1] + np.linalg.norm(b - a))
    P, UV, W = [], [], []
    for i in range(n):
        s, f = K.frame(path[min(i + 1, n - 1)] - path[max(i - 1, 0)], np.array(ref, float))
        for k in range(sides + 1):
            a = deg(a0 + (a1 - a0) * k / sides)
            p = path[i] + f * rb[i] * math.cos(a) + s * ra[i] * math.sin(a)
            P.append(p)
            UV.append(K.region_uv(region, k / sides, vrange[0] + (vrange[1] - vrange[0]) * length[i] / (length[-1] or 1)))
            W.append(K.weights_for(wspec, p))
    F = []
    row = sides + 1
    for i in range(n - 1):
        for k in range(sides):
            a, b, c, d = i * row + k, i * row + k + 1, (i + 1) * row + k + 1, (i + 1) * row + k
            F += [(a, b, c), (a, c, d)]
    F = np.array(F)
    body.add(P, UV, F[:, [0, 2, 1]] if flip else F, W, region, False)


# ------------------------------------------------------------------ agile body (rogue; later ranger, monk, bard)
def agile_body(B, rig, M0, jerkin='plate', sleeve='leather', pants='leather', boot='leather', cuff='trim', glove='leather',
               belt='leather', buckle='bone'):
    """Light body on the shared skeleton: slim torso, arms and legs (about 80% of the heavy body's girth), smooth skin over every
    joint, no plates. Torso, sleeves, trousers, boots and gloves only; neck, head, cloak and gear belong to the character."""
    J = JOINTS
    zs = [.94, 1.00, 1.08, 1.17, 1.26, 1.36, 1.44, 1.49]
    ra = [.150, .155, .140, .150, .180, .200, .205, .105]
    rb = [.105, .110, .098, .108, .125, .138, .136, .085]
    wt = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.06, .07), ('spine2', 1.18, .07), ('chest', 1.30, .07)])
    loft(B, [(0, 0, z) for z in zs], ra, rb, sd(14), jerkin, wt, dense=dn(2))
    loft(B, [(0, 0, 1.00), (0, 0, 1.055)], [.163, .163], [.118, .118], sd(14), belt, 'pelvis')               # belt
    box(B, (.122, 0, 1.03), (.03, .06, .06), buckle, 'pelvis')                                              # buckle
    for sg, s in SIDES:
        sh, el, wr = np.array(J[f'upperarm.{s}'][1]), np.array(J[f'upperarm.{s}'][2]), np.array(J[f'forearm.{s}'][2])
        d = (el - sh) / np.linalg.norm(el - sh)
        along = lambda p, sh=sh, d=d: float(np.dot(np.array(p) - sh, d))
        arm = blend_chain(along, [(f'upperarm.{s}', 0, 0), (f'forearm.{s}', .30, .06)])
        loft(B, [sh - d * .02, sh + d * .12, el, el + d * .14, wr], [.050, .047, .042, .042, .036], [.050, .047, .042, .042, .036],
             sd(10), sleeve, arm, vrange=(0, 1), dense=dn(3))
        loft(B, [el + d * .09, wr - d * .02], [.046, .040], [.046, .040], sd(8), jerkin, f'forearm.{s}')       # bracer
        loft(B, [wr - d * .045, wr + d * .005], [.043, .043], [.043, .043], sd(8), cuff, f'forearm.{s}')       # pale cuff
        hc = wr + d * .055
        box(B, hc, (.062, .066, .09), glove, f'hand.{s}', rot=frame_matrix(d, (1, 0, 0), 2))
        if detail(0):
            for fi in range(3):                                                                              # fingers
                box(B, hc + d * .055 + np.array((.019 * (fi - 1), 0, 0)), (.017, .017, .04), glove, f'hand.{s}', rot=frame_matrix(d, (1, 0, 0), 2))
        if detail(1):
            loft(B, [el - d * .06, el - d * .02], [.056, .056], [.056, .056], sd(8), cuff, f'upperarm.{s}', caps=(False, False))   # upper-arm strap
    for sg, s in SIDES:
        hip, kn, an = (np.array(J[f'thigh.{s}'][1]), np.array(J[f'shin.{s}'][1]), np.array(J[f'foot.{s}'][1]))
        leg = blend_chain(lambda p: p[2], [(f'thigh.{s}', 0, 0), (f'shin.{s}', .50, .06)], sgn=-1)
        loft(B, [hip + (0, 0, .03), (hip[0], hip[1], .75), kn, (kn[0], kn[1], .28), an + (0, 0, .03)],
             [.085, .075, .062, .056, .044], [.088, .078, .066, .060, .048], sd(10), pants, leg, vrange=(0, 1), dense=dn(3))
        loft(B, [(kn[0], kn[1], .375), (kn[0], kn[1], .345)], [.068, .068], [.072, .072], sd(8), cuff, f'shin.{s}')   # boot cuff
        if detail(1):
            box(B, (kn[0] + .064, kn[1], .50), (.03, .075, .09), jerkin, f'shin.{s}', rot=roty(-4))                      # knee guard
        wf = lambda p, s=s: {f'foot.{s}': 1.0} if p[0] < .15 else {f'toe.{s}': 1.0}
        loft(B, [(-.05, kn[1], .09), (.04, kn[1], .075), (.15, kn[1], .052), (.25, kn[1], .026)],
             [.050, .056, .052, .036], [.066, .058, .046, .028], sd(6), boot, wf, ref=(0, 0, 1))


def cloth_sheet(B, z_top, z_bot, half0, half1, back0, back1, nx, nz, region, lining, cw, lower, wob=.02, jag=0.0):
    """A hanging cloak or cape behind the figure: nz rows from z_top to z_bot, widening from half0 to half1, leaning back from
    back0 to back1. Weights follow the torso above .98 m (cw) and the legs below (lower). A second face carries `lining`."""
    P, UV, W, F = [], [], [], []
    for i in range(nz + 1):
        t = i / nz
        z = z_top - t * (z_top - z_bot)
        half = half0 + t * (half1 - half0)
        for k in range(nx + 1):
            u = k / nx
            drop = jag * (1 if (k % 2) else 0) * t
            p = (-(back0 + t * (back1 - back0)) - wob * math.sin(u * math.pi * 3) * t, (u * 2 - 1) * half, z - drop)
            P.append(p)
            UV.append(K.region_uv(region, u, 1 - t))
            W.append(lower(p) if z < .98 else cw(p))
    for i in range(nz):
        for k in range(nx):
            a = i * (nx + 1) + k
            F += [(a, a + 1, a + nx + 2), (a, a + nx + 2, a + nx + 1)]
    F = np.array(F)
    B.add(P, UV, F, W, region, False)
    B.add(np.array(P) + (.006, 0, 0), UV, F[:, [0, 2, 1]], W, lining, False)


def dagger(B, g, ax, ws, blade=.30, rune=True):
    """Short blade gripped at g, pointing along ax (authored where the hand is in Idle). Grip, guard, diamond blade, a glowing
    pommel gem and, on the larger LODs, a glowing rune on the flat."""
    ax = unit(ax)
    loft(B, [g - ax * .075, g + ax * .05], [.017, .017], [.017, .017], sd(6), 'leather', ws, ref=(0, 0, 1))
    box(B, g + ax * .058, (.022, .115, .028), 'trim', ws, rot=frame_matrix(ax, (0, 0, 1), 0))
    loft(B, [g + ax * .07, g + ax * (.07 + blade * .55), g + ax * (.07 + blade)], [.0075, .0065, 0], [.030, .026, 0], 4, 'blade', ws,
         caps=(True, False), ref=(0, 0, 1))
    box(B, g - ax * .095, (.028, .028, .028), 'glow', ws)
    if rune and detail(0):
        rune_faces(B, g + ax * (.07 + blade * .42), (0, 1, 0), ax, ws, .035, .011, .0085)


# ------------------------------------------------------------------ ROGUE
def build_rogue(rig, M0):
    B = Body()
    agile_body(B, rig, M0, jerkin='plate', sleeve='leather', pants='leather', boot='leather', cuff='trim', glove='leather')
    cw = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.0, .10), ('spine2', 1.18, .08), ('chest', 1.32, .06)])
    # jerkin: pale lacing down the front, a cross-body strap with a pale buckle
    if detail(1):
        box(B, (.128, 0, 1.28), (.02, .05, .30), 'trim', 'chest')
        box(B, (.132, .02, 1.24), (.022, .05, .46), 'leather', 'chest', rot=rotx(-28))
        box(B, (.141, .02, 1.24), (.016, .045, .05), 'bone', 'chest')
    # sash at the waist with a long tail on the left hip: the copper note against dark ground
    loft(B, [(0, 0, 1.015), (0, 0, 1.10)], [.172, .172], [.126, .126], sd(12), 'emblem', 'pelvis')
    sk = skirt_w(1.06, .8, .6, .06)
    panel(B, (.11, .07, 1.07), (.14, .17, 1.07), (.18, .19, .56), (.13, .09, .56), 2, 3 if detail(0) else 2, 'emblem', sk)
    if detail(1):
        for sg in (1, -1):
            box(B, (.02, sg * .185, .92), (.07, .05, .085), 'leather', 'pelvis')                           # hip pouches
            box(B, (.02, sg * .216, .93), (.04, .012, .035), 'trim', 'pelvis')
    # pale scarf at the throat, and the hood lined in pale cloth around the face opening
    wn = blend_chain(lambda p: p[2], [('chest', 0, 0), ('neck', 1.48, .03)])
    loft(B, [(0, 0, 1.42), (0, 0, 1.51)], [.115, .095], [.10, .088], sd(10), 'accent', wn)
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, 1.50), (0, 0, 1.58), (0, 0, 1.68)], [.075, .085, .09], [.08, .092, .095], sd(10), 'skin', wh)            # head under hood
    for sg in (1, -1):
        box(B, (.092, sg * .04, 1.625), (.012, .045, .02), 'dark', 'head')                                  # eyes in the shade
        box(B, (.094, sg * .04, 1.645), (.010, .05, .008), 'bone', 'head')                                  # pale brow line
    box(B, (.095, 0, 1.55), (.02, .13, .075), 'accent', 'head')                                              # pale mask over the lower face
    hood = [(-.015, 0, 1.50), (-.015, 0, 1.58), (-.02, 0, 1.66), (-.045, 0, 1.725), (-.085, 0, 1.775)]
    hra, hrb = [.118, .126, .126, .102, .04], [.124, .134, .132, .11, .05]
    arc_loft(B, hood, hra, hrb, sd(12), 'cloth', wh, a0=62, a1=298, vrange=(1, 0))
    arc_loft(B, hood, [x - .012 for x in hra], [x - .012 for x in hrb], sd(12), 'accent', wh, a0=62, a1=298, flip=True)         # pale lining
    loft(B, [(-.01, 0, 1.485), (-.01, 0, 1.50)], [.14, .136], [.13, .126], sd(12), 'trim', wh, caps=(False, False))      # pale hood hem
    loft(B, [(-.085, 0, 1.772), (-.10, 0, 1.79)], [.04, .0], [.05, .0], sd(8), 'cloth', wh, caps=(True, False))               # hood peak
    for sg in (1, -1):                                                                                           # pale trim along the hood opening
        box(B, (.056, sg * .106, 1.63), (.02, .012, .22), 'trim', wh, rot=rotz(sg * 24))
    # shoulder mantle and the long cloak (sage, pale hem, pale lining)
    loft(B, [(0, 0, 1.50), (0, 0, 1.43), (0, 0, 1.36)], [.13, .25, .28], [.10, .17, .20], sd(14), 'cloth', 'chest', caps=(True, False), vrange=(1, 0), dense=dn(2))
    loft(B, [(0, 0, 1.372), (0, 0, 1.352)], [.277, .281], [.197, .201], sd(14), 'trim', 'chest', caps=(False, False))
    nx, nz_ = (6, 7) if detail(0) else (4, 4) if detail(1) else (3, 3)
    cloth_sheet(B, 1.40, .42, .20, .40, .17, .30, nx, nz_, 'cloth', 'accent', cw, skirt_w(.98, .85, .5, .09), wob=.025, jag=.03 if detail(0) else 0.0)
    # leather spaulders and two sheathed spare knives crossed on the back
    for sg, sd_ in SIDES:
        pc = np.array((0, sg * .245, 1.42))
        loft(B, [pc + (0, 0, z) for z in (-.04, .0, .035, .06)], [.10, .115, .09, .03], [.09, .104, .082, .028], sd(10), 'plate', f'clavicle.{sd_}', dense=dn(2))
        loft(B, [pc + (0, 0, -.04), pc + (0, 0, -.05)], [.10, .097], [.09, .087], sd(10), 'trim', f'clavicle.{sd_}')
    if detail(0):
        for sg in (1, -1):
            c0 = np.array((-.15, sg * .03, 1.18))
            axs = unit((0, sg * .52, .85))
            loft(B, [c0 - axs * .17, c0 + axs * .17], [.024, .016], [.03, .02], 5, 'leather', 'chest', ref=(1, 0, 0))                # sheath
            loft(B, [c0 + axs * .17, c0 + axs * .26], [.014, .014], [.014, .014], 5, 'bone', 'chest', ref=(1, 0, 0))               # hilt
    # twin daggers: right on weapon_socket, left on shield_socket, ready at the hip in Idle
    g = hand_grip(rig, M0, 'R')
    B.push(rig.to_bind(M0, 'weapon_socket'))
    dagger(B, g, (math.cos(deg(42)), 0, math.sin(deg(42))), 'weapon_socket')
    B.pop()
    g = hand_grip(rig, M0, 'L')
    B.push(rig.to_bind(M0, 'shield_socket'))
    dagger(B, g, (math.cos(deg(30)), -.18, math.sin(deg(30))), 'shield_socket')
    B.pop()
    return B, ROGUE_ATLAS


ROGUE_ATLAS = {
    'plate': ('bands', dict(base=(.54, .40, .25), line=(.96, .90, .72), spacing=64)),       # tan studded leather, pale stitching (one notch darker, review round 1)
    'trim': ('scales', dict(a=(.97, .94, .82), b=(.86, .80, .62))),                           # pale cream trim
    'cloth': ('folds', dict(base=(.32, .49, .34), period=30, hem=(.95, .92, .80), hem2=(.80, .72, .46))),
    'leather': ('weave', dict(base=(.50, .34, .20))),
    'blade': ('blade', dict(base=(.90, .94, 1.0))),
    'glow': ('flat', dict(base=(.55, 1.0, .78))),
    'rune': ('rune', dict(bg=(.05, .20, .18), fg=(.55, 1.0, .80))),
    'skin': ('flat', dict(base=(.82, .66, .54))),
    'bone': ('flat', dict(base=(.96, .93, .84))),
    'accent': ('flat', dict(base=(.97, .95, .88))),                                           # hood lining, scarf, mask
    'dark': ('flat', dict(base=(.20, .18, .22))),
    'emblem': ('folds', dict(base=(.95, .54, .22), period=22, hem=(.98, .90, .70), hem2=(.80, .40, .14))),     # the copper sash
    'emblem2': ('flat', dict(base=(.62, .43, .26))),
}


# ------------------------------------------------------------------ CLERIC (robed body)
CZ = [1.00, .90, .76, .58, .40, .27]
CRA = [.185, .21, .24, .275, .30, .31]
CRB = [.14, .165, .19, .215, .23, .235]


def cleric_rb(z):
    return float(np.interp(-z, [-x for x in CZ], CRB))


def cleric_ra(z):
    return float(np.interp(-z, [-x for x in CZ], CRA))


def pauldron_round(B, plate, trim):
    """Round shoulder caps with a gold edge: rigid on the clavicles, no spikes."""
    for sg, s in SIDES:
        pc = np.array((0, sg * .245, 1.42))
        pz = [-.04, .0, .04, .075, .09]
        pr = [.115, .13, .11, .065, .025]
        loft(B, [pc + (0, 0, z) for z in pz], pr, [x * .92 for x in pr], sd(12), plate, f'clavicle.{s}', dense=dn(2))
        loft(B, [pc + (0, 0, -.04), pc + (0, 0, -.055)], [.115, .108], [.106, .10], sd(12), trim, f'clavicle.{s}')


def round_shield(B, rig, M0, face, back, rim, boss):
    """Round shield on the left forearm (rigid on shield_socket), authored where the arm is in Idle."""
    el, wr = rig.head(M0, 'forearm.L'), rig.tail(M0, 'forearm.L')
    c = (el + wr) / 2 + np.array((.10, .06, .02))
    n = unit((.82, .57, 0.0))
    u = unit((-.14, 0, 1.0))
    r = np.cross(u, n)
    u = np.cross(n, r)
    B.push(rig.to_bind(M0, 'shield_socket'))
    m = 12 if detail(0) else 8
    rad = .24
    outline = [(rad * math.cos(2 * math.pi * k / m), rad * math.sin(2 * math.pi * k / m)) for k in range(m)]
    P, UV, F = [], [], []
    for (x, z) in outline:
        P.append(c + r * x + u * z)
        UV.append(K.region_uv(face, (x + rad) / (2 * rad), (z + rad) / (2 * rad)))
    P.append(c + n * .035)
    UV.append(K.region_uv(face, .5, .5))
    for i in range(m):
        F.append((m, i, (i + 1) % m))
    backc = len(P)
    for (x, z) in outline:
        P.append(c + r * x * .94 + u * z * .94 - n * .03)
        UV.append(K.region_uv(back, (x + rad) / (2 * rad), (z + rad) / (2 * rad)))
    P.append(c - n * .03)
    UV.append(K.region_uv(back, .5, .5))
    for i in range(m):
        F.append((backc + m, backc + (i + 1) % m, backc + i))
    for i in range(m):
        j = (i + 1) % m
        F += [(i, backc + i, backc + j), (i, backc + j, j)]
    B.add(P, UV, F, [{'shield_socket': 1.0}] * len(P), face, True)
    loft(B, [c + n * .034, c + n * .052, c + n * .066], [.046, .040, .0], [.046, .040, .0], sd(8), rim, 'shield_socket', caps=(True, False))
    box(B, c + n * .068, (.022, .022, .022), boss, 'shield_socket', rot=np.column_stack((r, n, u)))            # boss gem
    if detail(1):
        loft(B, [c + n * .036 + u * .0, c + n * .04], [.235, .235], [.235, .235], m, rim, 'shield_socket', caps=(False, False), ref=(0, 0, 1))
    B.pop()


def build_cleric(rig, M0):
    B = Body()
    J = JOINTS
    sk = skirt_w(.98, .85, .85, .09)
    # cassock: ivory, to mid-shin so the boots show, a gold hem
    loft(B, [(0, 0, z) for z in CZ], CRA, CRB, sd(24), 'cloth', sk, caps=(False, False), vrange=(1, 0), dense=dn(3, 2))
    # armoured bodice: a bright steel cuirass over the cloth, a gold gorget, belt and cord
    zs = [.96, 1.04, 1.14, 1.26, 1.38, 1.46, 1.50]
    ra = [.185, .17, .16, .175, .215, .24, .12]
    rb = [.14, .125, .12, .13, .16, .165, .09]
    wt = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.08, .08), ('spine2', 1.20, .09), ('chest', 1.32, .09)])
    loft(B, [(0, 0, z) for z in zs], ra, rb, sd(16), 'cloth', wt, vrange=(1, 0), dense=dn(3, 2))
    loft(B, [(0, 0, 1.17), (0, 0, 1.27), (0, 0, 1.38), (0, 0, 1.45)], [.172, .20, .236, .238], [.132, .148, .172, .170], sd(14), 'plate',
         wt, caps=(False, False), dense=dn(2))
    loft(B, [(0, 0, 1.00), (0, 0, 1.09)], [.2, .2], [.15, .15], sd(16), 'trim', 'pelvis')                    # gold belt
    box(B, (.158, 0, 1.05), (.03, .07, .07), 'accent', 'pelvis')
    if detail(1):
        loft(B, [(0, 0, 1.47), (0, 0, 1.53)], [.14, .085], [.115, .075], sd(12), 'trim', blend_chain(lambda p: p[2], [('chest', 0, 0), ('neck', 1.50, .02)]))   # gold gorget
    # tabard in teal and gold with the dawn emblem, front and back, hanging over the cassock
    zt, zb = 1.34, (.30 if detail(0) else .34)
    for sgn, xf in ((1, .152), (-1, -.142)):
        xt = xf + sgn * .03
        xb = sgn * (cleric_rb(zb) + .016)
        pts = ((xt, -.095, zt), (xt, .095, zt), (xb, .095, zb), (xb, -.095, zb)) if sgn > 0 else ((xt, .095, zt), (xt, -.095, zt), (xb, -.095, zb), (xb, .095, zb))
        panel(B, *pts, 2, 3 if detail(0) else 2, 'emblem', sk, vrange=(0, 1))
    # glowing glyphs around the hem
    nr = 10 if detail(0) else 7 if detail(1) else 5
    for k in range(nr):
        a = 2 * math.pi * (k + .5) / nr
        z = .33
        rr_a, rr_b = cleric_ra(z) + .012, cleric_rb(z) + .012
        c = np.array((rr_b * math.cos(a), rr_a * math.sin(a), z))
        tan = unit((-rr_b * math.sin(a), rr_a * math.cos(a), 0))
        quad(B, c, tan * .045, np.array((0, 0, .045)), 'rune', sk)
    quad(B, (.206, 0, 1.10), np.array((0, .075, 0)), np.array((0, 0, .075)), 'emblem2', sk)                  # the dawn emblem on the tabard
    quad(B, (-.196, 0, 1.10), np.array((0, -.075, 0)), np.array((0, 0, .075)), 'emblem2', sk)
    pauldron_round(B, 'plate', 'trim')
    # back mantle: a pale cape lined in teal, short of the hem
    cw = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.0, .10), ('spine2', 1.18, .08), ('chest', 1.32, .06)])
    nx, nz_ = (5, 5) if detail(0) else (3, 3)
    cloth_sheet(B, 1.40, .62, .22, .34, .20, .27, nx, nz_, 'cloth', 'accent', cw, skirt_w(.98, .85, .5, .09), wob=.02)
    # sleeves: fitted with pale-gold cuffs
    for sg, s in SIDES:
        sh, el, wr = np.array(J[f'upperarm.{s}'][1]), np.array(J[f'upperarm.{s}'][2]), np.array(J[f'forearm.{s}'][2])
        d = (el - sh) / np.linalg.norm(el - sh)
        along = lambda p, sh=sh, d=d: float(np.dot(np.array(p) - sh, d))
        arm = blend_chain(along, [(f'upperarm.{s}', 0, 0), (f'forearm.{s}', .30, .09)])
        loft(B, [sh - d * .02, sh + d * .15, el, el + d * .15, wr + d * .02], [.062, .066, .066, .074, .084], [.062, .066, .066, .074, .084],
             sd(10), 'cloth', arm, vrange=(1, 0), dense=dn(4, 2))
        loft(B, [wr - d * .02, wr + d * .035], [.086, .088], [.086, .088], sd(10), 'trim', f'forearm.{s}', caps=(False, False))
        hc = wr + d * .07
        box(B, hc, (.062, .062, .10), 'skin', f'hand.{s}', rot=frame_matrix(d, (1, 0, 0), 2))
    # head: warm face, steel coif, gold circlet with a glowing gem
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, 1.46), (0, 0, 1.52)], [.07, .06], [.07, .06], sd(8), 'skin', wh)
    loft(B, [(0, 0, 1.52), (0, 0, 1.60), (0, 0, 1.67), (0, 0, 1.73)], [.084, .098, .096, .062], [.092, .106, .10, .062], sd(14), 'skin', wh, dense=dn(2))
    for sg in (1, -1):
        box(B, (.104, sg * .045, 1.625), (.012, .045, .028), 'dark', 'head')
        if detail(1):
            box(B, (.103, sg * .045, 1.652), (.012, .05, .008), 'leather', 'head')
    box(B, (.112, 0, 1.585), (.024, .02, .05), 'skin', 'head')
    loft(B, [(0, 0, 1.665), (0, 0, 1.70), (0, 0, 1.745)], [.103, .094, .055], [.108, .098, .058], sd(12), 'leather', 'head', caps=(False, True))   # hair
    coif = [(-.005, 0, 1.52), (-.005, 0, 1.60), (-.012, 0, 1.69), (-.02, 0, 1.75)]
    cra, crb = [.112, .124, .122, .08], [.118, .13, .126, .085]
    arc_loft(B, coif, cra, crb, sd(14), 'plate', wh, a0=64, a1=296, vrange=(1, 0))                                  # mail coif: nape, ears and crown
    arc_loft(B, coif, [x - .012 for x in cra], [x - .012 for x in crb], sd(14), 'accent', wh, a0=64, a1=296, flip=True)
    loft(B, [(0, 0, 1.675), (0, 0, 1.705)], [.118, .118], [.122, .122], sd(14), 'trim', 'head', caps=(False, False))      # gold circlet
    box(B, (.122, 0, 1.69), (.014, .03, .036), 'glow', 'head')                                               # circlet gem
    if detail(1):
        loft(B, [(-.02, 0, 1.752), (-.02, 0, 1.80)], [.012, .0], [.012, .0], 4, 'trim', 'head', caps=(True, False))   # coif finial
    # holy medallion on the chest
    if detail(1):
        box(B, (.158, 0, 1.31), (.016, .09, .09), 'trim', 'chest', rot=rotx(45))
        box(B, (.168, 0, 1.31), (.012, .034, .034), 'glow', 'chest', rot=rotx(45))
    # boots peek out at the hem
    for sg, s in SIDES:
        kn = np.array(J[f'shin.{s}'][1])
        wf = lambda p, s=s: {f'foot.{s}': 1.0} if p[0] < .15 else {f'toe.{s}': 1.0}
        loft(B, [(.02, kn[1], .09), (.12, kn[1], .06), (.25, kn[1], .03)], [.06, .06, .04], [.07, .06, .03], sd(6), 'leather', wf, ref=(0, 0, 1))
        loft(B, [(kn[0], kn[1], .22), (kn[0], kn[1], .09)], [.058, .052], [.062, .056], sd(8), 'leather', f'shin.{s}')
        loft(B, [(kn[0], kn[1], .235), (kn[0], kn[1], .21)], [.064, .064], [.068, .068], sd(8), 'trim', f'shin.{s}')
    round_shield(B, rig, M0, 'emblem2', 'leather', 'trim', 'glow')
    # flanged mace in the right hand, head up and forward in Idle, glowing glyphs on the flanges
    g = hand_grip(rig, M0, 'R')
    ax = unit((math.cos(deg(58)), 0, math.sin(deg(58))))
    B.push(rig.to_bind(M0, 'weapon_socket'))
    ws = 'weapon_socket'
    loft(B, [g - ax * .12, g + ax * .30], [.02, .02], [.02, .02], sd(6), 'leather', ws, ref=(0, 1, 0))
    loft(B, [g - ax * .17, g - ax * .12], [.0, .03], [.0, .03], sd(6), 'trim', ws, caps=(True, False), ref=(0, 1, 0))      # pommel
    hc = g + ax * .40
    loft(B, [g + ax * .28, g + ax * .33], [.024, .034], [.024, .034], sd(6), 'trim', ws, ref=(0, 1, 0))                  # collar
    loft(B, [hc - ax * .06, hc, hc + ax * .06], [.04, .06, .04], [.04, .06, .04], sd(8), 'trim', ws, ref=(0, 1, 0))      # ball head
    for k in range(4 if detail(0) else 3):                                                                          # flanges
        ang = 2 * math.pi * k / (4 if detail(0) else 3)
        dirn = np.cross(ax, (0, 0, 1))
        dirn = unit(dirn) * math.cos(ang) + unit(np.cross(ax, unit(dirn))) * math.sin(ang)
        box(B, hc + dirn * .055, (.13, .02, .05), 'plate', ws, rot=frame_matrix(ax, dirn, 0))
    box(B, hc + ax * .072, (.026, .026, .026), 'glow', ws)
    if detail(1):
        rune_faces(B, hc - ax * .03, (0, 1, 0), ax, ws, .035, .02, .066)
    B.pop()
    return B, CLERIC_ATLAS


CLERIC_ATLAS = {
    'plate': ('bands', dict(base=(.80, .82, .90), line=(1.0, .96, .80), spacing=32)),         # bright steel with gold lines
    'trim': ('scales', dict(a=(1.0, .84, .36), b=(.90, .68, .24))),
    'cloth': ('folds', dict(base=(.95, .92, .82), period=34, hem=(.99, .84, .40), hem2=(.88, .66, .22))),   # ivory, gold hem
    'leather': ('weave', dict(base=(.56, .39, .25))),
    'blade': ('blade', dict(base=(.84, .88, .95))),
    'glow': ('flat', dict(base=(1.0, .90, .50))),
    'rune': ('rune', dict(bg=(.30, .22, .06), fg=(1.0, .90, .50))),
    'skin': ('flat', dict(base=(.86, .68, .54))),
    'bone': ('flat', dict(base=(.96, .94, .86))),
    'accent': ('flat', dict(base=(.98, .96, .90))),
    'dark': ('flat', dict(base=(.22, .18, .20))),
    'emblem': ('folds', dict(base=(.20, .50, .66), period=30, hem=(1.0, .84, .36), hem2=(.90, .66, .22))),           # tabard: teal, gold hem
    'emblem2': ('emblem', dict(base=(.18, .42, .60), border=(1.0, .82, .34), mark=(.99, .93, .62), symbol='sigil')),   # shield face
}
# Original holy symbol (drawn after the atlas is built): a half sun rising over a horizon line with short rays.
DAWN = {'cleric': ('emblem2',)}


def paint_dawn(A, region, spec, seed):
    kind, kw = spec[region]
    rng = np.random.default_rng(seed + 7)
    nz = K.smooth_noise(rng, K.TILE, 16)
    n = K.TILE
    yy, xx = np.mgrid[0:n, 0:n]
    u = (xx + .5) / n * 2 - 1
    v = (yy + .5) / n * 2 - 1
    t = np.ones((n, n, 4), np.float32)
    col = lambda c: np.array(c, np.float32)
    t[..., :3] = col(kw['base']) * (.94 + .12 * nz[..., None])
    border = (np.abs(u) > .88) | (np.abs(v) > .93)
    t[border, :3] = col(kw['border'])
    hv = .30                                                                  # horizon
    r = np.hypot(u, v - hv)
    ang = np.arctan2(-(v - hv), u)                                            # 0 right, pi/2 up
    half = (r < .36) & (v < hv)
    ray = (np.abs(((ang * 7 / math.pi) % 1.0) - .5) < .24 * (1.10 - r)) & (r > .46) & (r < .80) & (v < hv - .04)
    bar = (np.abs(v - hv - .06) < .045) & (np.abs(u) < .62)
    low = (np.abs(v - hv - .20) < .03) & (np.abs(u) < .38)
    t[half | ray | bar | low, :3] = col(kw['mark'])
    i = K.REGIONS.index(region)
    c, rr = i % 4, i // 4
    A[rr * K.TILE:(rr + 1) * K.TILE, c * K.TILE:(c + 1) * K.TILE] = t


def make_atlas(key, spec, seed):
    A = K.make_atlas(spec, seed)
    for region in DAWN.get(key, ()):
        paint_dawn(A, region, spec, seed)
    return A


CHARACTERS = {   # key: (builder, name, atlas seed)
    'rogue': (build_rogue, 'SK_Hero_Rogue', 1474),
    'cleric': (build_cleric, 'SK_Hero_Cleric', 1475),
}
ASSET_IDS = {'rogue': 'sk-hero-rogue', 'cleric': 'sk-hero-cleric'}
