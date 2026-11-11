"""Character recipes for the #150 rigged cast, batch 2: barbarian, monk, paladin and ranger.

Same skeleton, atlas layout, LOD helpers and conventions as batches 0 and 1 (Art/rig-pilot/r001/README.md,
Art/rig-pilot/cast/README.md): the 25-bone armature, one material on its own 1024x1024 atlas of 13 tiles, RGBA vertex colour
with the glow mask in alpha on runes and gems only, at most 4 influences, the shared clips. This module registers its classes in
cast_characters.CHARACTERS / ASSET_IDS in memory (cast_characters.py itself is not edited, so the batch 1 asset records that hash
it stay valid), carries its own atlas painters and imports brace.py (the tenth clip), so build2.py, review2.py and make_records2.py
only need to import it.

Each recipe is build_<name>(rig, M0) -> (Body, atlas_spec); M0 is the posed FK of Idle frame 0. The bow is the one exception: it is
authored where the left hand is in the Ranged hold (see build_ranger) so that the bow is upright when it is drawn.
"""
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import cast_characters as C                                                     # noqa: E402  (puts r001/scripts on sys.path)
import brace                                                                    # noqa: E402,F401  (registers the Brace clip)
import clips as CL                                                              # noqa: E402
import rigkit as K                                                              # noqa: E402
from rigkit import Body, JOINTS, blend_chain, box, deg, detail, dn, frame_matrix, loft, panel, prism, quad, sd, spike   # noqa: E402

R1 = C.R1
SIDES = C.SIDES
rotx, roty, rotz = C.rotx, C.roty, C.rotz
skirt_w, hand_grip, rune_faces = C.skirt_w, C.hand_grip, C.rune_faces
unit, arc_loft, cloth_sheet, dagger, agile_body, pauldron_round = C.unit, C.arc_loft, C.cloth_sheet, C.dagger, C.agile_body, C.pauldron_round


def leg_weights(s):
    return blend_chain(lambda p: p[2], [(f'thigh.{s}', 0, 0), (f'shin.{s}', .50, .04)], sgn=-1)


def foot_weights(s):
    return lambda p: {f'foot.{s}': 1.0} if p[0] < .15 else {f'toe.{s}': 1.0}


def arm_frame(s):
    J = JOINTS
    sh, el, wr = np.array(J[f'upperarm.{s}'][1]), np.array(J[f'upperarm.{s}'][2]), np.array(J[f'forearm.{s}'][2])
    d = (el - sh) / np.linalg.norm(el - sh)
    return sh, el, wr, d


def tufts(B, ring_z, ra, rb, n, length, r, region, wspec, a0=0.0):
    """Short soft tufts hanging from a ring (fur hem): downward cones, rigid on one bone."""
    for k in range(n):
        a = a0 + 2 * math.pi * (k + .5) / n
        spike(B, (rb * math.cos(a), ra * math.sin(a), ring_z), (0.12 * math.cos(a), 0.12 * math.sin(a), -1.0), length, r, region, wspec, sides=5)


def tower_shield(B, rig, M0, outline, w, h, face, back, rim, boss, spine=None, ribs=True):
    """A tall shield on the left forearm (rigid on shield_socket), authored where the arm is in Idle, like the fighter's kite shield.
    outline: (x, z) points around the centre, x in +-w/2, z in +-h/2 (convex)."""
    el, wr = rig.head(M0, 'forearm.L'), rig.tail(M0, 'forearm.L')
    c = (el + wr) / 2 + np.array((.10, .06, .02))
    n = unit((.82, .57, 0.0))
    u = unit((-.14, 0, 1.0))
    r = np.cross(u, n)
    u = np.cross(n, r)
    B.push(rig.to_bind(M0, 'shield_socket'))
    m = len(outline)
    P, UV, F = [], [], []
    for (x, z) in outline:
        P.append(c + r * x + u * z)
        UV.append(K.region_uv(face, (x + w / 2) / w, (z + h / 2) / h))
    P.append(c + n * .04)
    UV.append(K.region_uv(face, .5, .5))
    for i in range(m):
        F.append((m, i, (i + 1) % m))
    backc = len(P)
    for (x, z) in outline:
        P.append(c + r * x * .95 + u * z * .95 - n * .03)
        UV.append(K.region_uv(back, (x + w / 2) / w, (z + h / 2) / h))
    P.append(c - n * .03)
    UV.append(K.region_uv(back, .5, .5))
    for i in range(m):
        F.append((backc + m, backc + (i + 1) % m, backc + i))
    for i in range(m):
        j = (i + 1) % m
        F += [(i, backc + i, backc + j), (i, backc + j, j)]
    B.add(P, UV, F, [{'shield_socket': 1.0}] * len(P), face, True)
    loft(B, [c + n * .04, c + n * .075, c + n * .09], [.052, .044, .0], [.052, .044, .0], sd(8), rim, 'shield_socket', caps=(True, False))
    box(B, c + n * .094, (.024, .024, .024), boss, 'shield_socket', rot=np.column_stack((r, n, u)))
    if detail(1):
        for i in range(m):                                       # raised rim along every edge
            a, b = np.array(outline[i]), np.array(outline[(i + 1) % m])
            pa, pb = c + r * a[0] + u * a[1], c + r * b[0] + u * b[1]
            t = pb - pa
            ln = np.linalg.norm(t)
            t /= ln
            box(B, (pa + pb) / 2 + n * .043, (ln + .012, .02, .03), rim, 'shield_socket', rot=np.column_stack((t, n, np.cross(t, n))))
    B.pop()


