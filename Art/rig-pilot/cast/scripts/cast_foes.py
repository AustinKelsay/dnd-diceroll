"""Character recipes for the #150 foe batch: scout, archer, skirmisher, brute and warden.

Same skeleton, atlas layout, LOD helpers and conventions as batches 0 to 4 (Art/rig-pilot/r001/README.md, cast/README*.md): the 25-bone armature,
one material on its own 1024x1024 atlas of 13 tiles, RGBA vertex colour with the glow mask in alpha (runes, gems and the Vault mask only), at most
4 influences, the ten shared clips. The foes share one hostile palette family, the raider's: tan hide, red cloth and bone-white, with ember glow;
the warden alone is cold stone and verdigris with the Vault's cyan glow (0.5, 0.85, 1.0, the Waystone's, #197). This module registers its
classes in cast_characters.CHARACTERS / ASSET_IDS in memory and imports cast_batch2 (Brace and the batch 2 helpers), so no earlier batch file is
edited and their asset records keep their hashes.

Each recipe is build_<name>(rig, M0) -> (Body, atlas_spec); M0 is the posed FK of Idle frame 0.
"""
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import cast_characters as C                                                     # noqa: E402  (puts r001/scripts on sys.path)
import cast_batch2 as B2                                                        # noqa: E402  (registers batch 2 and the Brace clip)
import clips as CL                                                              # noqa: E402,F401
import overrides_foes as OF                                                     # noqa: E402
import rigkit as K                                                              # noqa: E402
from rigkit import Body, JOINTS, blend_chain, box, deg, detail, dn, frame_matrix, loft, panel, prism, quad, sd, spike   # noqa: E402

R1 = C.R1
SIDES = C.SIDES
rotx, roty, rotz = C.rotx, C.roty, C.rotz
skirt_w, hand_grip, rune_faces = C.skirt_w, C.hand_grip, C.rune_faces
unit, arc_loft, cloth_sheet, dagger, agile_body = C.unit, C.arc_loft, C.cloth_sheet, C.dagger, C.agile_body
arm_frame = B2.arm_frame

CW = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.0, .10), ('spine2', 1.18, .08), ('chest', 1.32, .06)])   # torso weights for capes


def _tile(A, region, t):
    i = K.REGIONS.index(region)
    c, r = i % 4, i // 4
    A[r * K.TILE:(r + 1) * K.TILE, c * K.TILE:(c + 1) * K.TILE] = t


def feather(B, base, direction, length, w, region, wspec, side=(0, 1, 0)):
    """A flat feather: a flattened diamond, wide across `side x direction`. Rigid on one bone."""
    base = np.array(base, float)
    d = unit(direction)
    loft(B, [base, base + d * length * .5, base + d * length], [w * .5, w, 0.0], [.004, .005, 0.0], 4, region, wspec, caps=(True, False), ref=tuple(side))


def bone_charm(B, p, wspec, size=.03, tilt=0.0):
    """A small bone fetish: a short shaft with a knob at each end, hung at p."""
    p = np.array(p, float)
    box(B, p, (size * .55, size * .55, size * 1.6), 'bone', wspec, rot=rotx(tilt))
    if detail(1):
        for sg in (1, -1):
            box(B, p + np.array((0, 0, sg * size * .8)), (size * .8, size * .8, size * .45), 'bone', wspec, rot=rotx(tilt))


def cord(B, path, wspec, r=.006, region='leather'):
    loft(B, path, [r] * len(path), [r] * len(path), 4, region, wspec, caps=(True, True), ref=(1, 0, 0))


def hide_hood(B, wh, path, ra, rb, outer='plate', lining='cloth', a0=62, a1=298):
    """A hide cowl lined in red cloth around an open face."""
    arc_loft(B, path, ra, rb, sd(12), outer, wh, a0=a0, a1=a1, vrange=(1, 0))
    arc_loft(B, path, [x - .012 for x in ra], [x - .012 for x in rb], sd(12), lining, wh, a0=a0, a1=a1, flip=True)


def small_head(B, wh, eye='dark'):
    """The agile head: a neck and a skull, eyes in the shade, a brow line. Returns nothing; the cowl or mask is added by the recipe."""
    loft(B, [(0, 0, 1.46), (0, 0, 1.52)], [.068, .06], [.068, .06], sd(8), 'skin', wh)
    loft(B, [(0, 0, 1.50), (0, 0, 1.58), (0, 0, 1.68)], [.075, .085, .09], [.08, .092, .095], sd(10), 'skin', wh)
    for sg in (1, -1):
        box(B, (.093, sg * .04, 1.625), (.012, .045, .02), eye, 'head')
        box(B, (.095, sg * .04, 1.645), (.010, .05, .008), 'dark', 'head')


