"""Character recipes for the #150 rigged cast, batch 4: bard and artificer (the last two classes).

Same skeleton, atlas layout, LOD helpers and conventions as batches 0 to 3 (Art/rig-pilot/r001/README.md, cast/README.md,
cast/README_BATCH2.md, cast/README_BATCH3.md): the 25-bone armature, one material on its own 1024x1024 atlas of 13 tiles, RGBA vertex colour
with the glow mask in alpha on runes, gems and the focus only, at most 4 influences, the ten shared clips (Brace included). This module
registers its classes in cast_characters.CHARACTERS / ASSET_IDS in memory and imports cast_batch3 (so the Brace clip and the earlier helpers
come with it); no batch 1, 2 or 3 file is edited, so their asset records keep their hashes.

Each recipe is build_<name>(rig, M0) -> (Body, atlas_spec); M0 is the posed FK of Idle frame 0.
"""
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import cast_characters as C                                                     # noqa: E402  (puts r001/scripts on sys.path)
import cast_batch3 as B3                                                        # noqa: E402  (registers batches 2 and 3 and the Brace clip)
import clips as CL                                                              # noqa: E402,F401
import rigkit as K                                                              # noqa: E402
from rigkit import Body, JOINTS, blend_chain, box, deg, detail, dn, frame_matrix, loft, panel, prism, quad, sd, spike   # noqa: E402

SIDES = C.SIDES
rotx, roty, rotz = C.rotx, C.roty, C.rotz
skirt_w, hand_grip, rune_faces = C.skirt_w, C.hand_grip, C.rune_faces
unit, arc_loft, cloth_sheet, agile_body = C.unit, C.arc_loft, C.cloth_sheet, C.agile_body
arm_frame = B3.arm_frame
leaf, edge_tube = B3.leaf, B3.edge_tube
R1 = C.R1


# ------------------------------------------------------------------ BARD (agile body, doublet, lute on the back, rapier, feathered cap)
def lute(B, wspec='chest'):
    """A lute slung diagonally across the back: a ribbed pear-shaped bowl at the right hip, a neck rising over the left shoulder, a bent
    peg-box, a glowing rosette and a few strings; a cross-body strap in front. Rigid on `chest`. Authored behind the cape."""
    a = unit((-.02, .48, .88))                       # bowl-to-peghead axis: up and to the figure's left
    base = np.array((-.285, -.10, .78))
    side = unit(np.cross(a, (1, 0, 0)))              # across the soundboard
    L = 0.40                                         # bowl length
    ts = np.linspace(0, 1, 9 if detail(0) else 6)
    prof_w = [0, .075, .125, .150, .150, .125, .085, .045, .028]
    prof_d = [0, .045, .072, .088, .090, .075, .052, .032, .024]
    idx = np.linspace(0, len(prof_w) - 1, len(ts)).astype(int)
    path = [base + a * L * t for t in ts]
    loft(B, path, [prof_w[i] for i in idx], [prof_d[i] for i in idx], sd(12), 'plate', wspec, caps=(True, True), ref=(1, 0, 0), dense=dn(1))
    top = base + a * L
    # fingerboard neck, then a tilted peg-box with pegs
    n2 = top + a * .30
    loft(B, [top - a * .02, n2], [.030, .026], [.020, .018], sd(6), 'leather', wspec, ref=(1, 0, 0))
    ang = unit(a + np.array((-.55, 0, 0)))
    pb = n2 + ang * .15
    loft(B, [n2, pb], [.030, .034], [.018, .018], sd(6), 'leather', wspec, caps=(True, True), ref=(1, 0, 0))
    if detail(1):
        for k in range(3):
            c = n2 + ang * (.04 + .045 * k)
            box(B, c + side * .036, (.020, .014, .014), 'trim', wspec)
            box(B, c - side * .036, (.020, .014, .014), 'trim', wspec)
    # rosette: a glowing ring on the soundboard (facing +X, away from the camera at the back? it is on the front face towards the body)
    rc = base + a * L * .50 + np.array((.088, 0, 0))
    loft(B, [rc, rc + np.array((.006, 0, 0))], [.04, .036], [.04, .036], sd(8), 'glow', wspec, caps=(False, False), ref=(0, 0, 1))
    if detail(0):                                                                       # strings along the face
        for sgn in (-1, 0, 1):
            p0 = base + a * L * .78 + side * .008 * sgn + np.array((.087, 0, 0))
            p1 = n2 + side * .008 * sgn + np.array((.03, 0, 0))
            loft(B, [p0, p1], [.0025, .0025], [.0025, .0025], 3, 'bone', wspec, ref=(1, 0, 0))
    # bridge and a gold edge band around the bowl
    box(B, base + a * L * .22 + np.array((.09, 0, 0)), (.014, .12, .012), 'trim', wspec, rot=frame_matrix(a, (1, 0, 0), 2))
    loft(B, [base + a * L * .50 + np.array((.0, 0, 0)) - np.array((.0, 0, 0)), base + a * L * .50 + a * .008], [.152, .152], [.092, .092], sd(12), 'trim', wspec,
         caps=(False, False), ref=(1, 0, 0))
    # strap: from the neck over the left shoulder, across the chest to the bowl at the right hip (gold stitched leather)
    p_sh = np.array((.0, .20, 1.455))
    p_hip = np.array((.0, -.19, .93))
    d = p_hip - p_sh
    box(B, (p_sh + p_hip) / 2 + np.array((.145, 0, 0)), (.020, .045, np.linalg.norm(d)), 'leather', 'chest', rot=frame_matrix(d, (1, 0, 0), 2))
    if detail(1):
        box(B, (p_sh + p_hip) / 2 + np.array((.157, 0, 0)), (.008, .012, np.linalg.norm(d)), 'trim', 'chest', rot=frame_matrix(d, (1, 0, 0), 2))
    box(B, p_sh + np.array((-.02, 0, .02)) * 0 + np.array((.03, -.01, -.02)), (.05, .06, .05), 'trim', 'chest')                  # shoulder buckle
    # the strap's back run, from the shoulder to the neck of the lute
    q = base + a * (L + .05)
    back = np.array((-.15, .17, 1.36))
    d2 = q - back
    box(B, (q + back) / 2, (.016, .03, np.linalg.norm(d2)), 'leather', 'chest', rot=frame_matrix(d2, (1, 0, 0), 2))