# ------------------------------------------------------------------ BARBARIAN (heavy body, bulked up, bare arms and warpaint)
def barbarian_body(B, rig, M0, g=1.09):
    """The heavy body's skeleton fit, bulkier (girth g): a bare painted torso and arms, a fur kilt, wrapped leather legs."""
    J = JOINTS
    zs = [.94, 1.00, 1.08, 1.17, 1.26, 1.36, 1.44, 1.49]
    ra = [x * g for x in (.175, .18, .165, .17, .21, .245, .25, .13)]
    rb = [x * g for x in (.12, .125, .115, .125, .15, .17, .165, .10)]
    wt = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.06, .04), ('spine2', 1.18, .05), ('chest', 1.30, .05)])
    loft(B, [(0, 0, z) for z in zs], ra, rb, sd(12), 'emblem', wt, dense=dn(2))                              # painted skin
    loft(B, [(0, 0, 1.01), (0, 0, 1.075)], [.20 * g, .20 * g], [.14 * g, .14 * g], sd(12), 'leather', 'pelvis')   # wide belt
    box(B, (.15 * g, 0, 1.042), (.03, .09, .08), 'trim', 'pelvis')                                         # bronze buckle
    if detail(1):                                                                                            # crossed harness straps with a bronze ring
        box(B, (.172, .0, 1.27), (.024, .07, .50), 'leather', 'chest', rot=rotx(-30))
        box(B, (.176, .0, 1.27), (.024, .07, .50), 'leather', 'chest', rot=rotx(30))
        box(B, (.19, 0, 1.27), (.016, .07, .07), 'trim', 'chest', rot=rotx(45))
    # fur kilt, to the knee, raggedly cut, over the thighs (smooth skirt weights)
    sk = skirt_w(1.02, .75, .70, .09)
    kz = [1.04, .96, .84, .72, .62]
    kra = [x * g for x in (.19, .205, .225, .24, .245)]
    krb = [x * g for x in (.14, .155, .175, .19, .195)]
    loft(B, [(0, 0, z) for z in kz], kra, krb, sd(18), 'cloth', sk, caps=(False, False), vrange=(1, 0), dense=dn(2))
    if detail(1):
        tufts(B, .64, kra[-1] + .004, krb[-1] + .004, 14 if detail(0) else 9, .075, .032, 'cloth', sk)
    # bare arms with warpaint, a bronze armlet above the elbow and a leather bracer on each forearm
    for sg, s in SIDES:
        sh, el, wr, d = arm_frame(s)
        along = lambda p, sh=sh, d=d: float(np.dot(np.array(p) - sh, d))
        arm = blend_chain(along, [(f'upperarm.{s}', 0, 0), (f'forearm.{s}', .30, .035)])
        loft(B, [sh - d * .02, sh + d * .12, el, el + d * .14, wr], [x * g for x in (.068, .066, .058, .058, .047)],
             [x * g for x in (.068, .066, .058, .058, .047)], sd(10), 'emblem', arm, vrange=(0, 1), dense=dn(3))
        loft(B, [sh + d * .17, sh + d * .22], [.071 * g, .071 * g], [.071 * g, .071 * g], sd(8), 'trim', f'upperarm.{s}', caps=(False, False))
        loft(B, [el + d * .07, wr - d * .015], [.056 * g, .048 * g], [.056 * g, .048 * g], sd(8), 'plate', f'forearm.{s}')
        loft(B, [wr - d * .05, wr + d * .005], [.054 * g, .054 * g], [.054 * g, .054 * g], sd(8), 'leather', f'forearm.{s}')
        hc = wr + d * .06
        box(B, hc, (.088, .088, .11), 'skin', f'hand.{s}', rot=frame_matrix(d, (1, 0, 0), 2))
        if detail(0):
            for fi in range(4):
                box(B, hc + d * .056 + np.array((.026 * (fi - 1.5), 0, 0)), (.022, .02, .04), 'skin', f'hand.{s}', rot=frame_matrix(d, (1, 0, 0), 2))
    # legs: wrapped leather, fur-cuffed boots
    for sg, s in SIDES:
        hip, kn, an = np.array(J[f'thigh.{s}'][1]), np.array(J[f'shin.{s}'][1]), np.array(J[f'foot.{s}'][1])
        loft(B, [hip + (0, 0, .03), (hip[0], hip[1], .75), kn, (kn[0], kn[1], .28), an + (0, 0, .03)],
             [x * g for x in (.100, .088, .075, .066, .052)], [x * g for x in (.100, .090, .078, .070, .055)], sd(10), 'leather', leg_weights(s),
             vrange=(0, 1), dense=dn(3))
        loft(B, [(kn[0], kn[1], .35), (kn[0], kn[1], .24)], [.098, .092], [.100, .094], sd(10), 'cloth', f'shin.{s}', vrange=(1, 0))       # fur boot cuff
        loft(B, [(kn[0], kn[1], .52), (kn[0], kn[1], .475)], [.092, .092], [.095, .095], sd(10), 'trim', f'shin.{s}', caps=(False, False))   # bronze knee band
        loft(B, [(-.06, kn[1], .095), (.04, kn[1], .075), (.15, kn[1], .052), (.25, kn[1], .026)],
             [.058 * g, .066 * g, .062 * g, .042 * g], [.075 * g, .066 * g, .052 * g, .03 * g], sd(6), 'leather', foot_weights(s), ref=(0, 0, 1))