# ------------------------------------------------------------------ SCOUT (agile body, hide cowl, short spear)
def build_scout(rig, M0):
    B = Body()
    agile_body(B, rig, M0, jerkin='plate', sleeve='leather', pants='leather', boot='leather', cuff='bone', glove='leather', belt='leather', buckle='bone')
    sk = skirt_w(1.06, .8, .6, .06)
    # red sash, a loincloth front and back, a cross strap with bone charms
    loft(B, [(0, 0, 1.015), (0, 0, 1.10)], [.172, .172], [.126, .126], sd(12), 'cloth', 'pelvis')
    panel(B, (.13, -.09, 1.06), (.13, .09, 1.06), (.17, .10, .60), (.17, -.10, .60), 2, 3 if detail(0) else 2, 'cloth', sk)
    panel(B, (-.12, .09, 1.06), (-.12, -.09, 1.06), (-.16, -.10, .66), (-.16, .10, .66), 2, 2, 'cloth', sk)
    if detail(1):
        box(B, (.134, 0, 1.27), (.024, .06, .52), 'cloth', 'chest', rot=rotx(-30))
        for k, t in enumerate((.25, .55, .85)):                                                              # bone charms hanging from the strap
            p = np.array((.145, -.12 + t * .24, 1.45 - t * .40))
            cord(B, [p + (0, 0, .0), p + (0, 0, -.06)], 'chest', r=.004)
            bone_charm(B, p + (0, 0, -.085), 'chest', size=.034, tilt=8 * (k - 1))
        for sg in (1, -1):
            box(B, (.02, sg * .185, .92), (.07, .05, .085), 'leather', 'pelvis')                           # hip pouches
            box(B, (.02, sg * .216, .93), (.04, .012, .035), 'bone', 'pelvis')
    # head, a red scarf over the lower face, bone brow line, a hide cowl lined in red with a bone-pale rim
    wn = blend_chain(lambda p: p[2], [('chest', 0, 0), ('neck', 1.48, .03)])
    loft(B, [(0, 0, 1.42), (0, 0, 1.51)], [.115, .095], [.10, .088], sd(10), 'cloth', wn)
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    small_head(B, wh)
    box(B, (.097, 0, 1.55), (.02, .13, .075), 'cloth', 'head')
    if detail(1):
        for sg in (1, -1):
            box(B, (.103, sg * .045, 1.668), (.010, .05, .01), 'bone', 'head', rot=rotx(sg * -12))        # raised brow marks
    hood = [(-.015, 0, 1.50), (-.015, 0, 1.58), (-.02, 0, 1.665), (-.045, 0, 1.735), (-.10, 0, 1.78), (-.18, 0, 1.755)]
    hra, hrb = [.120, .130, .128, .106, .07, .02], [.126, .138, .136, .114, .078, .03]
    hide_hood(B, wh, hood, hra, hrb)
    loft(B, [(-.01, 0, 1.485), (-.01, 0, 1.50)], [.145, .14], [.135, .13], sd(12), 'bone', wh, caps=(False, False))     # bone-pale hem
    if detail(1):
        for sg in (1, -1):                                                                                   # a bone charm hung at each side of the cowl
            cord(B, [(.02, sg * .125, 1.52), (.03, sg * .13, 1.47)], wh, r=.004)
            bone_charm(B, (.03, sg * .13, 1.445), wh, size=.03, tilt=sg * 10)
    # shoulder mantle of hide, bone-trimmed, and a short pelt down the back
    loft(B, [(0, 0, 1.50), (0, 0, 1.43), (0, 0, 1.37)], [.13, .25, .28], [.10, .17, .20], sd(14), 'plate', 'chest', caps=(True, False), vrange=(1, 0), dense=dn(2))
    loft(B, [(0, 0, 1.382), (0, 0, 1.362)], [.277, .281], [.197, .201], sd(14), 'bone', 'chest', caps=(False, False))
    nx, nz_ = (4, 4) if detail(0) else (3, 3)
    cloth_sheet(B, 1.38, .66, .20, .30, .17, .24, nx, nz_, 'plate', 'cloth', CW, skirt_w(.98, .85, .5, .09), wob=.02, jag=.04 if detail(0) else 0.0)
    for sg, s_ in SIDES:                                                                                       # hide spaulders, one bone spike on the left
        pc = np.array((0, sg * .245, 1.42))
        loft(B, [pc + (0, 0, z) for z in (-.04, .0, .035, .06)], [.10, .115, .09, .03], [.09, .104, .082, .028], sd(10), 'plate', f'clavicle.{s_}', dense=dn(2))
        loft(B, [pc + (0, 0, -.04), pc + (0, 0, -.05)], [.10, .097], [.09, .087], sd(10), 'bone', f'clavicle.{s_}')
    spike(B, (0, .27, 1.48), (-.1, .5, .8), .15, .022, 'bone', 'clavicle.L', sides=4)
    # short spear in the right hand: a hide-wrapped shaft, a leaf blade, a bone ring, red tassels, an ember pommel gem
    g = hand_grip(rig, M0, 'R')
    ax = unit((math.cos(deg(68)), 0, math.sin(deg(68))))
    fw = unit(np.cross((0, 1, 0), ax))
    B.push(rig.to_bind(M0, 'weapon_socket'))
    ws = 'weapon_socket'
    loft(B, [g - ax * .34, g + ax * .30, g + ax * .88], [.019, .019, .021], [.019, .019, .021], sd(6), 'leather', ws, ref=(0, 1, 0), dense=dn(2))
    box(B, g - ax * .36, (.03, .03, .03), 'glow', ws)
    for off in ((.02, .42, .70) if detail(1) else (.42,)):
        loft(B, [g + ax * off, g + ax * (off + .045)], [.027, .027], [.027, .027], sd(6), 'bone', ws, caps=(False, False), ref=(0, 1, 0))
    top = g + ax * .90
    blade = [(0.0, -.01), (.040, .04), (.058, .12), (.030, .21), (0.0, .27), (-.030, .21), (-.058, .12), (-.040, .04)]
    prism(B, blade, top, (0, 1, 0), ax, fw, .008, 'blade', 'blade', ws, taper=.6)
    box(B, top, (.05, .05, .04), 'dark', ws, rot=np.column_stack((fw, (0, 1, 0), ax)))
    for sg in (1, -1):                                                                                         # red tassel streamers under the blade
        loft(B, [top - ax * .02, top - ax * .09 + np.array((0, sg * .03, 0)), top - ax * .17 + np.array((0, sg * .05, -.01))], [.014, .012, .0], [.004, .004, .0], 4,
             'cloth', ws, caps=(True, False), ref=(1, 0, 0))
    if detail(0):
        rune_faces(B, top + ax * .10, (0, 1, 0), ax, ws, .02, .045, .0095)
    B.pop()
    return B, SCOUT_ATLAS


# ------------------------------------------------------------------ ARCHER (agile body, hide hood, bone mask, shortbow)
_HOLD = {}


def archer_hold(rig):
    if id(rig) not in _HOLD:
        _HOLD[id(rig)] = OF.archer_hold_pose(rig)
    return _HOLD[id(rig)]


