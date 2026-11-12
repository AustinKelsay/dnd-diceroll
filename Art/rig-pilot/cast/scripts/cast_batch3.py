"""Character recipes for the #150 rigged cast, batch 3: sorcerer, warlock and druid.

Same skeleton, atlas layout, LOD helpers and conventions as batches 0 to 2 (Art/rig-pilot/r001/README.md, cast/README.md,
cast/README_BATCH2.md): the 25-bone armature, one material on its own 1024x1024 atlas of 13 tiles, RGBA vertex colour with the glow
mask in alpha on runes, gems and the focus only, at most 4 influences, the ten shared clips (Brace included). This module registers
its classes in cast_characters.CHARACTERS / ASSET_IDS in memory and imports cast_batch2 (the Brace clip and the batch 2 helpers), so
no batch 1 or batch 2 file is edited and their asset records keep their hashes.

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
import rigkit as K                                                              # noqa: E402
from rigkit import Body, JOINTS, blend_chain, box, deg, detail, dn, frame_matrix, loft, panel, prism, quad, sd, spike   # noqa: E402

R1 = C.R1
SIDES = C.SIDES
rotx, roty, rotz = C.rotx, C.roty, C.rotz
skirt_w, hand_grip, rune_faces = C.skirt_w, C.hand_grip, C.rune_faces
unit, arc_loft, cloth_sheet, dagger, agile_body, pauldron_round = C.unit, C.arc_loft, C.cloth_sheet, C.dagger, C.agile_body, C.pauldron_round
arm_frame = B2.arm_frame


def leaf(B, base, direction, length, w, t, region, wspec, radial=(1, 0, 0)):
    """A flattened diamond (a leaf or a flame lick): width w across `radial x direction`, thickness t along `radial`. Rigid on one bone."""
    d = unit(direction)
    loft(B, [np.array(base, float), np.array(base, float) + d * length * .45, np.array(base, float) + d * length], [w, w * .9, 0.0], [t, t * .9, 0.0], 4,
         region, wspec, caps=(True, False), ref=tuple(radial))


def edge_tube(B, path, r, region, wspec):
    loft(B, path, [r] * len(path), [r] * len(path), 4, region, wspec, caps=(True, True), ref=(1, 0, 0))


def ring_leaves(B, z, ra, rb, n, length, w, t, region, wspec, a0=0.0, tilt=.35, regions=None):
    """Leaves (or licks) hanging from a ring around the body, flat to the surface."""
    for k in range(n):
        a = a0 + 2 * math.pi * (k + .5) / n
        radial = unit((math.cos(a) / max(rb, 1e-3), math.sin(a) / max(ra, 1e-3), 0))
        base = np.array((rb * math.cos(a), ra * math.sin(a), z))
        leaf(B, base, (radial[0] * tilt, radial[1] * tilt, -1.0), length, w, t, regions[k % len(regions)] if regions else region, wspec, radial=radial)


def _tile(A, region, t):
    i = K.REGIONS.index(region)
    c, r = i % 4, i // 4
    A[r * K.TILE:(r + 1) * K.TILE, c * K.TILE:(c + 1) * K.TILE] = t


# ------------------------------------------------------------------ SORCERER (fitted coat over the agile body, crystal focus)
def ember_veins(B, rig, M0):
    """Glowing cracks along both arms (the innate magic showing through): thin zigzag glow boxes on the outer face of the sleeves."""
    for sg, s in SIDES:
        sh, el, wr, d = arm_frame(s)
        out = np.array((0.0, sg, 0.0))
        for k, off in enumerate((.10, .19, .28, .38, .47) if detail(0) else (.12, .30, .46) if detail(1) else (.3,)):
            c = sh + d * off + out * (.052 if off < .30 else .046) + np.array((.012 * (-1) ** k, 0, 0))
            dd = d + np.array((.35 * (-1) ** k, 0, 0))
            bone = f'upperarm.{s}' if off < .30 else f'forearm.{s}'
            box(B, c, (.009, .006, .07), 'glow', bone, rot=frame_matrix(dd, (0, 1, 0), 2))


def crystal_focus(B, rig, M0):
    """A crackling focus crystal held in the right hand: a dark-gold claw cradle, a tall central crystal, two shorter tilted ones and
    short jagged sparks around it. Rigid on weapon_socket, authored where the hand is in Idle. The crystal, sparks and runes are glow."""
    g = hand_grip(rig, M0, 'R')
    ax = unit((math.cos(deg(55)), 0, math.sin(deg(55))))
    side = unit(np.cross(ax, (0, 1, 0)))
    B.push(rig.to_bind(M0, 'weapon_socket'))
    ws = 'weapon_socket'
    loft(B, [g - ax * .05, g + ax * .03, g + ax * .08], [.028, .03, .04], [.028, .03, .04], sd(6), 'dark', ws, ref=(0, 1, 0))               # grip and cup
    for k in range(4 if detail(0) else 3):                                                                                                # gold claws
        a = 2 * math.pi * k / (4 if detail(0) else 3)
        dirn = side * math.cos(a) + np.cross(ax, side) * math.sin(a)
        loft(B, [g + ax * .08 + dirn * .032, g + ax * .15 + dirn * .045], [.011, .006], [.011, .006], 4, 'trim', ws, caps=(True, False), ref=(0, 1, 0))
    c0 = g + ax * .10
    loft(B, [c0 + ax * .0, c0 + ax * .09, c0 + ax * .22, c0 + ax * .33], [0, .052, .046, 0], [0, .052, .046, 0], 6 if not K.LOD else 4, 'glow', ws,
         caps=(False, False), ref=(0, 1, 0))
    for sgn in (1, -1):
        d2 = unit(ax + side * .55 * sgn)
        loft(B, [c0 + ax * .03, c0 + d2 * .09, c0 + d2 * .19], [0, .03, 0], [0, .03, 0], 4, 'glow', ws, caps=(False, False), ref=(0, 1, 0))
    if detail(0):                                                                                                                         # crackling sparks
        for k in range(7):
            a = 2 * math.pi * k / 7 + .4
            dirn = unit(side * math.cos(a) * 1.0 + np.cross(ax, side) * math.sin(a) + ax * (.35 if k % 2 else -.1))
            base = c0 + ax * (.10 + .05 * (k % 3)) + dirn * .045
            mid = base + dirn * .05 + unit(np.cross(dirn, ax)) * .02
            loft(B, [base, mid, mid + dirn * .05 + unit(np.cross(dirn, ax)) * -.025], [.007, .006, 0], [.007, .006, 0], 4, 'glow', ws, caps=(True, False), ref=(0, 1, 0))
    elif detail(1):
        for k in range(4):
            a = 2 * math.pi * k / 4 + .4
            dirn = unit(side * math.cos(a) + np.cross(ax, side) * math.sin(a) + ax * .3)
            spike(B, c0 + ax * .14 + dirn * .045, dirn, .10, .008, 'glow', ws, sides=4)
    B.pop()


def build_sorcerer(rig, M0):
    B = Body()
    # body: crimson fitted coat bodice, black sleeves and trousers, ember-gold cuffs, bare hands
    agile_body(B, rig, M0, jerkin='emblem', sleeve='cloth', pants='dark', boot='leather', cuff='trim', glove='skin', belt='trim', buckle='glow')
    cw = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.0, .10), ('spine2', 1.18, .08), ('chest', 1.32, .06)])
    sk = skirt_w(.98, .85, .85, .09)
    # the coat: shorter and fitted, open at the front, flaring to the knee, lined in black, with ember-gold edging
    zs = [1.02, .92, .80, .68, .56]
    ra = [.175, .20, .23, .255, .275]
    rb = [.125, .15, .17, .19, .205]
    arc_loft(B, [(0, 0, z) for z in zs], ra, rb, sd(14), 'emblem', sk, a0=42, a1=318, vrange=(1, 0))
    arc_loft(B, [(0, 0, z) for z in zs], [x - .01 for x in ra], [x - .01 for x in rb], sd(14), 'dark', sk, a0=42, a1=318, flip=True)
    for a in (42, 318):
        pts = [np.array((rb_ * math.cos(deg(a)), -ra_ * math.sin(deg(a)), z)) for z, ra_, rb_ in zip(zs, ra, rb)]
        edge_tube(B, pts, .013, 'trim', sk)
    loft(B, [(0, 0, .575), (0, 0, .545)], [.278, .282], [.208, .212], sd(14), 'trim', sk, caps=(False, False))                              # hem band
    sk2 = skirt_w(1.06, .8, .6, .06)
    panel(B, (.11, .00, 1.07), (.15, .11, 1.07), (.20, .13, .50), (.15, .02, .50), 2, 3 if detail(0) else 2, 'plate', sk2)                  # a hanging ember sash tail
    # flaring collar behind the head: crimson licks edged in ember, no horns or spikes
    nl = 5 if detail(1) else 3
    for k in range(nl):
        t = k / (nl - 1) - .5
        ang = t * 120
        c = np.array((-.085, math.sin(deg(ang)) * .13, 1.47 + .07 * (1 - abs(t) * 1.3)))
        h = .17 - .06 * abs(t)
        outline = [(-.05, 0), (.05, 0), (.03, h * .55), (0.0, h * 1.25), (-.03, h * .55)]
        prism(B, outline, c + np.array((0, 0, .02)), (0, 1, 0), (0, 0, 1), (-1, 0, 0), .008, 'emblem', 'emblem', 'chest', taper=.9)
    loft(B, [(0, 0, 1.44), (0, 0, 1.52)], [.115, .095], [.10, .088], sd(10), 'emblem', blend_chain(lambda p: p[2], [('chest', 0, 0), ('neck', 1.48, .03)]))   # collar
    # a short black mantle with an ember hem, deeper on the left shoulder
    loft(B, [(0, 0, 1.50), (0, 0, 1.43), (0, 0, 1.37)], [.13, .25, .28], [.10, .17, .20], sd(14), 'cloth', 'chest', caps=(True, False), vrange=(1, 0), dense=dn(2))
    loft(B, [(0, 0, 1.382), (0, 0, 1.360)], [.277, .281], [.197, .201], sd(14), 'trim', 'chest', caps=(False, False))
    loft(B, [(0, .25, 1.43), (0, .27, 1.33), (0, .275, 1.22)], [.10, .105, .07], [.085, .09, .06], sd(10), 'cloth', 'clavicle.L', caps=(True, False), vrange=(1, 0))   # left cape
    loft(B, [(0, .272, 1.235), (0, .275, 1.215)], [.07, .068], [.06, .058], sd(10), 'trim', 'clavicle.L', caps=(False, False))
    if detail(0):                                                                                                                       # a short cape down the back
        cloth_sheet(B, 1.40, .86, .20, .30, .17, .23, 4, 3, 'cloth', 'dark', cw, skirt_w(.98, .85, .5, .09), wob=.02, jag=.03)
    # head: bare, swept-back black hair with a crimson streak, ember-gold brow clasp
    wn = blend_chain(lambda p: p[2], [('chest', 0, 0), ('neck', 1.48, .03)])
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, 1.46), (0, 0, 1.52)], [.068, .06], [.068, .06], sd(8), 'skin', wh)
    loft(B, [(0, 0, 1.52), (0, 0, 1.60), (0, 0, 1.67), (0, 0, 1.73)], [.083, .098, .096, .062], [.09, .106, .10, .062], sd(14), 'skin', wh, dense=dn(2))
    for sg in (1, -1):
        box(B, (.104, sg * .045, 1.625), (.012, .045, .026), 'dark', 'head')
        if detail(1):
            box(B, (.103, sg * .045, 1.652), (.012, .05, .008), 'dark', 'head', rot=rotx(sg * -10))
    box(B, (.112, 0, 1.585), (.022, .02, .05), 'skin', 'head')
    loft(B, [(-.005, 0, 1.655), (-.005, 0, 1.71), (-.02, 0, 1.775)], [.108, .106, .06], [.114, .11, .064], sd(12), 'emblem2', 'head', caps=(False, True), dense=dn(2))   # hair
    for k, sg in enumerate((1, 0, -1)):                                                                                                 # swept-back locks
        loft(B, [(-.05, sg * .045, 1.76), (-.14, sg * .06, 1.77), (-.23, sg * .09, 1.72), (-.28, sg * .10, 1.64)], [.028, .026, .018, 0], [.028, .026, .018, 0],
             sd(6), 'emblem2', 'head', caps=(True, False), ref=(0, 1, 0))
    if detail(1):
        box(B, (.10, .05, 1.735), (.012, .035, .075), 'plate', 'head', rot=rotx(0))                                                     # crimson streak
        loft(B, [(.10, 0, 1.672), (.10, 0, 1.70)], [.11, .11], [.04, .04], 6, 'trim', 'head', caps=(False, False), ref=(0, 0, 1))        # brow clasp
        box(B, (.118, 0, 1.69), (.012, .03, .03), 'glow', 'head')
    ember_veins(B, rig, M0)
    # the crystal focus
    crystal_focus(B, rig, M0)
    if detail(1):                                                                                                                       # a small ember vial and pouch at the right hip
        box(B, (.02, -.215, .96), (.07, .05, .09), 'leather', 'pelvis')
        box(B, (.02, -.243, .97), (.03, .012, .04), 'glow', 'pelvis')
    return B, SORCERER_ATLAS


SORCERER_ATLAS = {
    'plate': ('bands', dict(base=(.86, .16, .22), line=(1.0, .66, .36), spacing=64)),         # crimson
    'trim': ('scales', dict(a=(1.0, .78, .36), b=(.92, .56, .22))),                           # ember gold
    'cloth': ('folds', dict(base=(.42, .32, .50), period=30, hem=(1.0, .62, .26), hem2=(.88, .36, .16))),   # black-violet mantle (lifted one notch after the contrast gate), ember hem
    'leather': ('weave', dict(base=(.44, .30, .28))),
    'blade': ('blade', dict(base=(.84, .86, .92))),
    'glow': ('flat', dict(base=(1.0, .66, .30))),
    'rune': ('rune', dict(bg=(.30, .06, .05), fg=(1.0, .62, .26))),
    'skin': ('flat', dict(base=(.88, .68, .58))),
    'bone': ('flat', dict(base=(.98, .93, .84))),
    'accent': ('flat', dict(base=(.99, .92, .82))),
    'dark': ('flat', dict(base=(.30, .20, .26))),                                             # black trousers and lining (lifted after the contrast gate)
    'emblem': ('folds', dict(base=(.88, .14, .20), period=28, hem=(1.0, .66, .30), hem2=(.90, .40, .16))),   # crimson coat
    'emblem2': ('flat', dict(base=(.16, .10, .14))),                                          # black hair
}


# ------------------------------------------------------------------ WARLOCK (agile body, long hooded coat, pact tome and token)
def pact_eye(B, c, n, u, size, wspec):
    """A glowing eye sigil quad on a flat face with normal n."""
    n, u = unit(n), unit(u)
    v = np.cross(n, u)
    quad(B, np.array(c, float) + n * .004, u * size, v * size * .8, 'rune', wspec, double=True)


def build_warlock(rig, M0):
    B = Body()
    agile_body(B, rig, M0, jerkin='plate', sleeve='plate', pants='dark', boot='leather', cuff='trim', glove='dark', belt='leather', buckle='glow')
    cw = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.0, .10), ('spine2', 1.18, .08), ('chest', 1.32, .06)])
    sk = skirt_w(1.06, .8, .7, .06)
    # long coat: front flaps either side of a dark gap, a back sheet to the calf with a jagged hem, a pale-lavender hem band, glowing glyphs
    nzf = 4 if detail(0) else 3
    panel(B, (.150, .035, 1.03), (.14, .21, 1.03), (.205, .25, .36), (.215, .07, .36), 2, nzf, 'cloth', sk)
    panel(B, (.150, -.21, 1.03), (.14, -.035, 1.03), (.215, -.07, .36), (.205, -.25, .36), 2, nzf, 'cloth', sk)
    nx, nz_ = (6, 7) if detail(0) else (4, 4) if detail(1) else (3, 3)
    cloth_sheet(B, 1.40, .34, .20, .34, .17, .31, nx, nz_, 'cloth', 'emblem', cw, skirt_w(.98, .85, .5, .09), wob=.025, jag=.06 if detail(0) else 0.0)
    nr = 6 if detail(0) else 4 if detail(1) else 3
    for k in range(nr):                                                                                                                  # glyphs on the back hem
        t = (k + .5) / nr * 2 - 1
        quad(B, (-.318 - .012, t * .26, .43), np.array((0, .03, 0)), np.array((0, 0, .045)), 'rune', skirt_w(.98, .85, .5, .09), double=True)
    for sg in (1, -1):
        quad(B, (.19, sg * .15, .43), np.array((0, .025 * sg, 0)), np.array((0, 0, .04)), 'rune', sk, double=True)
    if detail(1):
        for sg in (1, -1):                                                                                                               # pale edging on the front flaps
            panel(B, (.152, sg * .035, 1.03), (.152, sg * .042, 1.03), (.218, sg * .075, .36), (.217, sg * .068, .36), 1, nzf, 'trim', sk)
    # bone-pale left pauldron of stacked plates, a plain plum mantle on the right
    loft(B, [(0, 0, 1.50), (0, 0, 1.43), (0, 0, 1.37)], [.13, .25, .28], [.10, .17, .20], sd(14), 'cloth', 'chest', caps=(True, False), vrange=(1, 0), dense=dn(2))
    for z, r in ((1.415, .125), (1.455, .105), (1.49, .08)):
        pc = np.array((0, .245, z))
        loft(B, [pc + (0, 0, -.025), pc + (0, 0, .02)], [r, r * .85], [r * .93, r * .8], sd(10), 'trim', 'clavicle.L', dense=dn(1))
    loft(B, [(0, -.255, 1.385), (0, -.255, 1.36)], [.115, .11], [.105, .10], sd(10), 'trim', 'clavicle.R', caps=(False, False))
    # tall stand collar and a pointed cowl, lined in pale bone
    loft(B, [(-.02, 0, 1.42), (-.04, 0, 1.52), (-.05, 0, 1.58)], [.115, .12, .13], [.10, .115, .125], sd(10), 'plate', 'chest', caps=(False, False), vrange=(1, 0))
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, 1.46), (0, 0, 1.52)], [.066, .058], [.066, .058], sd(8), 'skin', wh)
    loft(B, [(0, 0, 1.50), (0, 0, 1.58), (0, 0, 1.68)], [.075, .085, .09], [.08, .092, .095], sd(10), 'skin', wh)
    for sg in (1, -1):
        box(B, (.093, sg * .04, 1.625), (.012, .05, .022), 'dark', 'head')
        box(B, (.095, sg * .04, 1.648), (.010, .055, .008), 'bone', 'head')
    hood = [(-.015, 0, 1.50), (-.015, 0, 1.58), (-.02, 0, 1.665), (-.05, 0, 1.725), (-.10, 0, 1.745), (-.20, 0, 1.70)]
    hra, hrb = [.120, .130, .128, .105, .06, .012], [.126, .138, .136, .114, .07, .02]
    arc_loft(B, hood, hra, hrb, sd(12), 'cloth', wh, a0=58, a1=302, vrange=(1, 0))
    arc_loft(B, hood, [x - .012 for x in hra], [x - .012 for x in hrb], sd(12), 'accent', wh, a0=58, a1=302, flip=True)                  # pale lining
    loft(B, [(-.01, 0, 1.485), (-.01, 0, 1.50)], [.145, .14], [.135, .13], sd(12), 'trim', wh, caps=(False, False))
    # pact token: an eye pendant on the chest, a bandolier chain and a tome hanging at the left hip
    if detail(1):
        chain = [(.13, -.10, 1.44), (.17, -.02, 1.30), (.19, .06, 1.15), (.20, .14, 1.00), (.12, .21, .92)]
        edge_tube(B, chain[:3], .007, 'bone', 'chest')
        edge_tube(B, chain[2:], .007, 'bone', 'pelvis')
        for p in chain[1:4]:
            box(B, p, (.018, .018, .018), 'bone', 'chest' if p[2] > 1.2 else 'pelvis')
    box(B, (.185, 0, 1.285), (.016, .07, .09), 'trim', 'chest', rot=rotx(45))                                                           # pendant: pale frame, glowing eye
    box(B, (.194, 0, 1.285), (.012, .034, .05), 'glow', 'chest', rot=rotx(45))
    box(B, (.03, .245, .84), (.20, .06, .26), 'trim', 'pelvis')
    box(B, (.03, .247, .84), (.19, .062, .25), 'emblem', 'pelvis')
    box(B, (.03, .280, .84), (.16, .006, .21), 'emblem2', 'pelvis')
    pact_eye(B, (.03, .284, .84), (0, 1, 0), (1, 0, 0), .07, 'pelvis')
    box(B, (.03, .255, .975), (.05, .07, .04), 'trim', 'pelvis')
    # a rod in the right hand: dark shaft, pale rings, a claw cradling a green orb
    g = hand_grip(rig, M0, 'R')
    ax = unit((math.cos(deg(50)), 0, math.sin(deg(50))))
    B.push(rig.to_bind(M0, 'weapon_socket'))
    ws = 'weapon_socket'
    loft(B, [g - ax * .13, g + ax * .20, g + ax * .52], [.017, .017, .022], [.017, .017, .022], sd(6), 'dark', ws, ref=(0, 1, 0), dense=dn(2))
    loft(B, [g - ax * .16, g - ax * .12], [.0, .028], [.0, .028], sd(6), 'trim', ws, caps=(True, False), ref=(0, 1, 0))
    for off in ((.04, .22, .38) if detail(1) else (.22,)):
        loft(B, [g + ax * off, g + ax * (off + .025)], [.027, .027], [.027, .027], sd(6), 'trim', ws, caps=(False, False), ref=(0, 1, 0))
    top = g + ax * .54
    side = unit(np.cross(ax, (0, 1, 0)))
    for k in range(3):
        a = 2 * math.pi * k / 3
        dirn = side * math.cos(a) + np.cross(ax, side) * math.sin(a)
        loft(B, [top, top + dirn * .045 + ax * .05, top + dirn * .02 + ax * .13], [.012, .010, 0], [.012, .010, 0], 4, 'trim', ws, caps=(True, False), ref=(0, 1, 0))
    loft(B, [top + ax * .02, top + ax * .08, top + ax * .15], [0, .04, 0], [0, .04, 0], 6 if not K.LOD else 4, 'glow', ws, caps=(False, False), ref=(0, 1, 0))
    B.pop()
    # glowing sigil on the back of the left hand
    sh, el, wr, d = arm_frame('L')
    hc = wr + d * .055
    quad(B, hc + np.array((0, .036, 0)), np.array((0, 0, .03)), np.array((.03, 0, 0)), 'rune', 'hand.L', double=True)
    return B, WARLOCK_ATLAS


WARLOCK_ATLAS = {
    'plate': ('bands', dict(base=(.62, .34, .78), line=(.84, .66, .93), spacing=64)),         # plum coat (lifted twice after the contrast gate)
    'trim': ('scales', dict(a=(.96, .92, .99), b=(.80, .74, .90))),                           # pale bone-lavender trim
    'cloth': ('folds', dict(base=(.68, .38, .84), period=30, hem=(.88, .82, .95), hem2=(.56, .92, .30))),   # plum, pale hem, green line
    'leather': ('weave', dict(base=(.52, .36, .54))),
    'blade': ('blade', dict(base=(.84, .90, .82))),
    'glow': ('flat', dict(base=(.62, 1.0, .34))),                                             # sickly green
    'rune': ('rune', dict(bg=(.07, .13, .05), fg=(.62, 1.0, .34))),
    'skin': ('flat', dict(base=(.72, .74, .68))),
    'bone': ('flat', dict(base=(.96, .93, .86))),
    'accent': ('flat', dict(base=(.97, .94, .99))),                                           # hood lining, half mask
    'dark': ('flat', dict(base=(.48, .30, .60))),
    'emblem': ('flat', dict(base=(.40, .20, .52))),                                           # coat lining, tome cover edge
    'emblem2': ('flat', dict(base=(.22, .09, .30))),                                          # tome cover, repainted by paint_eye
}


def paint_eye(A, region, spec, seed):
    """An original pact sigil: an almond eye with a slit pupil in a ring of short rays, pale green on deep plum."""
    rng = np.random.default_rng(seed + 19)
    n = K.TILE
    nz = K.smooth_noise(rng, n, 16)
    yy, xx = np.mgrid[0:n, 0:n]
    u = (xx + .5) / n * 2 - 1
    v = (yy + .5) / n * 2 - 1
    t = np.ones((n, n, 4), np.float32)
    t[..., :3] = np.array(spec[region][1]['base'], np.float32) * (.92 + .14 * nz[..., None])
    green = np.array((.66, 1.0, .40), np.float32)
    t[(np.abs(u) > .88) | (np.abs(v) > .93), :3] = green * .8
    lid = .46 * np.clip(1 - u * u, 0, 1) ** .9
    almond = np.abs(v) < lid
    outline = almond & (np.abs(v) > lid - .06)
    iris = almond & (np.hypot(u, v) < .30)
    pupil = (np.abs(u) < .06) & (np.abs(v) < .26)
    ang = np.arctan2(v, u)
    r = np.hypot(u, v)
    rays = (np.abs(((ang * 9 / math.pi) % 1.0) - .5) < .2) & (r > .62) & (r < .80) & ~almond
    t[outline | rays, :3] = green
    t[iris, :3] = green * .55
    t[pupil, :3] = (.05, .02, .08)
    _tile(A, region, t)


# ------------------------------------------------------------------ DRUID (robed body, leaf mantle, circlet, gnarled staff)
DZ = [1.00, .90, .76, .60, .46, .38]
DRA = [.185, .21, .24, .27, .29, .295]
DRB = [.14, .165, .19, .21, .22, .225]


def druid_ra(z):
    return float(np.interp(-z, [-x for x in DZ], DRA))


def druid_rb(z):
    return float(np.interp(-z, [-x for x in DZ], DRB))


def gnarled(B, pts, r0, r1, region, wspec, knots=()):
    """A bent branch: a loft along pts with a thickness taper and swollen knots at the given parameters."""
    n = len(pts)
    ra = []
    for i in range(n):
        t = i / (n - 1)
        r = r0 + (r1 - r0) * t
        for kt in knots:
            r *= 1 + .45 * math.exp(-((t - kt) / .05) ** 2)
        ra.append(r)
    loft(B, pts, ra, ra, sd(7), region, wspec, caps=(True, True), ref=(0, 1, 0), dense=dn(2))


def build_druid(rig, M0):
    B = Body()
    J = JOINTS
    sk = skirt_w(.98, .85, .85, .09)
    # bark-brown tunic to the shin and, over it, a moss-green skirted robe split at the front, with a ragged leaf hem
    loft(B, [(0, 0, z) for z in DZ], DRA, DRB, sd(24), 'leather', sk, caps=(False, False), vrange=(1, 0), dense=dn(3, 2))
    zs = [1.02, .90, .76, .62, .50]
    ra = [x + .012 for x in (.19, .215, .245, .27, .285)]
    rb = [x + .012 for x in (.145, .17, .195, .215, .225)]
    arc_loft(B, [(0, 0, z) for z in zs], ra, rb, sd(18), 'cloth', sk, a0=34, a1=326, vrange=(1, 0))
    arc_loft(B, [(0, 0, z) for z in zs], [x - .01 for x in ra], [x - .01 for x in rb], sd(18), 'leather', sk, a0=34, a1=326, flip=True)
    if True:
        ring_leaves(B, .515, ra[-1] + .004, rb[-1] + .004, 12 if detail(0) else 9 if detail(1) else 7, .10, .045, .012, 'emblem', sk, a0=deg(34), regions=('emblem', 'cloth'))
    # bodice and rope belt
    zb = [.96, 1.04, 1.14, 1.26, 1.38, 1.46, 1.50]
    bra = [.185, .17, .16, .175, .215, .24, .12]
    brb = [.14, .125, .12, .13, .16, .165, .09]
    wt = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.08, .08), ('spine2', 1.20, .09), ('chest', 1.32, .09)])
    loft(B, [(0, 0, z) for z in zb], bra, brb, sd(16), 'leather', wt, vrange=(1, 0), dense=dn(3, 2))
    loft(B, [(0, 0, 1.00), (0, 0, 1.08)], [.20, .20], [.15, .15], sd(16), 'trim', 'pelvis')                                              # amber-banded rope belt
    if detail(1):
        for k in range(3):
            loft(B, [(0, 0, 1.012 + k * .026), (0, 0, 1.02 + k * .026)], [.203, .203], [.153, .153], sd(16), 'bone', 'pelvis', caps=(False, False))
        loft(B, [(.16, .03, 1.02), (.18, .05, .78), (.17, .05, .62)], [.012, .011, .008], [.012, .011, .008], 4, 'bone', 'pelvis')    # hanging rope end
        box(B, (.02, -.215, .90), (.09, .06, .10), 'plate', 'pelvis')                                                                 # herb pouch
        box(B, (.02, -.245, .91), (.03, .012, .05), 'trim', 'pelvis')
    # layered leaf mantle over the shoulders: three tiers of leaves
    cw = blend_chain(lambda p: p[2], [('pelvis', 0, 0), ('spine1', 1.0, .10), ('spine2', 1.18, .08), ('chest', 1.32, .06)])
    loft(B, [(0, 0, 1.54), (0, 0, 1.46), (0, 0, 1.38)], [.13, .26, .30], [.10, .18, .21], sd(14), 'cloth', 'chest', caps=(True, False), vrange=(1, 0), dense=dn(2))
    tiers = ((1.455, .262, .182, 11), (1.395, .292, .205, 12), (1.335, .310, .218, 13)) if detail(0) else ((1.40, .285, .20, 10),) if detail(1) else ((1.40, .285, .20, 8),)
    for (z, a_, b_, n_) in tiers:
        ring_leaves(B, z, a_, b_, n_, .11, .05, .012, 'emblem', 'chest', a0=0.2 * z, regions=('emblem', 'cloth', 'accent'))
    if detail(0):                                                                                                                        # a leaf cape down the back
        cloth_sheet(B, 1.34, .74, .20, .27, .20, .26, 4, 4, 'cloth', 'emblem', cw, skirt_w(.98, .85, .5, .09), wob=.02, jag=.05)
    # sleeves: bark brown with carved wooden bracers; bare hands
    for sg, s in SIDES:
        sh, el, wr, d = arm_frame(s)
        along = lambda p, sh=sh, d=d: float(np.dot(np.array(p) - sh, d))
        arm = blend_chain(along, [(f'upperarm.{s}', 0, 0), (f'forearm.{s}', .30, .08)])
        loft(B, [sh - d * .02, sh + d * .15, el, el + d * .15, wr + d * .02], [.060, .064, .060, .066, .072], [.060, .064, .060, .066, .072],
             sd(10), 'leather', arm, vrange=(1, 0), dense=dn(4, 2))
        loft(B, [el + d * .08, wr - d * .01], [.07, .062], [.07, .062], sd(8), 'plate', f'forearm.{s}')                                    # wooden bracer
        loft(B, [wr - d * .03, wr + d * .02], [.066, .066], [.066, .066], sd(8), 'trim', f'forearm.{s}', caps=(False, False))
        hc = wr + d * .07
        box(B, hc, (.062, .062, .10), 'skin', f'hand.{s}', rot=frame_matrix(d, (1, 0, 0), 2))
        if detail(0):
            for fi in range(3):
                box(B, hc + d * .06 + np.array((.02 * (fi - 1), 0, 0)), (.016, .016, .04), 'skin', f'hand.{s}', rot=frame_matrix(d, (1, 0, 0), 2))
    # boots, wrapped in leather and bark
    for sg, s in SIDES:
        kn = np.array(J[f'shin.{s}'][1])
        wf = lambda p, s=s: {f'foot.{s}': 1.0} if p[0] < .15 else {f'toe.{s}': 1.0}
        loft(B, [(.02, kn[1], .09), (.12, kn[1], .06), (.25, kn[1], .03)], [.06, .06, .04], [.07, .06, .03], sd(6), 'leather', wf, ref=(0, 0, 1))
        loft(B, [(kn[0], kn[1], .36), (kn[0], kn[1], .09)], [.058, .052], [.062, .056], sd(8), 'plate', f'shin.{s}')
        loft(B, [(kn[0], kn[1], .375), (kn[0], kn[1], .345)], [.064, .064], [.068, .068], sd(8), 'trim', f'shin.{s}')
    # head: tan face, long brown hair, a small leafy circlet with two short sprigs and an amber gem
    wh = blend_chain(lambda p: p[2], [('neck', 0, 0), ('head', 1.54, .03)])
    loft(B, [(0, 0, 1.46), (0, 0, 1.52)], [.07, .06], [.07, .06], sd(8), 'skin', wh)
    loft(B, [(0, 0, 1.52), (0, 0, 1.60), (0, 0, 1.67), (0, 0, 1.73)], [.084, .098, .096, .062], [.092, .106, .10, .062], sd(14), 'skin', wh, dense=dn(2))
    for sg in (1, -1):
        box(B, (.104, sg * .045, 1.625), (.012, .045, .026), 'dark', 'head')
        if detail(1):
            box(B, (.103, sg * .045, 1.652), (.012, .05, .008), 'leather', 'head', rot=rotx(sg * -8))
    box(B, (.112, 0, 1.585), (.022, .02, .05), 'skin', 'head')
    loft(B, [(-.005, 0, 1.655), (-.005, 0, 1.71), (-.02, 0, 1.765)], [.108, .106, .06], [.114, .11, .064], sd(12), 'emblem2', 'head', caps=(False, True), dense=dn(2))   # hair
    loft(B, [(-.06, 0, 1.69), (-.14, 0, 1.58), (-.17, 0, 1.40), (-.14, 0, 1.24)], [.10, .105, .075, .0], [.05, .06, .045, .0], sd(8), 'emblem2', 'head',
         caps=(True, False), ref=(0, 1, 0), dense=dn(2))                                                                                 # long hair down the back
    loft(B, [(0, 0, 1.678), (0, 0, 1.69)], [.113, .113], [.118, .118], sd(14), 'plate', 'head', caps=(False, False))                      # circlet ring of twig
    nl = 9 if detail(0) else 6 if detail(1) else 4
    for k in range(nl):
        a = 2 * math.pi * (k + .5) / nl
        if math.cos(a) < -.2 and k % 2:
            continue
        radial = unit((math.cos(a), math.sin(a), 0))
        leaf(B, (math.cos(a) * .115, math.sin(a) * .115, 1.688), radial * .6 + np.array((0, 0, .8)), .055, .02, .006, 'emblem' if k % 2 else 'cloth', 'head', radial=radial)
    box(B, (.122, 0, 1.684), (.012, .026, .03), 'glow', 'head')                                                                          # amber gem
    # gnarled staff in the right hand: crooked top cradling a glowing amber seed
    g = hand_grip(rig, M0, 'R')
    B.push(rig.to_bind(M0, 'weapon_socket'))
    ws = 'weapon_socket'
    ax = unit((.05, 0, 1.0))
    foot_pt = g - ax * (g[2] / ax[2] - .02)
    p1 = g + ax * ((1.30 - g[2]) / ax[2])
    pts = [foot_pt + (.0, .0, .0), foot_pt + (.015, .02, .38), g + (.012, -.02, .0), p1 + (-.012, .02, .0), p1 + (.015, .0, .30), p1 + (.07, -.015, .50),
           p1 + (.145, .0, .60), p1 + (.20, .015, .53)]
    gnarled(B, pts, .026, .014, 'plate', ws, knots=(.25, .55, .78))
    seed_c = p1 + (.09, .0, .43)
    loft(B, [seed_c + (0, 0, -.06), seed_c, seed_c + (0, 0, .07)], [0, .05, 0], [0, .05, 0], 6 if not K.LOD else 4, 'glow', ws, caps=(False, False), ref=(1, 0, 0))
    gnarled(B, [p1 + (.02, .0, .15), p1 + (-.06, .04, .30), p1 + (-.08, .08, .42)], .011, .004, 'plate', ws)                             # a side twig
    if detail(1):
        leaf(B, p1 + (-.08, .08, .42), (-.2, .4, .9), .07, .024, .006, 'emblem', ws, radial=(1, 0, 0))
        leaf(B, p1 + (-.055, .04, .30), (-.5, .2, .8), .06, .022, .006, 'cloth', ws, radial=(1, 0, 0))
        for off in (.04, .70):                                                                                                           # amber bindings
            loft(B, [g + ax * (off - .16 + .0), g + ax * (off - .13)], [.034, .034], [.034, .034], sd(8), 'trim', ws, caps=(False, False), ref=(0, 1, 0))
        rune_faces(B, g + ax * .45, (0, 1, 0), ax, ws, .04, .018, .0285)
    B.pop()
    return B, DRUID_ATLAS


DRUID_ATLAS = {
    'plate': ('bands', dict(base=(.50, .34, .18), line=(.80, .62, .36), spacing=48)),         # carved wood: bracers, staff
    'trim': ('scales', dict(a=(1.0, .76, .28), b=(.86, .56, .18))),                           # amber binding
    'cloth': ('folds', dict(base=(.40, .60, .22), period=30, hem=(.88, .78, .36), hem2=(.64, .44, .20))),   # moss green robe, golden hem
    'leather': ('weave', dict(base=(.44, .30, .17))),                                          # bark brown, repainted by paint_bark
    'blade': ('blade', dict(base=(.84, .88, .80))),
    'glow': ('flat', dict(base=(1.0, .74, .22))),                                             # amber
    'rune': ('rune', dict(bg=(.26, .14, .04), fg=(1.0, .76, .24))),
    'skin': ('flat', dict(base=(.80, .60, .44))),
    'bone': ('flat', dict(base=(.95, .90, .76))),
    'accent': ('flat', dict(base=(.78, .90, .46))),                                           # pale new leaf
    'dark': ('flat', dict(base=(.16, .14, .08))),
    'emblem': ('flat', dict(base=(.60, .80, .30))),                                           # lighter leaf, repainted by paint_leaf
    'emblem2': ('flat', dict(base=(.50, .32, .17))),                                          # brown hair
}


def paint_leaf(A, region, spec, seed):
    """Bright leaf: a central rib and slanted veins, darker toward the edge."""
    rng = np.random.default_rng(seed + 23)
    n = K.TILE
    nz = K.smooth_noise(rng, n, 16)
    yy, xx = np.mgrid[0:n, 0:n]
    u = (xx + .5) / n * 2 - 1
    t = np.ones((n, n, 4), np.float32)
    base = np.array(spec[region][1]['base'], np.float32)
    t[..., :3] = base * (.86 + .22 * nz[..., None]) * (1 - .22 * np.abs(u)[..., None])
    rib = np.abs(u) < .035
    veins = (np.abs(((yy / n * 6 + np.abs(u) * 2.2) % 1.0) - .5) < .06) & (np.abs(u) < .8)
    t[rib, :3] = (.92, .96, .60)
    t[veins & ~rib, :3] *= 1.18
    _tile(A, region, np.clip(t, 0, 1))


def paint_bark(A, region, spec, seed):
    """Bark: vertical furrows and a few dark knots."""
    rng = np.random.default_rng(seed + 29)
    n = K.TILE
    nz = K.smooth_noise(rng, n, 16)
    yy, xx = np.mgrid[0:n, 0:n]
    col = np.array(spec[region][1]['base'], np.float32)
    streak = np.repeat(rng.random(n // 4 + 1), 4)[:n][None, :] * np.ones((n, 1))
    t = np.ones((n, n, 4), np.float32)
    t[..., :3] = col * (.66 + .42 * streak[..., None]) * (.90 + .2 * nz[..., None])
    for _ in range(9):
        cx, cy, r = rng.integers(20, n - 20), rng.integers(20, n - 20), rng.integers(5, 10)
        t[np.hypot(xx - cx, yy - cy) < r, :3] *= .55
    _tile(A, region, np.clip(t, 0, 1))


CHARACTERS = {   # key: (builder, name, atlas seed)
    'sorcerer': (build_sorcerer, 'SK_Hero_Sorcerer', 1480),
    'warlock': (build_warlock, 'SK_Hero_Warlock', 1481),
    'druid': (build_druid, 'SK_Hero_Druid', 1482),
}
ASSET_IDS = {'sorcerer': 'sk-hero-sorcerer', 'warlock': 'sk-hero-warlock', 'druid': 'sk-hero-druid'}
BATCH3 = tuple(CHARACTERS)

PAINTERS = {   # key -> [(region, fn(A, region, spec, seed))]: marks painted over the base atlas
    'warlock': [('emblem2', paint_eye)],
    'druid': [('emblem', paint_leaf), ('leather', paint_bark)],
}


def make_atlas(key, spec, seed):
    A = C.make_atlas(key, spec, seed)
    for region, fn in PAINTERS.get(key, ()):
        fn(A, region, spec, seed)
    return A


C.CHARACTERS.update(CHARACTERS)
C.ASSET_IDS.update(ASSET_IDS)