def build_barbarian(rig, M0):
    B = Body()
    barbarian_body(B, rig, M0)
    cw = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.0, .10), ('spine2', 1.18, .08), ('chest', 1.32, .06)])
    # the fur mantle: a thick shoulder cape of tawny fur with dark tips, a fur collar, tufted edge
    loft(B, [(0, 0, 1.55), (0, 0, 1.47), (0, 0, 1.39), (0, 0, 1.31)], [.15, .285, .335, .325], [.115, .20, .24, .23], sd(16), 'cloth', 'chest',
         caps=(True, False), vrange=(1, 0), dense=dn(2))
    if detail(1):
        tufts(B, 1.315, .326, .231, 16 if detail(0) else 10, .085, .034, 'cloth', 'chest')
    loft(B, [(0, 0, 1.47), (0, 0, 1.555)], [.125, .10], [.105, .09], sd(12), 'cloth', blend_chain(lambda p: p[2], [('chest', 0, 0), ('neck', 1.50, .03)]))   # collar
    if detail(0):                                                                                           # a hanging fur tail down the back
        cloth_sheet(B, 1.34, .78, .19, .26, .21, .26, 4, 4, 'cloth', 'cloth', cw, skirt_w(.98, .85, .5, .09), wob=.02, jag=.04)
    # head: bare, braided auburn hair and beard, headband, white warpaint on the cheeks
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, 1.46), (0, 0, 1.52)], [.075, .065], [.075, .065], sd(8), 'skin', wh)
    loft(B, [(0, 0, 1.52), (0, 0, 1.60), (0, 0, 1.67), (0, 0, 1.73)], [.088, .104, .102, .066], [.096, .112, .106, .066], sd(14), 'skin', wh, dense=dn(2))
    for sg in (1, -1):
        box(B, (.112, sg * .048, 1.628), (.012, .048, .028), 'dark', 'head')
        if detail(1):
            box(B, (.113, sg * .048, 1.66), (.012, .062, .012), 'dark', 'head', rot=rotx(sg * -14))        # a heavy scowling brow
            for k in (-1, 0, 1):
                box(B, (.1, sg * (.075 + .012 * k), 1.585), (.012, .008, .05), 'accent', 'head')            # warpaint on the cheek
    box(B, (.119, 0, 1.585), (.026, .022, .052), 'skin', 'head')
    loft(B, [(-.005, 0, 1.655), (-.005, 0, 1.70), (-.01, 0, 1.76)], [.108, .106, .06], [.114, .11, .062], sd(12), 'emblem2', 'head', caps=(False, True), dense=dn(2))   # hair
    loft(B, [(0, 0, 1.675), (0, 0, 1.70)], [.11, .11], [.116, .116], sd(12), 'trim', 'head', caps=(False, False))                                 # headband
    loft(B, [(.08, 0, 1.56), (.115, 0, 1.47), (.10, 0, 1.38), (.06, 0, 1.30)], [.06, .066, .05, 0], [.034, .05, .04, 0], sd(8), 'emblem2', 'head',
         caps=(True, False), ref=(0, 1, 0), dense=dn(2))                                                      # beard
    for sg in (1, -1):                                                                                      # two braids over the mantle
        loft(B, [(-.04, sg * .09, 1.64), (-.09, sg * .12, 1.52), (-.09, sg * .13, 1.40), (-.07, sg * .12, 1.30)], [.03, .028, .026, .0], [.03, .028, .026, .0],
             sd(6), 'emblem2', 'head', caps=(True, False), ref=(0, 1, 0))
    # greataxe in the right hand: a long haft, a double-bit crescent head, bronze fittings, ember runes on the blades. No spikes.
    g = hand_grip(rig, M0, 'R')
    ax = unit((math.cos(deg(60)), 0, math.sin(deg(60))))
    fw = unit((math.sin(deg(60)), 0, -math.cos(deg(60))))
    B.push(rig.to_bind(M0, 'weapon_socket'))
    ws = 'weapon_socket'
    loft(B, [g - ax * .30, g + ax * .20, g + ax * 1.05], [.026, .026, .029], [.026, .026, .029], sd(6), 'leather', ws, ref=(0, 1, 0), dense=dn(2))
    loft(B, [g - ax * .33, g - ax * .29], [.0, .04], [.0, .04], sd(6), 'trim', ws, caps=(True, False), ref=(0, 1, 0))
    for off in ((.0, .35, .70) if detail(1) else (.35,)):
        loft(B, [g + ax * (off + .02), g + ax * (off + .06)], [.034, .034], [.034, .034], sd(6), 'trim', ws, caps=(False, False), ref=(0, 1, 0))
    top = g + ax * 1.04
    rot3 = np.column_stack((fw, (0, 1, 0), ax))
    blade = [(.03, .12), (.14, .15), (.24, .23), (.31, .09), (.31, -.09), (.24, -.23), (.14, -.15), (.03, -.12)]
    prism(B, blade, top, fw, ax, np.cross(fw, ax), .015, 'blade', 'blade', ws, taper=.55)
    prism(B, [(-x, y) for x, y in blade], top, fw, ax, np.cross(fw, ax), .015, 'blade', 'blade', ws, taper=.55)
    box(B, top, (.13, .075, .20), 'trim', ws, rot=rot3)
    loft(B, [top + ax * .09, top + ax * .15], [.035, .0], [.035, .0], sd(6), 'trim', ws, caps=(True, False), ref=(0, 1, 0))
    for sgn in (1, -1):
        rune_faces(B, top + fw * (.17 * sgn) + ax * .0, (0, 1, 0), fw, ws, .045, .07, .0165)
    B.pop()
    return B, BARBARIAN_ATLAS


BARBARIAN_ATLAS = {
    'plate': ('bands', dict(base=(.55, .34, .20), line=(.86, .66, .42), spacing=64)),         # tanned hide bracers
    'trim': ('scales', dict(a=(.88, .60, .26), b=(.70, .44, .18))),                           # bronze
    'cloth': ('folds', dict(base=(.80, .62, .38), period=26, hem=(.42, .27, .16), hem2=(.30, .18, .10))),   # fur, repainted by paint_fur
    'leather': ('weave', dict(base=(.50, .31, .18))),
    'blade': ('blade', dict(base=(.74, .78, .83))),
    'glow': ('flat', dict(base=(1.0, .50, .22))),
    'rune': ('rune', dict(bg=(.30, .10, .05), fg=(1.0, .55, .24))),
    'skin': ('flat', dict(base=(.78, .57, .42))),
    'bone': ('flat', dict(base=(.95, .91, .80))),
    'accent': ('flat', dict(base=(.96, .95, .90))),                                           # white paint
    'dark': ('flat', dict(base=(.17, .12, .10))),
    'emblem': ('flat', dict(base=(.78, .57, .42))),                                           # warpaint skin, repainted by paint_warpaint
    'emblem2': ('flat', dict(base=(.58, .27, .13))),                                          # auburn hair and beard
}