def shortbow(B, rig, M0, pose):
    """A shortbow in the left hand, authored where the left hand is in `pose` (the archer's Ranged hold, frame 23 of Ranged_Archer) so it is upright
    while drawn: a curved dark-wood stave with bone tips and a red grip wrap, rigid on shield_socket; the bone-pale string is two segments from the limb tips
    to a nock vertex skinned to hand.R, authored where the right hand is at full draw, so the clip's right hand draws it back (and in every other clip the
    string trails to the hand)."""
    M = rig.fk(*pose)
    h, t = rig.head(M, 'hand.L'), rig.tail(M, 'hand.L')
    cg = h + (t - h) * .6
    fa = rig.tail(M, 'forearm.L') - rig.head(M, 'forearm.L')
    aim = unit((fa[0], fa[1], 0.0))
    B.push(rig.to_bind(M, 'shield_socket'))
    ws = 'shield_socket'
    half = .46
    zs = np.linspace(-half, half, 9 if detail(0) else 7 if detail(1) else 5)
    pts = [cg + (0, 0, z) - aim * (.14 * (z / half) ** 2) for z in zs]
    rad = [.011 + .010 * (1 - abs(z / half)) ** 1.5 for z in zs]
    loft(B, pts, rad, rad, sd(6), 'dark', ws, caps=(True, True), ref=(0, 0, 1), dense=1)
    loft(B, [cg + (0, 0, -.08), cg + (0, 0, .08)], [.019, .019], [.019, .019], sd(6), 'cloth', ws, ref=(0, 0, 1))                         # red grip wrap
    for sg in (1, -1):
        tip = pts[-1] if sg > 0 else pts[0]
        loft(B, [tip, tip + np.array((0, 0, .035 * sg))], [.015, .0], [.015, .0], 4, 'bone', ws, caps=(True, False), ref=(1, 0, 0))
    box(B, cg + aim * .03 + (0, 0, .10), (.016, .016, .016), 'glow', ws)
    B.pop()
    T_bow, T_hand = rig.to_bind(M, 'shield_socket'), rig.to_bind(M, 'hand.R')
    h, t = rig.head(M, 'hand.R'), rig.tail(M, 'hand.R')
    nock_world = h + (t - h) * .6
    a = (T_bow @ np.append(pts[0], 1.0))[:3]
    b = (T_bow @ np.append(pts[-1], 1.0))[:3]
    n = (T_hand @ np.append(nock_world, 1.0))[:3]
    loft(B, [a, n, b], [.0035] * 3, [.0035] * 3, 4, 'bone', lambda p: {'hand.R': 1.0} if np.linalg.norm(np.asarray(p, float) - n) < .02 else {'shield_socket': 1.0},
         ref=(1, 0, 0), dense=1)


def bone_mask(B, wh):
    """A bone-white half mask over the face: a rounded plate with two dark eye slits, a nose notch and a row of teeth, a red stripe down the brow."""
    outline = [(-.062, -.075), (.062, -.075), (.082, -.02), (.080, .05), (.055, .10), (-.055, .10), (-.080, .05), (-.082, -.02)]
    # drawn in the (y, z) plane facing +X: u = +Y, v = +Z, normal +X, centred on the face
    prism(B, outline, (.099, 0, 1.585), (0, 1, 0), (0, 0, 1), (1, 0, 0), .009, 'bone', 'bone', wh, taper=.9)
    for sg in (1, -1):
        box(B, (.111, sg * .036, 1.617), (.008, .042, .018), 'dark', 'head', rot=rotx(sg * -12))
    box(B, (.111, 0, 1.585), (.008, .014, .03), 'dark', 'head')
    if detail(1):
        for k in (-2, -1, 0, 1, 2):
            box(B, (.111, k * .016, 1.528), (.008, .008, .026), 'dark', 'head')
        box(B, (.1125, 0, 1.66), (.006, .018, .05), 'cloth', 'head')


def build_archer(rig, M0):
    B = Body()
    agile_body(B, rig, M0, jerkin='plate', sleeve='leather', pants='leather', boot='leather', cuff='bone', glove='leather', belt='leather', buckle='bone')
    sk = skirt_w(1.06, .8, .6, .06)
    loft(B, [(0, 0, 1.015), (0, 0, 1.10)], [.172, .172], [.126, .126], sd(12), 'cloth', 'pelvis')                    # red sash with a long tail at the right hip
    panel(B, (.11, -.07, 1.07), (.15, -.17, 1.07), (.19, -.19, .52), (.14, -.09, .52), 2, 3 if detail(0) else 2, 'cloth', sk)
    if detail(1):
        box(B, (.128, 0, 1.28), (.02, .05, .30), 'bone', 'chest')                                                    # lacing
        box(B, (.134, -.02, 1.24), (.022, .05, .48), 'leather', 'chest', rot=rotx(28))                              # quiver strap
        box(B, (.143, -.02, 1.24), (.016, .045, .05), 'bone', 'chest')
        for sg in (1, -1):
            box(B, (.02, sg * .185, .92), (.07, .05, .085), 'leather', 'pelvis')
            box(B, (.02, sg * .216, .93), (.04, .012, .035), 'bone', 'pelvis')
        for k, t in enumerate((.2, .5)):                                                                             # bone charms on the strap
            p = np.array((.15, -.04 + t * .1, 1.35 - t * .32))
            cord(B, [p, p + (0, 0, -.05)], 'chest', r=.004)
            bone_charm(B, p + (0, 0, -.07), 'chest', size=.03, tilt=6 * (k - .5))
    wn = blend_chain(lambda p: p[2], [('chest', 0, 0), ('neck', 1.48, .03)])
    loft(B, [(0, 0, 1.42), (0, 0, 1.51)], [.115, .095], [.10, .088], sd(10), 'cloth', wn)
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, 1.46), (0, 0, 1.52)], [.068, .06], [.068, .06], sd(8), 'skin', wh)
    loft(B, [(0, 0, 1.50), (0, 0, 1.58), (0, 0, 1.68)], [.075, .085, .09], [.08, .092, .095], sd(10), 'skin', wh)
    bone_mask(B, wh)
    hood = [(-.015, 0, 1.50), (-.015, 0, 1.58), (-.03, 0, 1.67), (-.07, 0, 1.745), (-.15, 0, 1.76), (-.235, 0, 1.70)]
    hra, hrb = [.120, .128, .128, .110, .075, .02], [.126, .136, .136, .118, .084, .03]
    hide_hood(B, wh, hood, hra, hrb)
    loft(B, [(-.01, 0, 1.485), (-.01, 0, 1.50)], [.145, .14], [.135, .13], sd(12), 'bone', wh, caps=(False, False))
    if detail(1):                                                                                                    # two bone fetish feathers on the hood
        feather(B, (-.02, .12, 1.70), (-.4, .5, .6), .16, .028, 'accent', wh)
        feather(B, (-.04, .11, 1.68), (-.5, .6, .35), .13, .024, 'cloth', wh)
    loft(B, [(0, 0, 1.50), (0, 0, 1.43), (0, 0, 1.37)], [.13, .25, .28], [.10, .17, .20], sd(14), 'plate', 'chest', caps=(True, False), vrange=(1, 0), dense=dn(2))
    loft(B, [(0, 0, 1.382), (0, 0, 1.362)], [.277, .281], [.197, .201], sd(14), 'bone', 'chest', caps=(False, False))
    nx, nz_ = (4, 4) if detail(0) else (3, 3)
    cloth_sheet(B, 1.38, .62, .20, .30, .17, .24, nx, nz_, 'plate', 'cloth', CW, skirt_w(.98, .85, .5, .09), wob=.02, jag=.03 if detail(0) else 0.0)
    for sg, s_ in SIDES:
        pc = np.array((0, sg * .245, 1.42))
        loft(B, [pc + (0, 0, z) for z in (-.04, .0, .035, .06)], [.10, .115, .09, .03], [.09, .104, .082, .028], sd(10), 'plate', f'clavicle.{s_}', dense=dn(2))
        loft(B, [pc + (0, 0, -.04), pc + (0, 0, -.05)], [.10, .097], [.09, .087], sd(10), 'bone', f'clavicle.{s_}')
    # quiver on the back with red-fletched bone arrows
    if detail(1):
        qa = unit((.20, -.30, 1.0))
        q0 = np.array((-.17, .10, 1.00))
        loft(B, [q0, q0 + qa * .50], [.052, .056], [.052, .056], sd(8), 'leather', 'chest', ref=(1, 0, 0), dense=dn(2))
        loft(B, [q0 + qa * .44, q0 + qa * .48], [.060, .060], [.060, .060], sd(8), 'bone', 'chest', caps=(False, False), ref=(1, 0, 0))
        for k, (dy, dx) in enumerate(((-.02, .0), (.025, -.01), (.0, .03), (.03, .02))[:4 if detail(0) else 2]):
            top = q0 + qa * .49 + np.array((dx, dy, 0))
            loft(B, [top, top + qa * .18], [.006, .006], [.006, .006], 4, 'bone', 'chest', ref=(1, 0, 0))
            for sgn in (1, -1):
                quad(B, top + qa * .15, np.array((0, .02, 0)), qa * .03 * sgn, 'cloth', 'chest')
    # hunting knife in the right hand (hidden during the draw), the shortbow in the left, authored at the Ranged hold
    g = hand_grip(rig, M0, 'R')
    B.push(rig.to_bind(M0, 'weapon_socket'))
    dagger(B, g, (math.cos(deg(48)), 0, math.sin(deg(48))), 'weapon_socket', blade=.28, rune=True)
    B.pop()
    shortbow(B, rig, M0, archer_hold(rig))
    return B, ARCHER_ATLAS