def rapier(B, g, ax, ws, blade=.78):
    """A rapier gripped at g, pointing along ax (authored where the hand is in Idle): wrapped grip, swept cup and quillons, a long thin blade
    and a glowing gold pommel gem."""
    ax = unit(ax)
    loft(B, [g - ax * .075, g + ax * .05], [.017, .017], [.017, .017], sd(6), 'leather', ws, ref=(0, 0, 1))
    loft(B, [g - ax * .10, g - ax * .075], [.0, .03], [.0, .03], sd(6), 'trim', ws, ref=(0, 0, 1))                       # pommel
    box(B, g + ax * .058, (.022, .21, .02), 'trim', ws, rot=frame_matrix(ax, (0, 0, 1), 0))                           # quillons
    loft(B, [g + ax * .03, g + ax * .075, g + ax * .095], [.062, .056, .0], [.062, .056, .0], sd(8), 'trim', ws, caps=(False, False), ref=(0, 0, 1))   # cup
    if detail(1):
        loft(B, [g + ax * .055, g + ax * .055 + unit(np.cross(ax, (0, 1, 0))) * .05, g + ax * .09], [.008, .008, .008], [.008, .008, .008], 4, 'trim', ws, ref=(0, 1, 0))   # knuckle bow
    loft(B, [g + ax * .095, g + ax * (.095 + blade * .5), g + ax * (.095 + blade)], [.0045, .004, 0], [.017, .014, 0], 4, 'blade', ws, caps=(True, False), ref=(0, 0, 1))
    box(B, g - ax * .115, (.026, .026, .026), 'glow', ws)
    if detail(0):
        rune_faces(B, g + ax * (.095 + blade * .30), (0, 1, 0), ax, ws, .06, .009, .006)


def feather(B, path, w, wspec, region='accent'):
    """A curved plume: a flat ribbon along `path` (width w at the widest point), cream, with a thin dark spine on the larger LODs."""
    path = [np.array(p, float) for p in path]
    n = len(path)
    ra = [w * s for s in np.sin(np.linspace(.25, math.pi * .92, n))]
    rb = [.006] * n
    loft(B, path, ra, rb, 4, region, wspec, caps=(True, True), ref=(0, 1, 0))