def paint_fur(A, region, spec, seed):
    """Tawny fur: pale roots, streaks, dark tips towards the hem (the tile's bottom rows)."""
    rng = np.random.default_rng(seed + 11)
    n = K.TILE
    nz = K.smooth_noise(rng, n, 16)
    yy, xx = np.mgrid[0:n, 0:n]
    col = np.array(spec[region][1]['base'], np.float32)
    tip = np.array((.34, .21, .12), np.float32)
    streak = np.repeat(rng.random(n // 3 + 1), 3)[:n][None, :] * np.ones((n, 1))
    wob = np.sin(yy * .07 + xx * .02 + rng.random() * 6) * .5 + .5
    t = np.ones((n, n, 4), np.float32)
    shade = (.70 + .28 * streak + .16 * nz) * (.92 + .1 * wob)
    k = np.clip((yy - n * .55) / (n * .45), 0, 1)[..., None] ** 1.3
    t[..., :3] = (col * shade[..., None]) * (1 - k) + tip * k * (.8 + .4 * streak[..., None])
    i = K.REGIONS.index(region)
    c, r = i % 4, i // 4
    A[r * K.TILE:(r + 1) * K.TILE, c * K.TILE:(c + 1) * K.TILE] = t


def paint_warpaint(A, region, spec, seed):
    """Bare skin with white stripes and woad-blue bands (the torso and arms): longitudinal lines plus two rings."""
    rng = np.random.default_rng(seed + 13)
    n = K.TILE
    nz = K.smooth_noise(rng, n, 16)
    yy, xx = np.mgrid[0:n, 0:n]
    col = np.array(spec[region][1]['base'], np.float32)
    t = np.ones((n, n, 4), np.float32)
    t[..., :3] = col * (.9 + .16 * nz[..., None])
    white, blue = np.array((.96, .95, .90), np.float32), np.array((.18, .34, .62), np.float32)
    for cx in (.10, .30, .50, .70, .90):
        m = (np.abs(xx / n - cx) < .016) & (yy > n * .08) & (yy < n * .92)
        t[m, :3] = white
    for y0, y1 in ((.30, .38), (.62, .66)):
        m = (yy / n > y0) & (yy / n < y1)
        t[m, :3] = blue * (.9 + .2 * nz[m][:, None])
    i = K.REGIONS.index(region)
    c, r = i % 4, i // 4
    A[r * K.TILE:(r + 1) * K.TILE, c * K.TILE:(c + 1) * K.TILE] = t


# ------------------------------------------------------------------ MONK (agile body, unarmed)
def build_monk(rig, M0):
    B = Body()
    agile_body(B, rig, M0, jerkin='cloth', sleeve='skin', pants='plate', boot='leather', cuff='accent', glove='accent', belt='trim',
               buckle='trim')
    sk = skirt_w(1.06, .8, .6, .06)
    # a short ochre tabard front and back over the saffron gi, edged in dark trim
    zt, zb = 1.34, (.80 if detail(0) else .82)
    hw = .105
    panel(B, (.176, -hw, zt), (.176, hw, zt), (.19, hw, zb), (.19, -hw, zb), 2, 3 if detail(0) else 2, 'plate', sk)
    panel(B, (-.162, hw, zt), (-.162, -hw, zt), (-.176, -hw, zb), (-.176, hw, zb), 2, 3 if detail(0) else 2, 'plate', sk)
    for xx, sgn in ((.185, 1), (-.17, -1)):
        for sg in (1, -1):
            box(B, (xx + sgn * .004, sg * hw, (zt + zb) / 2), (.012, .018, zt - zb), 'trim', sk)                  # dark edge strips
        box(B, (xx + sgn * .004, 0, zb), (.012, hw * 2, .02), 'trim', sk)                                         # dark hem
    # wide sash with two tails (dark rust over the saffron), the long tail on the left hip
    loft(B, [(0, 0, 1.00), (0, 0, 1.115)], [.175, .175], [.128, .128], sd(12), 'emblem', 'pelvis')
    panel(B, (.12, .07, 1.08), (.15, .18, 1.08), (.19, .21, .52), (.14, .10, .52), 2, 3 if detail(0) else 2, 'emblem', skirt_w(1.06, .8, .6, .06))
    panel(B, (.12, -.16, 1.08), (.15, -.06, 1.08), (.15, -.04, .72), (.12, -.13, .72), 2, 2, 'emblem', skirt_w(1.06, .8, .6, .06))
    for sg, s_ in SIDES:                                                                                    # cream hand wraps up the forearms (over the body's bracer)
        sh, el, wr, d = arm_frame(s_)
        loft(B, [el + d * .085, wr - d * .01], [.053, .047], [.053, .047], sd(8), 'accent', f'forearm.{s_}')
    if detail(1):                                                                                           # dark wrap rings on the forearms and shins
        for sg, s_ in SIDES:
            sh, el, wr, d = arm_frame(s_)
            for off in (.10, .15, .20):
                loft(B, [el + d * off, el + d * (off + .018)], [.0545, .0545], [.0545, .0545], sd(8), 'trim', f'forearm.{s_}', caps=(False, False))
            kn = np.array(JOINTS[f'shin.{s_}'][1])
            for z in (.26, .22, .18, .14):
                loft(B, [(kn[0], kn[1], z), (kn[0], kn[1], z - .016)], [.056, .056], [.060, .060], sd(8), 'trim', f'shin.{s_}', caps=(False, False))
    if detail(1):                                                                                           # a bo staff slung across the back
        a, b = np.array((-.20, -.19, .62)), np.array((-.19, .17, 1.74))
        loft(B, [a, (a + b) / 2, b], [.019, .019, .019], [.019, .019, .019], sd(6), 'leather', 'chest', ref=(1, 0, 0), dense=dn(2))
        for t_ in (0.0, .5, 1.0):
            loft(B, [a + (b - a) * t_, a + (b - a) * t_ + unit(b - a) * .05 * (1 if t_ < 1 else -1)], [.026, .026], [.026, .026], sd(6), 'trim', 'chest', caps=(False, False), ref=(1, 0, 0))
        for t_ in (.12, .88):
            loft(B, [a + (b - a) * t_, a + (b - a) * t_ + unit(b - a) * .03], [.024, .024], [.024, .024], sd(6), 'emblem', 'chest', caps=(False, False), ref=(1, 0, 0))
    # prayer beads with one glowing bead, the head: bare, headband with knot tails, a small topknot
    wn = blend_chain(lambda p: p[2], [('chest', 0, 0), ('neck', 1.48, .03)])
    loft(B, [(0, 0, 1.40), (0, 0, 1.47)], [.125, .10], [.108, .092], sd(10), 'cloth', wn, caps=(False, False))      # collar
    if detail(1):
        for k in range(9):
            a = math.radians(-60 + k * 120 / 8)
            box(B, (.125 * math.cos(a) + .02, .125 * math.sin(a), 1.31 - .035 * abs(math.sin(a * 1.5))), (.02, .02, .02), 'trim', 'chest')
        box(B, (.158, 0, 1.275), (.026, .026, .03), 'glow', 'chest')
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, 1.46), (0, 0, 1.52)], [.066, .058], [.066, .058], sd(8), 'skin', wh)
    loft(B, [(0, 0, 1.52), (0, 0, 1.60), (0, 0, 1.67), (0, 0, 1.73)], [.082, .098, .096, .062], [.09, .106, .1, .062], sd(14), 'skin', wh, dense=dn(2))
    for sg in (1, -1):
        box(B, (.104, sg * .045, 1.625), (.012, .045, .026), 'dark', 'head')
        if detail(1):
            box(B, (.103, sg * .045, 1.652), (.012, .05, .007), 'dark', 'head')
    box(B, (.112, 0, 1.585), (.022, .02, .05), 'skin', 'head')
    loft(B, [(0, 0, 1.675), (0, 0, 1.70)], [.101, .1], [.108, .106], sd(12), 'trim', 'head', caps=(False, False))   # dark headband
    if detail(1):
        box(B, (-.105, 0, 1.69), (.03, .06, .05), 'emblem', 'head')                                                 # knot
        for sg in (1, -1):
            loft(B, [(-.11, sg * .02, 1.685), (-.17, sg * .05, 1.60), (-.19, sg * .06, 1.52)], [.016, .014, .0], [.014, .012, .0], 4, 'emblem', 'head',
                 caps=(True, False), ref=(0, 0, 1))
    loft(B, [(-.02, 0, 1.73), (-.035, 0, 1.775), (-.045, 0, 1.805)], [.05, .042, .0], [.05, .042, .0], sd(8), 'dark', 'head', caps=(True, False), ref=(0, 1, 0))   # topknot
    return B, MONK_ATLAS