# ------------------------------------------------------------------ SKIRMISHER (agile body, bare painted skin, twin sickles, fetishes)
def sickle(B, g, ax, ws, fwd, hook=1.0, scale=1.0):
    """A hooked blade gripped at g along ax (authored where the hand is in Idle): a leather grip with a bone pommel, a small guard, a crescent blade
    that hooks towards `fwd` (a unit vector perpendicular to ax in the XZ plane), ember glow on the pommel and a rune at the base."""
    ax, fwd = unit(ax), unit(fwd) * hook
    side = unit(np.cross(ax, fwd))
    loft(B, [g - ax * .085, g + ax * .06], [.017, .017], [.017, .017], sd(6), 'leather', ws, ref=(0, 0, 1))
    box(B, g - ax * .10, (.03, .03, .03), 'bone', ws)
    box(B, g + ax * .068, (.022, .075, .026), 'dark', ws, rot=frame_matrix(ax, (0, 0, 1), 0))
    R, w, steps = .21 * scale, .055 * scale, (9 if detail(0) else 6 if detail(1) else 4)
    thmax = 1.95
    outer, inner = [], []
    for k in range(steps + 1):
        th = thmax * k / steps
        wk = w * (1 - k / steps) ** .8
        outer.append((R * math.sin(th), R - R * math.cos(th)))
        inner.append(((R - wk) * math.sin(th), R - (R - wk) * math.cos(th)))
    outline = outer + inner[::-1][1:-1]
    base = g + ax * .07
    prism(B, outline, base, ax, fwd, side, .006, 'blade', 'blade', ws, taper=.75)
    if detail(0):
        rune_faces(B, base + ax * .05 + fwd * .03, side, ax, ws, .028, .016, .0075)