def build_bard(rig, M0):
    B = Body()
    # body: wine doublet over a cream shirt, teal hose, brown boots with gold cuffs
    agile_body(B, rig, M0, jerkin='cloth', sleeve='bone', pants='emblem', boot='leather', cuff='trim', glove='leather', belt='leather', buckle='trim')
    cw = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.0, .10), ('spine2', 1.18, .08), ('chest', 1.32, .06)])
    sk = skirt_w(1.06, .8, .6, .06)
    # puffed upper sleeves (cream, slashed wine) and a wine shoulder cap
    for sg, s in SIDES:
        sh, el, wr, d = arm_frame(s)
        along = lambda p, sh=sh, d=d: float(np.dot(np.array(p) - sh, d))
        arm = blend_chain(along, [(f'upperarm.{s}', 0, 0), (f'forearm.{s}', .30, .06)])
        loft(B, [sh + d * .01, sh + d * .08, sh + d * .17, el - d * .02], [.064, .085, .082, .052], [.064, .085, .082, .052], sd(10), 'bone', arm, caps=(True, False), dense=dn(2))
        loft(B, [sh + d * .02, sh + d * .075], [.084, .09], [.084, .09], sd(10), 'cloth', arm, caps=(False, False))
        if detail(1):
            for k in range(3):
                a_ = k * 2 * math.pi / 3 + .4
                off = np.array((math.cos(a_) * .088, math.sin(a_) * .088, 0))
                box(B, sh + d * .125 + off * (1 if abs(off[1]) > .03 else 1), (.012, .012, .09), 'cloth', arm, rot=frame_matrix(d, (1, 0, 0), 2))
    # peplum: a short flared skirt of the doublet with a gold hem
    zs = [1.03, .96, .88]
    loft(B, [(0, 0, z) for z in zs], [.175, .215, .245], [.125, .16, .185], sd(14), 'cloth', sk, caps=(False, False), vrange=(1, 0))
    loft(B, [(0, 0, .885), (0, 0, .858)], [.246, .25], [.186, .19], sd(14), 'trim', sk, caps=(False, False))
    # doublet detail: gold frogging down the chest and a high gold-edged collar
    if detail(1):
        for k, z in enumerate((1.14, 1.22, 1.30, 1.38)):
            box(B, (.148 - .004 * k, 0, z), (.014, .20 - .015 * k, .022), 'trim', 'chest')
        box(B, (.150, 0, 1.26), (.012, .02, .30), 'trim', 'chest')
    wn = blend_chain(lambda p: p[2], [('chest', 0, 0), ('neck', 1.48, .03)])
    loft(B, [(0, 0, 1.42), (0, 0, 1.455), (0, 0, 1.50)], [.118, .128, .098], [.104, .112, .09], sd(12), 'bone', wn, caps=(False, False))          # ruff
    if detail(1):
        loft(B, [(0, 0, 1.45), (0, 0, 1.462)], [.13, .13], [.114, .114], sd(12), 'trim', wn, caps=(False, False))
    # short teal cape lined in wine, to the hip, with a short mantle and a gold rim
    loft(B, [(0, 0, 1.50), (0, 0, 1.43), (0, 0, 1.36)], [.13, .25, .275], [.10, .17, .195], sd(14), 'emblem', 'chest', caps=(True, False), vrange=(1, 0), dense=dn(2))
    loft(B, [(0, 0, 1.372), (0, 0, 1.352)], [.272, .276], [.192, .196], sd(14), 'trim', 'chest', caps=(False, False))
    nx, nz_ = (5, 5) if detail(0) else (4, 3) if detail(1) else (3, 3)
    cloth_sheet(B, 1.40, .84, .20, .32, .17, .24, nx, nz_, 'emblem', 'cloth', cw, skirt_w(.98, .85, .5, .09), wob=.02, jag=.0)
    # head: bare face, a moustache and goatee, wavy auburn hair to the shoulder, a soft feathered cap
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, 1.46), (0, 0, 1.52)], [.068, .06], [.068, .06], sd(8), 'skin', wh)
    loft(B, [(0, 0, 1.52), (0, 0, 1.60), (0, 0, 1.67), (0, 0, 1.73)], [.083, .098, .096, .062], [.09, .106, .10, .062], sd(14), 'skin', wh, dense=dn(2))
    for sg in (1, -1):
        box(B, (.104, sg * .045, 1.625), (.012, .045, .026), 'dark', 'head')
        if detail(1):
            box(B, (.103, sg * .045, 1.652), (.012, .05, .008), 'emblem2', 'head', rot=rotx(sg * -8))
    box(B, (.112, 0, 1.585), (.022, .02, .05), 'skin', 'head')
    if detail(1):
        box(B, (.108, 0, 1.558), (.012, .07, .014), 'emblem2', 'head')                                         # moustache
        box(B, (.106, 0, 1.528), (.012, .026, .03), 'emblem2', 'head')                                         # goatee
    loft(B, [(-.01, 0, 1.64), (-.10, 0, 1.58), (-.14, 0, 1.47), (-.12, 0, 1.40)], [.105, .11, .09, .02], [.05, .06, .05, .02], sd(8), 'emblem2', 'head', caps=(True, False), ref=(0, 1, 0), dense=dn(2))     # hair behind
    for sg in (1, -1):
        loft(B, [(.0, sg * .10, 1.64), (-.03, sg * .115, 1.56), (-.04, sg * .115, 1.47)], [.025, .028, .0], [.02, .025, .0], sd(6), 'emblem2', 'head', caps=(True, False), ref=(1, 0, 0))   # side locks
    # cap: a soft wine crown, a teal band, a turned-up brim at the front and a long cream plume with a gold brooch
    loft(B, [(0, 0, 1.655), (0, 0, 1.69), (-.02, 0, 1.74), (-.06, 0, 1.765)], [.128, .13, .115, .06], [.134, .136, .12, .065], sd(14), 'cloth', 'head', caps=(False, True), dense=dn(2))
    loft(B, [(0, 0, 1.655), (0, 0, 1.685)], [.132, .133], [.138, .139], sd(14), 'emblem', 'head', caps=(False, False))
    loft(B, [(0, 0, 1.652), (0, 0, 1.658)], [.14, .14], [.15, .15], sd(14), 'trim', 'head', caps=(False, False))
    if detail(1):
        box(B, (.138, 0, 1.675), (.028, .15, .035), 'cloth', 'head', rot=roty(-18))                              # front brim, turned up
    box(B, (.098, .13, 1.67), (.02, .026, .03), 'glow', 'head')                                                 # brooch
    feather(B, [(.0, .13, 1.69), (-.08, .16, 1.735), (-.20, .165, 1.76), (-.32, .14, 1.75), (-.40, .10, 1.69)], .034, 'head')
    if detail(1):
        loft(B, [(.0, .13, 1.69), (-.08, .16, 1.735), (-.20, .165, 1.76), (-.32, .14, 1.75), (-.40, .10, 1.69)], [.003] * 5, [.010] * 5, 3, 'trim', 'head', ref=(0, 1, 0))
    # belt pouch and a small dagger scabbard (right hip), and the lute
    if detail(1):
        box(B, (.03, -.215, .94), (.075, .05, .09), 'leather', 'pelvis')
        box(B, (.03, -.243, .95), (.03, .012, .04), 'trim', 'pelvis')
    lute(B)
    # rapier in the right hand, point up and forward
    g = hand_grip(rig, M0, 'R')
    B.push(rig.to_bind(M0, 'weapon_socket'))
    rapier(B, g, (math.cos(deg(52)), 0, math.sin(deg(52))), 'weapon_socket')
    B.pop()
    return B, BARD_ATLAS