MONK_ATLAS = {
    'plate': ('bands', dict(base=(.76, .52, .16), line=(.98, .84, .50), spacing=64)),         # ochre
    'trim': ('scales', dict(a=(.24, .17, .14), b=(.34, .22, .16))),                           # dark brown trim
    'cloth': ('folds', dict(base=(.98, .60, .10), period=32, hem=(.30, .17, .10), hem2=(.60, .34, .10))),   # saffron
    'leather': ('weave', dict(base=(.40, .27, .17))),
    'blade': ('blade', dict(base=(.84, .88, .94))),
    'glow': ('flat', dict(base=(1.0, .86, .38))),
    'rune': ('rune', dict(bg=(.30, .18, .06), fg=(1.0, .86, .38))),
    'skin': ('flat', dict(base=(.80, .60, .45))),
    'bone': ('flat', dict(base=(.96, .92, .82))),
    'accent': ('flat', dict(base=(.97, .93, .80))),                                           # hand and shin wraps
    'dark': ('flat', dict(base=(.16, .12, .12))),
    'emblem': ('folds', dict(base=(.62, .22, .10), period=24, hem=(.98, .62, .14), hem2=(.30, .17, .10))),   # sash: dark rust
    'emblem2': ('flat', dict(base=(.40, .27, .17))),
}


# ------------------------------------------------------------------ PALADIN (heavy body, polished plate)
def crest_fin(B, region, wspec):
    """A tall horsehair fin from brow to nape."""
    path = [(.085, 0, 1.712), (.03, 0, 1.762), (-.04, 0, 1.77), (-.10, 0, 1.73), (-.14, 0, 1.65), (-.15, 0, 1.57)]
    ra = [.022, .046, .052, .048, .04, .02]
    rb = [.018, .022, .024, .022, .02, .014]
    loft(B, path, ra, rb, sd(6), region, wspec, caps=(True, True), ref=(0, 1, 0), dense=dn(2))