def build_skirmisher(rig, M0):
    B = Body()
    agile_body(B, rig, M0, jerkin='emblem', sleeve='skin', pants='leather', boot='leather', cuff='bone', glove='skin', belt='cloth', buckle='bone')
    sk = skirt_w(1.06, .8, .6, .06)
    # light hide harness: a cross strap over bare painted skin, bone fetishes and feathers, red streamers from the belt
    if detail(1):
        box(B, (.132, .02, 1.26), (.022, .06, .52), 'leather', 'chest', rot=rotx(-30))
        box(B, (.132, -.02, 1.26), (.022, .06, .52), 'leather', 'chest', rot=rotx(30))
        box(B, (.146, 0, 1.26), (.014, .05, .05), 'bone', 'chest', rot=rotx(45))
        for k in range(5):                                                                                           # a necklace of small bones
            a = -.55 + k * .275
            bone_charm(B, (.095 * math.cos(a) + .06, .115 * math.sin(a), 1.425 - .02 * abs(math.sin(a * 2))), 'chest', size=.026, tilt=a * 40)
    for sg in (1, -1):                                                                                               # two long red streamers from the belt
        panel(B, (.06, sg * .10, 1.06), (.06, sg * .15, 1.06), (-.20 if sg > 0 else -.24, sg * .20, .56), (-.18, sg * .12, .56), 2, 3 if detail(0) else 2, 'cloth', sk)
    panel(B, (.13, -.07, 1.06), (.13, .07, 1.06), (.16, .075, .70), (.16, -.075, .70), 2, 2, 'cloth', sk)               # a short front flap
    # head: bare, dark topknot and a crest of bone-white and red feathers, a red war-paint stripe across the eyes, a red neck scarf
    wn = blend_chain(lambda p: p[2], [('chest', 0, 0), ('neck', 1.48, .03)])
    loft(B, [(0, 0, 1.43), (0, 0, 1.51)], [.105, .09], [.095, .084], sd(10), 'cloth', wn)
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, 1.46), (0, 0, 1.52)], [.066, .058], [.066, .058], sd(8), 'skin', wh)
    loft(B, [(0, 0, 1.50), (0, 0, 1.58), (0, 0, 1.67), (0, 0, 1.73)], [.078, .09, .088, .056], [.084, .098, .092, .056], sd(12), 'skin', wh, dense=dn(2))
    for sg in (1, -1):
        box(B, (.103, sg * .042, 1.628), (.012, .042, .02), 'dark', 'head')
        if detail(1):
            box(B, (.102, sg * .045, 1.652), (.012, .05, .009), 'dark', 'head', rot=rotx(sg * -14))
    box(B, (.108, 0, 1.585), (.02, .02, .048), 'skin', 'head')
    box(B, (.1065, 0, 1.628), (.006, .17, .028), 'cloth', 'head')                                                    # the war-paint stripe
    if detail(1):
        box(B, (.1055, 0, 1.55), (.006, .02, .06), 'bone', 'head')                                                  # a pale line down the chin
    loft(B, [(-.01, 0, 1.655), (-.01, 0, 1.70), (-.02, 0, 1.755)], [.102, .100, .056], [.108, .104, .058], sd(12), 'emblem2', 'head', caps=(False, True), dense=dn(2))
    loft(B, [(-.05, 0, 1.73), (-.09, 0, 1.78), (-.14, 0, 1.79)], [.042, .040, .0], [.042, .040, .0], sd(8), 'emblem2', 'head', caps=(True, False), ref=(0, 1, 0))   # topknot
    loft(B, [(0, 0, 1.667), (0, 0, 1.69)], [.108, .108], [.114, .114], sd(12), 'cloth', 'head', caps=(False, False))                              # red headband
    if detail(1):
        for k in range(5):                                                                                           # feather crest
            t = k / 4 - .5
            feather(B, (-.06, t * .06, 1.715), (-.45 + t * .1, t * 1.1, 1.0), .12 - .02 * abs(t), .03, 'accent' if k % 2 == 0 else 'cloth', 'head', side=(1, 0, 0))
        for sg in (1, -1):
            for ln in (0, 1):                                                                                        # two tails of the headband
                loft(B, [(-.10, sg * .06, 1.675), (-.2 - ln * .05, sg * (.08 + ln * .04), 1.62 - ln * .08), (-.30 - ln * .08, sg * (.11 + ln * .05), 1.48 - ln * .16)],
                     [.014, .014, .0], [.004, .004, .0], 4, 'cloth', 'head', caps=(True, False), ref=(0, 1, 0))
    # feathers and a bone fetish at each shoulder; a bone armlet
    for sg, s_ in SIDES:
        for k, (dz, ang) in enumerate(((.0, 30), (.04, 10), (.08, -10))):
            feather(B, (0, sg * .25, 1.46 + dz), (-.2, sg * .5, .8 + k * .1), .17 - .02 * k, .026, 'accent' if k != 1 else 'cloth', f'clavicle.{s_}', side=(1, 0, 0))
        sh, el, wr, d = arm_frame(s_)
        loft(B, [sh + d * .17, sh + d * .22], [.062, .062], [.062, .062], sd(8), 'bone', f'upperarm.{s_}', caps=(False, False))
    # twin sickles: right on weapon_socket, left on shield_socket
    g = hand_grip(rig, M0, 'R')
    ax = unit((math.cos(deg(40)), 0, math.sin(deg(40))))
    B.push(rig.to_bind(M0, 'weapon_socket'))
    sickle(B, g, ax, 'weapon_socket', fwd=(math.sin(deg(40)), 0, -math.cos(deg(40))), hook=-1.0)
    B.pop()
    g = hand_grip(rig, M0, 'L')
    ax = unit((math.cos(deg(30)), -.18, math.sin(deg(30))))
    B.push(rig.to_bind(M0, 'shield_socket'))
    fwd = unit(np.cross((0, 0, 1), ax))
    sickle(B, g, ax, 'shield_socket', fwd=np.cross(ax, fwd), hook=1.0)
    B.pop()
    return B, SKIRMISHER_ATLAS


# ------------------------------------------------------------------ BRUTE (heavy body, bulked, spiked hide pauldron, horned helm, stone maul)
def build_brute(rig, M0):
    B = Body()
    B2.barbarian_body(B, rig, M0, g=1.10)
    # one huge spiked hide pauldron on the left shoulder, a smaller one with two spikes on the right
    for sg, s_, big in ((1, 'L', 1.0), (-1, 'R', .62)):
        pc = np.array((0, sg * (.27 if big == 1.0 else .255), 1.43))
        pz = [-.06, 0.0, .06, .11, .15]
        pr = [x * (1.0 if big == 1.0 else .68) for x in (.20, .225, .19, .12, .05)]
        loft(B, [pc + (0, 0, z * big) for z in pz], pr, [x * .92 for x in pr], sd(12), 'plate', f'clavicle.{s_}', dense=dn(2))
        loft(B, [pc + (0, 0, -.06 * big), pc + (0, 0, -.085 * big)], [pr[0], pr[0] * .95], [pr[0] * .92, pr[0] * .88], sd(12), 'cloth', f'clavicle.{s_}')
        spec = ((10, .5, .36), (-35, .5, .32), (62, .3, .27), (-80, .3, .25), (-5, 1.1, .30)) if big == 1.0 else ((20, .6, .26), (-40, .6, .22))
        for ang, up, ln in spec:
            if not detail(1) and big == 1.0 and ang in (62, -80):
                continue
            a = math.radians(ang)
            spike(B, pc + (0, 0, .06 * big) + np.array((math.sin(a) * -.05, sg * .08 * math.cos(a), 0)), (math.sin(a) * -.5, sg * math.cos(a) * .7, up + .5), ln, .036 * big ** .3,
                  'bone', f'clavicle.{s_}', sides=5)
    # raider-family helm: a dark hide bucket with a bone brow ridge and nasal bar, ember eye slit, two tusks, and a pair of tall bull horns
    hz = [1.50, 1.56, 1.63, 1.69, 1.73]
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, 1.46), (0, 0, 1.52)], [.078, .068], [.078, .068], sd(8), 'skin', wh)
    loft(B, [(0, 0, z) for z in hz], [.103, .12, .12, .098, .055], [.114, .13, .13, .104, .06], sd(12), 'dark', wh, dense=dn(2))
    box(B, (.136, 0, 1.62), (.012, .165, .03), 'glow', 'head')
    box(B, (.125, 0, 1.69), (.034, .22, .04), 'bone', 'head')
    box(B, (.14, 0, 1.58), (.012, .024, .09), 'bone', 'head')
    for sg in (1, -1):
        spike(B, (.13, sg * .07, 1.53), (.5, sg * .25, -.9), .13, .024, 'bone', 'head', sides=4)
        if detail(1):
            box(B, (.12, sg * .075, 1.555), (.012, .03, .03), 'cloth', 'head')
        hp = [(0, sg * .11, 1.69), (-.01, sg * .20, 1.74), (.0, sg * .29, 1.84), (.05, sg * .31, 1.96), (.10, sg * .28, 2.04)]
        loft(B, hp, [.052, .047, .038, .026, .0], [.052, .047, .038, .026, .0], sd(8), 'bone', 'head', caps=(True, False), ref=(1, 0, 0), dense=dn(2))
        if detail(1):
            loft(B, [(0, sg * .125, 1.715), (-.002, sg * .14, 1.725)], [.058, .056], [.058, .056], sd(8), 'trim', 'head', caps=(False, False), ref=(1, 0, 0))
    # ragged red cloth: a back cape and a front flap over the kilt
    nx, nz_ = (4, 4) if detail(0) else (3, 3)
    cloth_sheet(B, 1.34, .66, .20, .30, .21, .26, nx, nz_, 'cloth', 'dark', CW, skirt_w(.98, .85, .5, .09), wob=.025, jag=.06 if detail(0) else 0.0)
    # two-handed stone maul in the right hand (the haft axis is the barbarian's, 60 degrees: Melee_Brute solves the left hand onto it)
    g = hand_grip(rig, M0, 'R')
    ax = unit((math.cos(deg(60)), 0, math.sin(deg(60))))
    fw = unit((math.sin(deg(60)), 0, -math.cos(deg(60))))
    B.push(rig.to_bind(M0, 'weapon_socket'))
    ws = 'weapon_socket'
    loft(B, [g - ax * .30, g + ax * .20, g + ax * 1.02], [.028, .028, .033], [.028, .028, .033], sd(6), 'leather', ws, ref=(0, 1, 0), dense=dn(2))
    loft(B, [g - ax * .33, g - ax * .29], [.0, .042], [.0, .042], sd(6), 'bone', ws, caps=(True, False), ref=(0, 1, 0))
    for off in ((.0, .38, .72) if detail(1) else (.38,)):
        loft(B, [g + ax * (off + .02), g + ax * (off + .07)], [.04, .04], [.04, .04], sd(6), 'cloth', ws, caps=(False, False), ref=(0, 1, 0))
    top = g + ax * 1.05
    rot3 = np.column_stack((fw, (0, 1, 0), ax))
    box(B, top, (.50, .27, .26), 'blade', ws, rot=rot3)                                                             # the stone head
    for sgn in (1, -1):
        box(B, top + fw * (.255 * sgn), (.03, .31, .30), 'dark', ws, rot=rot3)                                      # hide-bound striking faces
    for sgn in (-1, 0, 1):
        box(B, top + ax * .0 + fw * (.11 * sgn), (.045, .30, .275), 'leather', ws, rot=rot3)                        # lashing bands
    for sgn in (1, -1):
        spike(B, top + ax * .13 + fw * (.13 * sgn), ax + fw * (.35 * sgn), .17, .035, 'bone', ws, sides=5)          # two bone spikes on top
    loft(B, [top + ax * .13, top + ax * .22], [.04, .0], [.04, .0], sd(6), 'bone', ws, caps=(True, False), ref=(0, 1, 0))
    for sgn in (1, -1):
        loft(B, [top - fw * .18 + (0, sgn * .13, 0), top - fw * .20 + (0, sgn * .14, -.12), top - fw * .20 + (0, sgn * .15, -.26)], [.02, .017, .0], [.005, .005, .0], 4,
             'cloth', ws, caps=(True, False), ref=(1, 0, 0))                                                          # red rags
        rune_faces(B, top, (0, sgn, 0), fw, ws, .12, .09, .1352)
    B.pop()
    return B, BRUTE_ATLAS