BARD_ATLAS = {
    'plate': ('bands', dict(base=(.76, .50, .26), line=(.46, .28, .14), spacing=48)),          # honey wood with ribs: the lute bowl
    'trim': ('scales', dict(a=(1.0, .84, .38), b=(.90, .62, .22))),                           # gold
    'cloth': ('folds', dict(base=(.66, .16, .32), period=30, hem=(.98, .84, .48), hem2=(.90, .64, .26))),   # wine doublet, gold hem
    'leather': ('weave', dict(base=(.46, .30, .20))),
    'blade': ('blade', dict(base=(.90, .93, .98))),
    'glow': ('flat', dict(base=(1.0, .84, .42))),                                             # warm gold
    'rune': ('rune', dict(bg=(.30, .14, .06), fg=(1.0, .84, .42))),
    'skin': ('flat', dict(base=(.86, .66, .52))),
    'bone': ('flat', dict(base=(.97, .93, .82))),                                             # cream shirt and ruff
    'accent': ('flat', dict(base=(.98, .96, .90))),                                           # plume
    'dark': ('flat', dict(base=(.14, .10, .14))),
    'emblem': ('folds', dict(base=(.10, .56, .56), period=28, hem=(.98, .84, .48), hem2=(.90, .64, .26))),   # teal cape and hose
    'emblem2': ('flat', dict(base=(.58, .30, .16))),                                          # auburn hair
}