def build_paladin(rig, M0):
    B = Body()
    R1.heavy_body(B, rig, M0, plate='plate', trim='trim', leather='leather', fist='plate')
    pauldron_round(B, 'plate', 'trim')
    # closed helm: visor with a cross slit, gold brow band and cheek bars, a purple crest fin, a glowing brow gem
    hz = [1.50, 1.56, 1.63, 1.69, 1.73]
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, z) for z in hz], [.094, .107, .107, .087, .047], [.102, .117, .117, .092, .052], sd(12), 'plate', wh, dense=dn(2))
    box(B, (.12, 0, 1.62), (.012, .15, .02), 'dark', 'head')                                              # visor slit
    box(B, (.12, 0, 1.585), (.012, .02, .08), 'dark', 'head')                                             # cross bar slit
    box(B, (.10, 0, 1.685), (.03, .205, .03), 'trim', 'head')                                             # gold brow band
    box(B, (.127, 0, 1.70), (.012, .03, .03), 'glow', 'head')
    if detail(1):
        for sg in (1, -1):
            box(B, (.045, sg * .10, 1.54), (.10, .025, .10), 'plate', 'head', rot=rotz(-sg * 12))
            box(B, (.12, sg * .06, 1.55), (.012, .012, .09), 'trim', 'head')                                # gold cheek bars
    crest_fin(B, 'accent', 'head')
    # white cape lined in royal purple, a purple heraldic tabard to the knee with the crown device
    cw = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.0, .10), ('spine2', 1.18, .08), ('chest', 1.32, .06)])
    nx, nz_ = (5, 6) if detail(0) else (3, 3)
    cloth_sheet(B, 1.44, .50, .22, .36, .19, .30, nx, nz_, 'cloth', 'accent', cw, skirt_w(.98, .85, .5, .09), wob=.02)
    sk = skirt_w(1.06, .8, .6, .06)
    zt, zb = 1.34, (.46 if detail(0) else .50)
    hw = .125
    nzz = 3 if detail(0) else 2
    panel(B, (.19, -hw, zt), (.19, hw, zt), (.215, hw, zb), (.215, -hw, zb), 2, nzz, 'emblem', sk)
    panel(B, (-.17, hw, zt), (-.17, -hw, zt), (-.195, -hw, zb), (-.195, hw, zb), 2, nzz, 'emblem', sk)
    # tall tower shield (rigid on shield_socket): purple field, silver border, a gold rim and a glowing boss
    outline = [(-.16, .34), (-.11, .42), (.11, .42), (.16, .34), (.17, -.34), (.12, -.42), (-.12, -.42), (-.17, -.34)]
    tower_shield(B, rig, M0, outline, .36, .88, 'emblem2', 'leather', 'trim', 'glow')
    # warhammer in the right hand: a long haft, a square head with a back beak, gold bands, a glowing pommel gem
    g = hand_grip(rig, M0, 'R')
    ax = unit((math.cos(deg(62)), 0, math.sin(deg(62))))
    fw = unit((math.sin(deg(62)), 0, -math.cos(deg(62))))
    B.push(rig.to_bind(M0, 'weapon_socket'))
    ws = 'weapon_socket'
    loft(B, [g - ax * .15, g + ax * .20, g + ax * .86], [.021, .021, .025], [.021, .021, .025], sd(6), 'leather', ws, ref=(0, 1, 0), dense=dn(2))
    loft(B, [g - ax * .19, g - ax * .14], [.0, .03], [.0, .03], sd(6), 'trim', ws, caps=(True, False), ref=(0, 1, 0))
    box(B, g - ax * .20, (.026, .026, .026), 'glow', ws)
    for off in ((.12, .50) if detail(1) else (.5,)):
        loft(B, [g + ax * off, g + ax * (off + .05)], [.03, .03], [.03, .03], sd(6), 'trim', ws, caps=(False, False), ref=(0, 1, 0))
    hc = g + ax * .90
    rot3 = np.column_stack((fw, (0, 1, 0), ax))
    box(B, hc, (.20, .10, .13), 'plate', ws, rot=rot3)
    box(B, hc + fw * .105, (.025, .11, .14), 'trim', ws, rot=rot3)                                         # gold striking face
    box(B, hc - fw * .12, (.07, .06, .07), 'plate', ws, rot=rot3)
    loft(B, [hc - fw * .15, hc - fw * .30], [.03, .0], [.03, .0], sd(5), 'plate', ws, caps=(True, False), ref=(0, 0, 1))                # back beak
    loft(B, [hc + ax * .07, hc + ax * .15], [.025, .0], [.025, .0], sd(5), 'trim', ws, caps=(True, False), ref=(0, 1, 0))
    if detail(1):
        rune_faces(B, hc + fw * .0, (0, 1, 0), fw, ws, .06, .05, .0505)
    B.pop()
    return B, PALADIN_ATLAS


PALADIN_ATLAS = {
    'plate': ('bands', dict(base=(.86, .88, .94), line=(1.0, 1.0, 1.0), spacing=32)),         # polished silver
    'trim': ('scales', dict(a=(1.0, .84, .38), b=(.90, .70, .26))),                           # gold
    'cloth': ('folds', dict(base=(.95, .95, .99), period=34, hem=(.56, .22, .70), hem2=(1.0, .84, .38))),   # white cape, purple hem
    'leather': ('weave', dict(base=(.34, .22, .34))),
    'blade': ('blade', dict(base=(.88, .90, .96))),
    'glow': ('flat', dict(base=(.92, .82, 1.0))),
    'rune': ('rune', dict(bg=(.20, .08, .30), fg=(.90, .80, 1.0))),
    'skin': ('flat', dict(base=(.84, .66, .52))),
    'bone': ('flat', dict(base=(.96, .94, .90))),
    'accent': ('flat', dict(base=(.58, .22, .72))),                                           # royal purple: crest and cape lining
    'dark': ('flat', dict(base=(.12, .10, .16))),
    'emblem': ('flat', dict(base=(.52, .18, .64))),                                           # tabard: purple, crown device painted by paint_crown
    'emblem2': ('flat', dict(base=(.52, .18, .64))),                                          # shield face
}