# ------------------------------------------------------------------ WARDEN (heavy body, Vault stone and verdigris, cyan mask, tower shield, halberd)
def build_warden(rig, M0):
    B = Body()
    R1.heavy_body(B, rig, M0, plate='plate', trim='trim', leather='leather', fist='dark')
    # massive stacked pauldrons with verdigris rims and three raised fins each
    for sg, s_ in SIDES:
        pc = np.array((0, sg * .27, 1.425))
        pz = [-.06, 0.0, .055, .10, .125]
        pr = [.195, .215, .185, .115, .05]
        loft(B, [pc + (0, 0, z) for z in pz], pr, [x * .92 for x in pr], sd(12), 'plate', f'clavicle.{s_}', dense=dn(2))
        loft(B, [pc + (0, 0, -.06), pc + (0, 0, -.085)], [.195, .185], [.18, .17], sd(12), 'trim', f'clavicle.{s_}')
        loft(B, [pc + (0, 0, .02), pc + (0, 0, .045)], [.222, .222], [.205, .205], sd(12), 'trim', f'clavicle.{s_}', caps=(False, False))
        for ang, up, ln in ((15, .9, .26), (-40, .8, .22), (70, .6, .18)):
            if not detail(1) and ang == 70:
                continue
            a = math.radians(ang)
            spike(B, pc + (0, 0, .07) + np.array((math.sin(a) * -.05, sg * .08 * math.cos(a), 0)), (math.sin(a) * -.4, sg * math.cos(a) * .6, up + .6), ln, .034, 'trim',
                  f'clavicle.{s_}', sides=4)
    wn = blend_chain(lambda p: p[2], [('chest', 0, 0), ('neck', 1.50, .02)])
    loft(B, [(-.02, 0, 1.44), (-.04, 0, 1.52), (-.05, 0, 1.585)], [.14, .145, .15], [.12, .125, .13], sd(10), 'plate', wn, caps=(False, False), vrange=(1, 0))       # a standing collar
    if detail(0):
        quad(B, (.19, 0, 1.36), np.array((0, .05, 0)), np.array((0, 0, .065)), 'rune', 'chest')                       # a Vault sigil on the breastplate
    # tabard to the shin and a long cape, both in dark Vault teal with verdigris hems
    sk = skirt_w(1.06, .8, .6, .06)
    hw = .125
    nzz = 3 if detail(0) else 2
    panel(B, (.19, -hw, 1.30), (.19, hw, 1.30), (.22, hw, .30), (.22, -hw, .30), 2, nzz, 'cloth', sk)
    panel(B, (-.17, hw, 1.30), (-.17, -hw, 1.30), (-.20, -hw, .30), (-.20, hw, .30), 2, nzz, 'cloth', sk)
    quad(B, (.223, 0, .86), np.array((0, .085, 0)), np.array((0, 0, .085)), 'emblem2', sk, double=True)
    nx, nz_ = (5, 6) if detail(0) else (3, 3)
    cloth_sheet(B, 1.44, .34, .22, .36, .21, .32, nx, nz_, 'cloth', 'dark', CW, skirt_w(.98, .85, .5, .09), wob=.02)
    # closed great helm: a stone dome, a dark face plate with a T-shaped cyan glow slit, verdigris brow band and cheek bars, a crown and a tall crest
    hz = [1.50, 1.56, 1.63, 1.69, 1.73]
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, z) for z in hz], [.100, .114, .114, .094, .052], [.108, .122, .122, .098, .056], sd(12), 'plate', wh, dense=dn(2))
    box(B, (.124, 0, 1.60), (.016, .17, .15), 'dark', 'head')                                                       # face plate
    box(B, (.134, 0, 1.628), (.012, .15, .026), 'glow', 'head')                                                     # the cyan mask: an eye slit ...
    box(B, (.134, 0, 1.575), (.012, .026, .09), 'glow', 'head')                                                     # ... and a nasal bar
    if detail(1):
        for k in (-2, -1, 1, 2):
            box(B, (.134, k * .03, 1.525), (.012, .01, .026), 'glow', 'head')                                       # glowing jaw teeth
    box(B, (.113, 0, 1.685), (.034, .225, .03), 'trim', 'head')
    if detail(1):
        for sg in (1, -1):
            box(B, (.045, sg * .11, 1.55), (.10, .025, .10), 'plate', 'head', rot=rotz(-sg * 12))
            box(B, (.13, sg * .085, 1.56), (.012, .012, .09), 'trim', 'head')
    loft(B, [(-.005, 0, 1.715), (-.005, 0, 1.74)], [.118, .114], [.126, .122], sd(12), 'trim', 'head', caps=(False, False))               # crown band
    npk = 7 if detail(0) else 5 if detail(1) else 3
    for k in range(npk):                                                                                             # crown points around the band, long and short
        a = 2 * math.pi * (k + .5) / npk
        base = np.array((-.005 + .118 * math.cos(a), .122 * math.sin(a), 1.735))
        spike(B, base, (.16 * math.cos(a), .16 * math.sin(a), 1.0), .20 if k % 2 == 0 else .14, .02, 'trim', 'head', sides=4)
    crest = [(.085, 0, 1.735), (.04, 0, 1.84), (-.02, 0, 1.93), (-.075, 0, 2.00), (-.12, 0, 1.92), (-.14, 0, 1.81)]
    loft(B, crest, [.030, .05, .062, .05, .045, .03], [.014, .018, .02, .018, .016, .012], sd(6), 'trim', 'head', caps=(True, True), ref=(0, 1, 0), dense=dn(2))
    box(B, (.075, 0, 1.775), (.02, .03, .045), 'glow', 'head')                                                       # a cyan gem at the crest's foot
    # tall tower shield: cold stone with a verdigris rim, the Vault sigil on its face, a cyan boss
    outline = [(-.17, .46), (-.11, .53), (.11, .53), (.17, .46), (.18, -.30), (.0, -.52), (-.18, -.30)]
    B2.tower_shield(B, rig, M0, outline, .36, 1.06, 'emblem', 'dark', 'trim', 'glow')
    # halberd in the right hand: a dark haft with verdigris collars, an axe blade, a spear point and a back hook, cyan runes on the blade
    g = hand_grip(rig, M0, 'R')
    ax = unit((math.cos(deg(62)), 0, math.sin(deg(62))))
    fw = unit((math.sin(deg(62)), 0, -math.cos(deg(62))))
    B.push(rig.to_bind(M0, 'weapon_socket'))
    ws = 'weapon_socket'
    loft(B, [g - ax * .35, g + ax * .30, g + ax * 1.50], [.024, .024, .028], [.024, .024, .028], sd(6), 'dark', ws, ref=(0, 1, 0), dense=dn(2))
    loft(B, [g - ax * .39, g - ax * .34], [.0, .036], [.0, .036], sd(6), 'trim', ws, caps=(True, False), ref=(0, 1, 0))
    box(B, g - ax * .41, (.03, .03, .03), 'glow', ws)
    for off in ((.12, .62, 1.18) if detail(1) else (.62,)):
        loft(B, [g + ax * off, g + ax * (off + .06)], [.034, .034], [.034, .034], sd(6), 'trim', ws, caps=(False, False), ref=(0, 1, 0))
    top = g + ax * 1.36
    blade = [(.02, .10), (.12, .13), (.24, .22), (.31, .09), (.31, -.09), (.22, -.20), (.12, -.12), (.02, -.09)]
    prism(B, blade, top, fw, ax, np.cross(fw, ax), .014, 'blade', 'blade', ws, taper=.55)
    box(B, top - fw * .015, (.07, .05, .13), 'trim', ws, rot=np.column_stack((fw, (0, 1, 0), ax)))
    loft(B, [top + ax * .06, top + ax * .20, top + ax * .40], [.034, .028, .0], [.012, .010, .0], 4, 'blade', ws, caps=(True, False), ref=(1, 0, 0))             # the spear point
    spike(B, top - fw * .04, -fw * .9 - ax * .25, .20, .026, 'trim', ws, sides=4)                                    # back hook
    rune_faces(B, top + fw * .15 + ax * .02, (0, 1, 0), fw, ws, .05, .07, .0155)
    B.pop()
    return B, WARDEN_ATLAS