# ------------------------------------------------------------------ ARTIFICER (heavy body in canvas, leather apron, goggles, backpack, gauntlet, hammer)
def gear_disc(B, c, n, u, r, t, teeth, region, wspec):
    """A flat toothed gear: a prism over a polygon of `teeth` teeth, facing n, centred at c."""
    outline = []
    for k in range(teeth * 2):
        a = math.pi * k / teeth
        rr = r if k % 2 == 0 else r * .78
        outline.append((rr * math.cos(a), rr * math.sin(a)))
    n = unit(n)
    u = unit(u)
    v = np.cross(n, u)
    prism(B, outline, c, u, v, n, t / 2, region, region, wspec, taper=.95)


def tube(B, path, r, region, wspec, sides=6):
    loft(B, path, [r] * len(path), [r] * len(path), sd(sides), region, wspec, caps=(True, True), ref=(0, 0, 1))


def build_artificer(rig, M0):
    B = Body()
    # body: the heavy body's fit in a soot-grey canvas coat, copper-banded, no plates (cloth takes the plate slot)
    R1.heavy_body(B, rig, M0, plate='cloth', trim='trim', leather='leather', fist='leather')
    cw = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.0, .10), ('spine2', 1.18, .08), ('chest', 1.32, .06)])
    sk = skirt_w(1.06, .8, .6, .06)
    # leather apron over the front: a bib to the knee on shoulder straps, with tool loops, rivets and a brass buckle
    zt, zb = 1.40, .58
    hw = .135
    panel(B, (.178, -hw, zt), (.178, hw, zt), (.21, hw + .035, zb), (.21, -hw - .035, zb), 2, 3 if detail(0) else 2, 'leather', sk, vrange=(1, 0))
    panel(B, (.168, hw, 1.38), (.168, -hw, 1.38), (.205, -hw - .035, .62), (.205, hw + .035, .62), 1, 2, 'dark', sk)
    for sg in (1, -1):                                                                                   # shoulder straps
        box(B, (.04, sg * .13, 1.50), (.30, .035, .018), 'leather', 'chest', rot=rotx(0))
    if detail(1):
        for sg in (1, -1):
            box(B, (.184, sg * .095, 1.33), (.014, .035, .035), 'trim', 'chest')                         # strap rivets
        for k, y in enumerate((-.09, 0.0, .09)):                                                        # chest pocket tools: wrench, pliers, driver
            box(B, (.20, y, 1.18), (.022, .035, .17), 'leather', 'chest')
        box(B, (.215, -.09, 1.27), (.014, .028, .10), 'blade', 'chest')                                  # wrench shaft
        box(B, (.215, -.09, 1.325), (.014, .05, .03), 'blade', 'chest')
        box(B, (.215, .0, 1.27), (.014, .02, .13), 'plate', 'chest', rot=rotx(8))                       # brass pliers
        box(B, (.215, .09, 1.28), (.012, .016, .13), 'dark', 'chest')                                    # screwdriver
        box(B, (.215, .09, 1.35), (.016, .026, .04), 'trim', 'chest')
        box(B, (.208, 0, 0.98), (.026, .08, .08), 'plate', 'pelvis')                                     # apron buckle
    # tool belt pouches and a hanging wrench at the right hip
    if detail(0):
        for sg in (1, -1):
            box(B, (.02, sg * .21, .94), (.09, .05, .10), 'leather', 'pelvis')
            box(B, (.02, sg * .24, .95), (.05, .012, .04), 'trim', 'pelvis')
        loft(B, [(.02, -.225, .90), (.02, -.235, .68)], [.012, .012], [.012, .012], 5, 'blade', 'pelvis', ref=(1, 0, 0))
        box(B, (.02, -.235, .64), (.03, .07, .05), 'blade', 'pelvis')
    # backpack: a brass boiler with copper bands, two pipes over the shoulders, a valve and a gear
    bc = np.array((-.235, 0.0, 1.17))
    loft(B, [bc + (0, 0, -.30), bc + (0, 0, -.28), bc + (0, 0, .24), bc + (0, 0, .27)], [.055, .115, .115, .06], [.05, .095, .095, .05], sd(12), 'plate', 'chest',
         ref=(1, 0, 0), dense=1)
    for z in (-.18, -.02, .15):
        loft(B, [bc + (0, 0, z), bc + (0, 0, z + .03)], [.119, .119], [.099, .099], sd(12), 'trim', 'chest', caps=(False, False), ref=(1, 0, 0))
    for sg in (1, -1):
        tube(B, [bc + (.0, sg * .07, .26), bc + (.0, sg * .09, .36), bc + (.02, sg * .20, .385), bc + (.07, sg * .24, .33)], .020, 'emblem', 'chest', 6)
    if detail(0):
        tube(B, [bc + (-.06, 0, .27), bc + (-.06, 0, .40)], .026, 'dark', 'chest', 6)                    # short chimney
        loft(B, [bc + (-.06, 0, .395), bc + (-.06, 0, .43)], [.034, .04], [.034, .04], sd(6), 'trim', 'chest', ref=(0, 0, 1))
        gear_disc(B, bc + (-.115, 0, -.02), (-1, 0, 0), (0, 1, 0), .08, .02, 8, 'trim', 'chest')
        gear_disc(B, bc + (-.115, .095, .12), (-1, 0, 0), (0, 1, 0), .05, .02, 6, 'plate', 'chest')
        loft(B, [bc + (-.02, -.12, -.04), bc + (-.05, -.15, .02), bc + (-.02, -.16, .08)], [.016] * 3, [.016] * 3, 5, 'emblem', 'chest', ref=(0, 0, 1))     # coiled copper line
    # bandolier of tools and vials across the chest, from the left shoulder to the right hip
    if detail(0):
        p_sh, p_hip = np.array((.0, .20, 1.43)), np.array((.0, -.18, .96))
        dd = p_hip - p_sh
        box(B, (p_sh + p_hip) / 2 + np.array((.158, 0, 0)), (.020, .045, np.linalg.norm(dd)), 'leather', 'chest', rot=frame_matrix(dd, (1, 0, 0), 2))
        for t in (.25, .5, .75):
            c = p_sh + dd * t + np.array((.178, 0, 0))
            box(B, c, (.03, .05, .06), 'dark', 'chest')
            box(B, c + np.array((.016, 0, .0)), (.004, .03, .04), 'accent', 'chest')
    # head: bare, short copper hair, a brow strap with goggles pushed up (brass rims, pale lenses), a soot-grey neck scarf
    wn = blend_chain(lambda p: p[2], [('chest', 0, 0), ('neck', 1.48, .03)])
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, 1.42), (0, 0, 1.51)], [.118, .098], [.104, .09], sd(10), 'cloth', wn)
    loft(B, [(0, 0, 1.46), (0, 0, 1.52)], [.07, .06], [.07, .06], sd(8), 'skin', wh)
    loft(B, [(0, 0, 1.52), (0, 0, 1.60), (0, 0, 1.67), (0, 0, 1.73)], [.084, .098, .096, .062], [.092, .106, .10, .062], sd(14), 'skin', wh, dense=dn(2))
    for sg in (1, -1):
        box(B, (.104, sg * .045, 1.625), (.012, .045, .026), 'dark', 'head')
        if detail(1):
            box(B, (.103, sg * .045, 1.652), (.012, .05, .008), 'emblem2', 'head', rot=rotx(sg * -8))
    box(B, (.112, 0, 1.585), (.022, .02, .05), 'skin', 'head')
    if detail(1):
        box(B, (.108, 0, 1.54), (.012, .09, .012), 'emblem2', 'head')                                          # stubble moustache
    loft(B, [(-.005, 0, 1.655), (-.005, 0, 1.71), (-.02, 0, 1.765)], [.108, .106, .06], [.114, .11, .064], sd(12), 'emblem2', 'head', caps=(False, True), dense=dn(2))   # short hair
    if detail(1):
        loft(B, [(-.06, 0, 1.69), (-.12, 0, 1.64), (-.15, 0, 1.58)], [.06, .05, .0], [.03, .028, .0], sd(6), 'emblem2', 'head', ref=(0, 1, 0))                       # hair tail
    loft(B, [(-.005, 0, 1.676), (-.005, 0, 1.70)], [.114, .114], [.12, .12], sd(14), 'leather', 'head', caps=(False, False))                                    # brow strap
    for sg in (1, -1):
        c = np.array((.10, sg * .052, 1.738))
        loft(B, [c, c + np.array((.040, 0, 0))], [.040, .038], [.040, .038], sd(10), 'plate', 'head', caps=(True, True), ref=(0, 0, 1))                          # brass lens rim
        loft(B, [c + np.array((.040, 0, 0)), c + np.array((.046, 0, 0))], [.030, .030], [.030, .030], sd(10), 'accent', 'head', caps=(True, True), ref=(0, 0, 1))  # lens
        if detail(1):
            loft(B, [c + np.array((.005, 0, 0)), c + np.array((.012, 0, 0))], [.045, .045], [.045, .045], sd(10), 'trim', 'head', caps=(False, False), ref=(0, 0, 1))
    box(B, (.108, 0, 1.738), (.02, .035, .02), 'trim', 'head')                                                                                                  # bridge
    # arms: brass vambraces on both forearms; on the left a mechanical gauntlet with a glowing rune-plate and a crystal in a brass frame
    for sg, s in SIDES:
        sh, el, wr, d = arm_frame(s)
        loft(B, [el + d * .03, el + d * .10, wr - d * .01], [.072, .076, .066], [.072, .076, .066], sd(8), 'plate', f'forearm.{s}', caps=(True, True))
        loft(B, [el + d * .035, el + d * .055], [.077, .077], [.077, .077], sd(8), 'trim', f'forearm.{s}', caps=(False, False))
        loft(B, [wr - d * .03, wr - d * .01], [.07, .07], [.07, .07], sd(8), 'trim', f'forearm.{s}', caps=(False, False))
    sh, el, wr, d = arm_frame('L')
    out = np.array((0.0, 1.0, 0.0))
    # a larger gauntlet over the left hand: brass segments, copper finger caps, a gear
    hc = wr + d * .065
    box(B, hc, (.10, .10, .13), 'plate', 'hand.L', rot=frame_matrix(d, (1, 0, 0), 2))
    if detail(1):
        for k in range(3):
            box(B, hc + d * (.075 + .005 * k) + np.array((.03 * (k - 1), 0, 0)), (.026, .02, .05), 'trim', 'hand.L', rot=frame_matrix(d, (1, 0, 0), 2))
    # a brass mounting plate on the outer face of the forearm carrying a tall cyan crystal in a brass cage (the single glow), a small rune-plate near the wrist
    fr = frame_matrix(d, (1, 0, 0), 2)
    pc = el + d * .115 + out * .088
    box(B, pc, (.10, .014, .24), 'trim', 'forearm.L', rot=fr)
    c2 = pc + out * .06
    cry = [c2 - d * .13, c2 - d * .07, c2 + d * .05, c2 + d * .15]
    loft(B, cry, [0, .052, .046, 0], [0, .052, .046, 0], 6 if not K.LOD else 4, 'glow', 'forearm.L', caps=(False, False), ref=(1, 0, 0))
    for off in (-.10, .10):                                                                      # cage collars
        loft(B, [c2 + d * off - d * .008, c2 + d * off + d * .008], [.058, .058], [.058, .058], sd(8), 'plate', 'forearm.L', caps=(False, False), ref=(1, 0, 0))
    if detail(0):
        for sgn in (-1, 1):                                                                      # cage bars
            loft(B, [c2 + d * -.10 + np.array((.05 * sgn, 0, 0)), c2 + d * .10 + np.array((.05 * sgn, 0, 0))], [.007, .007], [.007, .007], 4, 'plate', 'forearm.L', ref=(0, 1, 0))
    box(B, el + d * .225 + out * .082, (.05, .01, .06), 'glow', 'forearm.L', rot=fr)
    if detail(0):
        gear_disc(B, el + d * .035 + out * .09, (0, 1, 0), (1, 0, 0), .035, .012, 7, 'plate', 'forearm.L')
    # light hammer in the right hand: a leather-wrapped haft, a steel head with a copper-banded brass peen, a cyan gem pommel
    g = hand_grip(rig, M0, 'R')
    ax = unit((math.cos(deg(55)), 0, math.sin(deg(55))))
    B.push(rig.to_bind(M0, 'weapon_socket'))
    ws = 'weapon_socket'
    loft(B, [g - ax * .12, g + ax * .26], [.019, .021], [.019, .021], sd(6), 'leather', ws, ref=(0, 0, 1))
    loft(B, [g - ax * .14, g - ax * .12], [.0, .03], [.0, .03], sd(6), 'trim', ws, ref=(0, 0, 1))
    top = g + ax * .26
    side = unit(np.cross(ax, (0, 1, 0)))
    box(B, top + ax * .045, (.09, .06, .10), 'blade', ws, rot=frame_matrix(ax, (0, 1, 0), 2))                # head block
    box(B, top + ax * .045 + side * .0, (.098, .068, .028), 'trim', ws, rot=frame_matrix(ax, (0, 1, 0), 2))   # copper band
    spike(B, top + ax * .045 - np.array((0, 0, 0)) + (side * -.04) * 0 + np.array((.0, .04, 0)), (0, 1, 0), .09, .028, 'plate', ws, sides=5)    # brass peen
    spike(B, top + ax * .045 + np.array((.0, -.04, 0)), (0, -1, 0), .05, .028, 'blade', ws, sides=5)           # striking face
    box(B, g - ax * .155, (.03, .03, .03), 'glow', ws)
    B.pop()
    return B, ARTIFICER_ATLAS