def paint_crown(A, region, spec, seed):
    """Heraldic device, original: a three-pointed crown with jewelled tips on a royal purple field, gold border (the tabard) or
    silver border (the shield)."""
    rng = np.random.default_rng(seed + 17)
    n = K.TILE
    nz = K.smooth_noise(rng, n, 16)
    yy, xx = np.mgrid[0:n, 0:n]
    u = (xx + .5) / n * 2 - 1
    v = -((yy + .5) / n * 2 - 1)                                           # up is +
    base = np.array(spec[region][1]['base'], np.float32)
    t = np.ones((n, n, 4), np.float32)
    t[..., :3] = base * (.92 + .14 * nz[..., None])
    border_col = np.array((1.0, .84, .38) if region == 'emblem' else (.90, .92, .98), np.float32)
    t[(np.abs(u) > .88) | (np.abs(v) > .93), :3] = border_col
    mark = np.array((.97, .96, .98), np.float32)
    band = (np.abs(u) < .46) & (v > -.28) & (v < -.08)
    peaks = np.zeros_like(band)
    for cx, top in ((-.38, .24), (0.0, .42), (.38, .24)):
        w = .17 * np.clip((top - v) / (top + .08), 0, 1)
        peaks |= (np.abs(u - cx) < w) & (v > -.08) & (v < top)
    t[band | peaks, :3] = mark
    for cx, top in ((-.38, .24), (0.0, .42), (.38, .24)):
        t[np.hypot(u - cx, v - top - .04) < .06, :3] = border_col
    t[(np.abs(u) < .46) & (v > -.34) & (v < -.28), :3] = border_col
    i = K.REGIONS.index(region)
    c, r = i % 4, i // 4
    A[r * K.TILE:(r + 1) * K.TILE, c * K.TILE:(c + 1) * K.TILE] = t


# ------------------------------------------------------------------ RANGER (agile body, longbow)
def longbow(B, rig, M0, pose):
    """A longbow in the left hand, authored where the left hand is in `pose` (the Ranged hold) so it is upright while drawn; the
    right hand draws with no string coupling. Rigid on shield_socket."""
    M = rig.fk(*pose)
    h, t = rig.head(M, 'hand.L'), rig.tail(M, 'hand.L')
    cg = h + (t - h) * .6
    fa = rig.tail(M, 'forearm.L') - rig.head(M, 'forearm.L')
    aim = unit((fa[0], fa[1], 0.0))
    B.push(rig.to_bind(M, 'shield_socket'))
    ws = 'shield_socket'
    half = .70
    zs = np.linspace(-half, half, 9 if detail(0) else 7 if detail(1) else 5)
    pts = [cg + (0, 0, z) - aim * (.20 * (z / half) ** 2) for z in zs]
    rad = [.011 + .011 * (1 - abs(z / half)) ** 1.5 for z in zs]
    loft(B, pts, rad, rad, sd(6), 'leather', ws, caps=(True, True), ref=(0, 0, 1), dense=1)
    loft(B, [cg + (0, 0, -.09), cg + (0, 0, .09)], [.019, .019], [.019, .019], sd(6), 'accent', ws, ref=(0, 0, 1))                        # grip wrap
    for sg in (1, -1):
        loft(B, [pts[-1] if sg > 0 else pts[0], (pts[-1] if sg > 0 else pts[0]) + np.array((0, 0, .03 * sg))], [.014, .0], [.014, .0], 4, 'trim', ws,
             caps=(True, False), ref=(1, 0, 0))
    loft(B, [pts[0], pts[-1]], [.0035, .0035], [.0035, .0035], 4, 'bone', ws, ref=(1, 0, 0))                                                 # bowstring
    box(B, cg + aim * .03 + (0, 0, .12), (.016, .016, .016), 'glow', ws)                                                                     # gem set in the riser
    B.pop()