# ------------------------------------------------------------------ atlases
FOE_BASE = {
    'plate': ('bands', dict(base=(.62, .44, .30), line=(.96, .88, .68), spacing=64)),         # tan hide, bone-pale stitching
    'trim': ('scales', dict(a=(1.0, .56, .20), b=(.80, .36, .12))),                           # ember copper
    'cloth': ('folds', dict(base=(.80, .17, .12), period=28, hem=(.96, .82, .56), hem2=(.62, .12, .08))),   # raider red, bone-pale hem
    'leather': ('weave', dict(base=(.54, .36, .22))),
    'blade': ('blade', dict(base=(.80, .84, .90))),
    'glow': ('flat', dict(base=(1.0, .62, .22))),                                             # ember
    'rune': ('rune', dict(bg=(.32, .10, .05), fg=(1.0, .62, .22))),
    'skin': ('flat', dict(base=(.76, .56, .43))),
    'bone': ('flat', dict(base=(.96, .92, .80))),
    'accent': ('flat', dict(base=(.96, .93, .84))),                                           # bone-white
    'dark': ('flat', dict(base=(.28, .20, .16))),
    'emblem': ('folds', dict(base=(.72, .17, .12), period=22, hem=(.96, .88, .66), hem2=(.78, .34, .14))),
    'emblem2': ('flat', dict(base=(.30, .20, .14))),
}


def foe_atlas(**over):
    a = dict(FOE_BASE)
    a.update(over)
    return a