ARTIFICER_ATLAS = {
    'plate': ('bands', dict(base=(.92, .68, .28), line=(.62, .42, .14), spacing=48)),         # brass
    'trim': ('scales', dict(a=(.86, .46, .26), b=(.66, .32, .16))),                           # copper
    'cloth': ('folds', dict(base=(.50, .52, .58), period=30, hem=(.74, .74, .78), hem2=(.40, .41, .46))),   # soot-grey canvas
    'leather': ('weave', dict(base=(.66, .44, .26))),                                          # apron tan
    'blade': ('blade', dict(base=(.82, .86, .92))),
    'glow': ('flat', dict(base=(.45, 1.0, 1.0))),                                             # the single cyan glow
    'rune': ('rune', dict(bg=(.04, .18, .22), fg=(.45, 1.0, 1.0))),
    'skin': ('flat', dict(base=(.80, .62, .50))),
    'bone': ('flat', dict(base=(.94, .90, .80))),
    'accent': ('flat', dict(base=(.76, .90, .94))),                                           # goggle lenses, vial glass
    'dark': ('flat', dict(base=(.17, .16, .19))),                                             # soot black
    'emblem': ('folds', dict(base=(.84, .42, .22), period=26, hem=(.92, .62, .30), hem2=(.64, .30, .14))),   # copper pipe and coil
    'emblem2': ('flat', dict(base=(.60, .32, .18))),                                          # copper-red hair
}


CHARACTERS = {   # key: (builder, name, atlas seed)
    'bard': (build_bard, 'SK_Hero_Bard', 1483),
    'artificer': (build_artificer, 'SK_Hero_Artificer', 1484),
}
ASSET_IDS = {'bard': 'sk-hero-bard', 'artificer': 'sk-hero-artificer'}
BATCH4 = tuple(CHARACTERS)


def make_atlas(key, spec, seed):
    return B3.make_atlas(key, spec, seed)


C.CHARACTERS.update(CHARACTERS)
C.ASSET_IDS.update(ASSET_IDS)