def build_ranger(rig, M0):
    B = Body()
    agile_body(B, rig, M0, jerkin='plate', sleeve='leather', pants='leather', boot='leather', cuff='trim', glove='leather',
               belt='leather', buckle='trim')
    cw = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.0, .10), ('spine2', 1.18, .08), ('chest', 1.32, .06)])
    if detail(1):                                                                                           # tunic lacing and a cross-body quiver strap
        box(B, (.128, 0, 1.28), (.02, .05, .30), 'accent', 'chest')
        box(B, (.134, -.02, 1.24), (.022, .05, .48), 'leather', 'chest', rot=rotx(28))
        box(B, (.143, -.02, 1.24), (.016, .045, .05), 'trim', 'chest')
    sk = skirt_w(1.06, .8, .6, .06)
    panel(B, (.11, -.07, 1.07), (.15, -.17, 1.07), (.18, -.18, .66), (.13, -.09, .66), 2, 2, 'cloth', sk)       # short green tail at the right hip
    if detail(1):
        for sg in (1, -1):
            box(B, (.02, sg * .185, .92), (.07, .05, .085), 'leather', 'pelvis')                           # hip pouches
            box(B, (.02, sg * .216, .93), (.04, .012, .035), 'trim', 'pelvis')
    # head under a deep pointed hood with a drooping tail, lined in dark green, a cream feather
    wn = blend_chain(lambda p: p[2], [('chest', 0, 0), ('neck', 1.48, .03)])
    loft(B, [(0, 0, 1.42), (0, 0, 1.51)], [.115, .095], [.10, .088], sd(10), 'accent', wn)                   # cream collar
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, 1.50), (0, 0, 1.58), (0, 0, 1.68)], [.075, .085, .09], [.08, .092, .095], sd(10), 'skin', wh)
    for sg in (1, -1):
        box(B, (.092, sg * .04, 1.625), (.012, .045, .02), 'dark', 'head')
        box(B, (.094, sg * .04, 1.645), (.010, .05, .008), 'dark', 'head')
    hood = [(-.015, 0, 1.50), (-.015, 0, 1.58), (-.03, 0, 1.67), (-.08, 0, 1.735), (-.16, 0, 1.735), (-.225, 0, 1.675)]
    hra, hrb = [.118, .126, .126, .108, .07, .02], [.124, .134, .134, .116, .08, .03]
    arc_loft(B, hood, hra, hrb, sd(12), 'cloth', wh, a0=62, a1=298, vrange=(1, 0))
    arc_loft(B, hood, [x - .012 for x in hra], [x - .012 for x in hrb], sd(12), 'emblem', wh, a0=62, a1=298, flip=True)
    loft(B, [(-.01, 0, 1.485), (-.01, 0, 1.50)], [.14, .136], [.13, .126], sd(12), 'trim', wh, caps=(False, False))     # russet hood hem
    if detail(1):
        loft(B, [(-.01, .115, 1.69), (-.09, .13, 1.75), (-.18, .12, 1.80)], [.016, .014, .0], [.006, .006, .0], 4, 'accent', wh, caps=(True, False), ref=(1, 0, 0))   # feather
    # shoulder mantle and a short cape to the hip (moss green, russet hem, dark lining)
    loft(B, [(0, 0, 1.50), (0, 0, 1.43), (0, 0, 1.37)], [.13, .24, .27], [.10, .165, .19], sd(14), 'cloth', 'chest', caps=(True, False), vrange=(1, 0), dense=dn(2))
    loft(B, [(0, 0, 1.382), (0, 0, 1.362)], [.267, .271], [.187, .191], sd(14), 'trim', 'chest', caps=(False, False))
    nx, nz_ = (5, 5) if detail(0) else (4, 3) if detail(1) else (3, 3)
    cloth_sheet(B, 1.40, .62, .20, .34, .17, .25, nx, nz_, 'cloth', 'emblem', cw, skirt_w(.98, .85, .5, .09), wob=.02, jag=.025 if detail(0) else 0.0)
    for sg, sd_ in SIDES:                                                                                    # leather spaulders
        pc = np.array((0, sg * .245, 1.42))
        loft(B, [pc + (0, 0, z) for z in (-.04, .0, .035, .06)], [.10, .115, .09, .03], [.09, .104, .082, .028], sd(10), 'plate', f'clavicle.{sd_}', dense=dn(2))
        loft(B, [pc + (0, 0, -.04), pc + (0, 0, -.05)], [.10, .097], [.09, .087], sd(10), 'trim', f'clavicle.{sd_}')
    # quiver on the back, tilted, with fletched arrows
    if detail(1):
        qa = unit((.20, -.30, 1.0))
        q0 = np.array((-.17, .10, 1.00))
        loft(B, [q0, q0 + qa * .50], [.052, .056], [.052, .056], sd(8), 'leather', 'chest', ref=(1, 0, 0), dense=dn(2))
        loft(B, [q0 + qa * .44, q0 + qa * .48], [.060, .060], [.060, .060], sd(8), 'trim', 'chest', caps=(False, False), ref=(1, 0, 0))
        for k, (dy, dx) in enumerate(((-.02, .0), (.025, -.01), (.0, .03), (.03, .02))[:4 if detail(0) else 2]):
            top = q0 + qa * .49 + np.array((dx, dy, 0))
            loft(B, [top, top + qa * .18], [.006, .006], [.006, .006], 4, 'bone', 'chest', ref=(1, 0, 0))
            for sgn in (1, -1):                                                                              # fletching, two single-sided quads (no duplicate faces)
                quad(B, top + qa * .15, np.array((0, .02, 0)), qa * .03 * sgn, 'accent', 'chest')
    # short blade in the right hand (a hunting sword), the longbow in the left, authored at the Ranged hold
    g = hand_grip(rig, M0, 'R')
    B.push(rig.to_bind(M0, 'weapon_socket'))
    dagger(B, g, (math.cos(deg(48)), 0, math.sin(deg(48))), 'weapon_socket', blade=.40, rune=True)
    B.pop()
    longbow(B, rig, M0, CL.CLIPS['Ranged'][2](22))
    return B, RANGER_ATLAS


RANGER_ATLAS = {
    'plate': ('bands', dict(base=(.60, .52, .26), line=(.90, .80, .52), spacing=64)),         # olive-tan leather tunic
    'trim': ('scales', dict(a=(.78, .44, .18), b=(.62, .32, .12))),                           # russet
    'cloth': ('folds', dict(base=(.44, .56, .20), period=30, hem=(.80, .52, .22), hem2=(.60, .36, .14))),   # moss green, russet hem
    'leather': ('weave', dict(base=(.46, .30, .18))),
    'blade': ('blade', dict(base=(.88, .92, .96))),
    'glow': ('flat', dict(base=(.86, 1.0, .42))),
    'rune': ('rune', dict(bg=(.14, .20, .05), fg=(.86, 1.0, .42))),
    'skin': ('flat', dict(base=(.80, .62, .48))),
    'bone': ('flat', dict(base=(.95, .92, .82))),
    'accent': ('flat', dict(base=(.95, .91, .76))),                                           # cream: collar, feather, grip, fletching
    'dark': ('flat', dict(base=(.14, .16, .10))),
    'emblem': ('folds', dict(base=(.20, .34, .17), period=26, hem=(.45, .30, .14), hem2=(.30, .20, .10))),   # dark green lining
    'emblem2': ('flat', dict(base=(.46, .30, .18))),
}


CHARACTERS = {   # key: (builder, name, atlas seed)
    'barbarian': (build_barbarian, 'SK_Hero_Barbarian', 1476),
    'monk': (build_monk, 'SK_Hero_Monk', 1477),
    'paladin': (build_paladin, 'SK_Hero_Paladin', 1478),
    'ranger': (build_ranger, 'SK_Hero_Ranger', 1479),
}
ASSET_IDS = {'barbarian': 'sk-hero-barbarian', 'monk': 'sk-hero-monk', 'paladin': 'sk-hero-paladin', 'ranger': 'sk-hero-ranger'}
BATCH2 = tuple(CHARACTERS)

PAINTERS = {   # key -> [(region, fn(A, region, spec, seed))]: marks painted over the base atlas
    'barbarian': [('cloth', paint_fur), ('emblem', paint_warpaint)],
    'paladin': [('emblem', paint_crown), ('emblem2', paint_crown)],
}


def make_atlas(key, spec, seed):
    A = C.make_atlas(key, spec, seed)
    for region, fn in PAINTERS.get(key, ()):
        fn(A, region, spec, seed)
    return A


C.CHARACTERS.update(CHARACTERS)
C.ASSET_IDS.update(ASSET_IDS)