SCOUT_ATLAS = foe_atlas(blade=('blade', dict(base=(.78, .80, .82))))
ARCHER_ATLAS = foe_atlas(dark=('flat', dict(base=(.34, .22, .15))))                           # dark-wood bow, lifted: the stave must read against the cloth
SKIRMISHER_ATLAS = foe_atlas(
    plate=('bands', dict(base=(.68, .50, .34), line=(.96, .88, .68), spacing=64)),
    skin=('flat', dict(base=(.78, .57, .44))),
    emblem=('flat', dict(base=(.78, .57, .44))),                                              # bare skin, repainted by paint_warpaint
    emblem2=('flat', dict(base=(.34, .20, .14))),
    blade=('blade', dict(base=(.84, .86, .88))))
BRUTE_ATLAS = foe_atlas(
    blade=('flat', dict(base=(.66, .62, .58))),                                                # stone (the maul head), repainted by paint_stone
    emblem=('flat', dict(base=(.72, .52, .40))),                                               # bare skin, repainted by paint_warpaint
    dark=('flat', dict(base=(.32, .23, .18))),
    glow=('flat', dict(base=(1.0, .50, .18))),
    rune=('rune', dict(bg=(.30, .08, .05), fg=(1.0, .50, .18))),
    emblem2=('flat', dict(base=(.30, .20, .14))))
WARDEN_ATLAS = {
    'plate': ('bands', dict(base=(.66, .72, .76), line=(.88, .93, .94), spacing=64)),         # cold pale stone, pale joints
    'trim': ('scales', dict(a=(.30, .76, .64), b=(.20, .58, .50))),                           # verdigris
    'cloth': ('folds', dict(base=(.24, .46, .50), period=30, hem=(.40, .80, .68), hem2=(.20, .54, .50))),   # Vault teal, verdigris hem
    'leather': ('weave', dict(base=(.30, .34, .38))),
    'blade': ('blade', dict(base=(.66, .78, .80))),
    'glow': ('flat', dict(base=(.50, .85, 1.0))),                                             # the Vault's cyan (the Waystone's)
    'rune': ('rune', dict(bg=(.04, .14, .18), fg=(.55, .92, 1.0))),
    'skin': ('flat', dict(base=(.60, .66, .66))),
    'bone': ('flat', dict(base=(.84, .90, .88))),
    'accent': ('flat', dict(base=(.88, .94, .94))),
    'dark': ('flat', dict(base=(.22, .28, .32))),
    'emblem': ('emblem', dict(base=(.24, .38, .42), border=(.30, .76, .64), mark=(.55, .92, 1.0), symbol='sigil')),
    'emblem2': ('emblem', dict(base=(.20, .36, .40), border=(.30, .76, .64), mark=(.55, .92, 1.0), symbol='sigil')),
}


def paint_warpaint(A, region, spec, seed):
    """Bare skin with a red war stripe running diagonally across the tile (a spiral band on a torso or limb) and short bone-white dashes beside it."""
    rng = np.random.default_rng(seed + 31)
    n = K.TILE
    nz = K.smooth_noise(rng, n, 16)
    yy, xx = np.mgrid[0:n, 0:n]
    u, v = (xx + .5) / n, (yy + .5) / n
    t = np.ones((n, n, 4), np.float32)
    base = np.array(spec[region][1]['base'], np.float32)
    t[..., :3] = base * (.90 + .2 * nz[..., None])
    red = np.array((.82, .12, .09), np.float32)
    bone = np.array((.96, .92, .80), np.float32)
    band = np.abs(((u * 1.0 + v * 1.0) % 1.0) - .5) < .14
    t[band, :3] = red * (.9 + .2 * nz[band][:, None])
    for off in (-.30, .30):
        edge = np.abs(((u + v + off) % 1.0) - .5) < .025
        t[edge, :3] = bone
    dots = ((np.floor(u * 12) + np.floor(v * 12)) % 3 == 0) & (np.hypot(u * 12 % 1 - .5, v * 12 % 1 - .5) < .17) & ~band
    t[dots, :3] = bone * .95
    _tile(A, region, np.clip(t, 0, 1))


def paint_stone(A, region, spec, seed):
    """A rough stone with cracks, pits and a few paler flecks (the maul head)."""
    rng = np.random.default_rng(seed + 37)
    n = K.TILE
    nz = K.smooth_noise(rng, n, 16)
    nz2 = K.smooth_noise(rng, n, 32)
    yy, xx = np.mgrid[0:n, 0:n]
    base = np.array(spec[region][1]['base'], np.float32)
    t = np.ones((n, n, 4), np.float32)
    t[..., :3] = base * (.70 + .5 * nz[..., None]) * (.9 + .2 * nz2[..., None])
    for _ in range(10):
        x0, y0 = rng.integers(0, n), rng.integers(0, n)
        for s in range(rng.integers(20, 55)):
            x0 = (x0 + rng.integers(-2, 3)) % n
            y0 = (y0 + rng.integers(0, 3)) % n
            t[y0, x0, :3] *= .45
    for _ in range(40):
        cx, cy, r = rng.integers(8, n - 8), rng.integers(8, n - 8), rng.integers(2, 5)
        t[np.hypot(xx - cx, yy - cy) < r, :3] *= .6
    t[rng.random((n, n)) > .993, :3] = (.92, .9, .86)
    _tile(A, region, np.clip(t, 0, 1))


CHARACTERS = {   # key: (builder, name, atlas seed)
    'scout': (build_scout, 'SK_Foe_Scout', 1483),
    'archer': (build_archer, 'SK_Foe_Archer', 1484),
    'skirmisher': (build_skirmisher, 'SK_Foe_Skirmisher', 1485),
    'brute': (build_brute, 'SK_Foe_Brute', 1486),
    'warden': (build_warden, 'SK_Foe_Warden', 1487),
}
ASSET_IDS = {'scout': 'sk-foe-scout', 'archer': 'sk-foe-archer', 'skirmisher': 'sk-foe-skirmisher', 'brute': 'sk-foe-brute', 'warden': 'sk-foe-warden'}
FOES = tuple(CHARACTERS)

PAINTERS = {   # key -> [(region, fn(A, region, spec, seed))]: marks painted over the base atlas
    'skirmisher': [('emblem', paint_warpaint)],
    'brute': [('emblem', paint_warpaint), ('blade', paint_stone)],
}


def make_atlas(key, spec, seed):
    A = C.make_atlas(key, spec, seed)
    for region, fn in PAINTERS.get(key, ()):
        fn(A, region, spec, seed)
    return A


C.CHARACTERS.update(CHARACTERS)
C.ASSET_IDS.update(ASSET_IDS)
