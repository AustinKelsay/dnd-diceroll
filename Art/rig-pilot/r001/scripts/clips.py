"""Hand-keyed clips for the shared humanoid skeleton (no external motion data). Pure numpy, no bpy.

Poses are dicts bone -> (rx, ry, rz) degrees about the model axes (X forward, Y left, Z up), applied in the parent's
posed frame. For a limb that hangs down in the rest pose, negative ry swings it forward, positive rx swings it to the
left (so use negative rx to abduct the right arm). For a bone that points up (spine, neck, head) positive ry leans
forward, positive rx leans right and positive rz turns left. Feet are levelled automatically (world-flat) unless a pose
names them. The pelvis may move; the root never does. Pelvis height is solved per frame against the floor (rigkit.ground_offset).

Clip lengths at 30 fps (frames; a looping clip repeats its first pose on the last frame):
  Idle 60 (2.0 s, loop)   Walk 30 (1.0 s, loop)   Melee 30   Ranged 36   Cast 42   Hit 15
  Downed 72 (collapse over 48 frames, held to 72)   Death 78 (held from 54)   Recover 66 (downed pose to idle)
"""
import math

from rigkit import Rig, blend_pose, sample_keys

LEGS = ('L', 'R')


def idle_pose(t):
    """t in [0,1). Breathing, a slow weight shift, sword arm resting, shield arm braced."""
    p = 2 * math.pi * t
    s, c = math.sin(p), math.cos(p)
    rot = {
        'pelvis': (1.2 * s, 0, 1.5 * s),
        'spine1': (-.6 * s, 1.0 * c, -1.0 * s),
        'spine2': (-.4 * s, 1.5 * s, -.8 * s),
        'chest': (-.4 * s, 1.8 * s, 0),
        'neck': (0, -1.0 * s, 1.5 * s),
        'head': (0, 1.0 * s, -1.5 * s),
        'upperarm.R': (3 + 1.0 * s, -8 - 1.5 * s, 0),
        'forearm.R': (0, -38 - 2 * s, 0),
        'hand.R': (0, 6 * s, 0),
        'upperarm.L': (-6 - 1.0 * s, -10 - 1.0 * s, 0),
        'forearm.L': (0, -50 - 1.5 * s, 0),
        'thigh.L': (-3, -3 + 1.0 * s, 0), 'shin.L': (0, 6 - 1.0 * s, 0),
        'thigh.R': (3, 2 - 1.0 * s, 0), 'shin.R': (0, 5 + 1.0 * s, 0),
    }
    return rot, {'pelvis': (0, .008 * s, 0)}


def walk_pose(t):
    p = 2 * math.pi * t
    s, c = math.sin(p), math.cos(p)
    rot = {}
    for sg, bone in ((1, 'L'), (-1, 'R')):
        ph = p if sg == 1 else p + math.pi
        sw, cw = math.sin(ph), math.cos(ph)
        thigh = -28 * sw
        knee = 8 + 48 * max(0.0, cw) ** 1.3
        rot[f'thigh.{bone}'] = (3 * sg * cw, thigh - 4, 0)
        rot[f'shin.{bone}'] = (0, knee, 0)
        rot[f'toe.{bone}'] = (0, 12 * max(0.0, -cw) if sw < 0 else 0.0, 0)
    rot['pelvis'] = (2 * c, 0, 7 * s)
    rot['spine1'] = (-1.5 * c, 2.5, -3 * s)
    rot['spine2'] = (-1 * c, 1.5, -3 * s)
    rot['chest'] = (-1 * c, 1.0, -1.5 * s)
    rot['neck'] = (0, -1, 3 * s)
    rot['head'] = (0, -1, 2 * s)
    rot['upperarm.R'] = (3, -10 - 12 * s, 0)
    rot['forearm.R'] = (0, -42 - 6 * s, 0)
    rot['hand.R'] = (0, 4 * s, 0)
    rot['upperarm.L'] = (-6, -8 + 10 * s, 0)
    rot['forearm.L'] = (0, -52 + 3 * s, 0)
    return rot, {'pelvis': (0, .022 * s, 0)}


def finish(rot, loc):
    """Auto-level the feet to the world unless a pose set them."""
    rot = dict(rot)
    for s in LEGS:
        if f'foot.{s}' not in rot:
            ry = -(rot.get('pelvis', (0, 0, 0))[1] + rot.get(f'thigh.{s}', (0, 0, 0))[1] + rot.get(f'shin.{s}', (0, 0, 0))[1])
            rot[f'foot.{s}'] = (0, ry, 0)
    return rot, dict(loc)


def idle_at(f, n=60):
    return finish(*idle_pose((f / n) % 1.0))


def walk_at(f, n=30):
    return finish(*walk_pose((f / n) % 1.0))


BASE = idle_pose(0.0)


def P(over=None, loc=None, base=BASE):
    """A full pose: BASE with the given bone overrides (and pelvis offset), feet levelled."""
    rot = dict(base[0])
    rot.update(over or {})
    lc = dict(base[1])
    lc.update(loc or {})
    return finish(rot, lc)


def keyed(keys):
    """keys: [(frame, over_dict, loc_dict_or_None, ease_into_key)] -> pose_at(f)."""
    compiled = [(f, P(o, l), e) for f, o, l, e in keys]
    return lambda f: sample_keys(compiled, f)


# ------------------------------------------------------------------ action clips
# arm angles note: blade/ staff elevation in the world = 16 - (sum of ry down the right arm chain) degrees for the held items.
MELEE = [
    (0, {}, None, 'smooth'),
    (7, {   # coil: weight back, sword hand cocked beside the head, shield forward
        'pelvis': (0, -3, -8), 'spine1': (0, -3, -6), 'spine2': (0, -3, -6), 'chest': (0, -4, -6), 'head': (0, 0, 14),
        'upperarm.R': (-30, -105, 0), 'forearm.R': (0, -70, 0), 'hand.R': (0, -10, 0),
        'upperarm.L': (14, -45, 0), 'forearm.L': (0, -75, 0),
        'thigh.L': (0, -10, 0), 'shin.L': (0, 16, 0), 'thigh.R': (0, 14, 0), 'shin.R': (0, 22, 0)},
     {'pelvis': (-.05, 0, 0)}, 'smooth'),
    (11, {   # strike: lunge forward, chop down across the body
        'pelvis': (0, 6, 10), 'spine1': (0, 8, 7), 'spine2': (0, 8, 7), 'chest': (0, 8, 6), 'head': (0, 4, -12),
        'upperarm.R': (-8, -84, 0), 'forearm.R': (0, -12, 0), 'hand.R': (0, 30, 0),
        'upperarm.L': (18, -30, 0), 'forearm.L': (0, -65, 0),
        'thigh.L': (0, -42, 0), 'shin.L': (0, 44, 0), 'thigh.R': (0, 24, 0), 'shin.R': (0, 8, 0)},
     {'pelvis': (.20, 0, 0)}, 'in'),
    (16, {   # follow through
        'pelvis': (0, 5, 12), 'spine1': (0, 8, 8), 'spine2': (0, 8, 8), 'chest': (0, 8, 8), 'head': (0, 4, -12),
        'upperarm.R': (10, -42, 0), 'forearm.R': (0, -8, 0), 'hand.R': (0, 34, 0),
        'upperarm.L': (18, -34, 0), 'forearm.L': (0, -65, 0),
        'thigh.L': (0, -42, 0), 'shin.L': (0, 44, 0), 'thigh.R': (0, 24, 0), 'shin.R': (0, 8, 0)},
     {'pelvis': (.21, 0, 0)}, 'out'),
    (30, {}, None, 'smooth'),
]

RANGED = [   # draw a bow or sling, hold, loose
    (0, {}, None, 'smooth'),
    (10, {   # side-on draw: left arm points at the target, right hand comes back to the cheek
        'pelvis': (0, 0, -26), 'spine1': (0, 0, -8), 'spine2': (0, 0, -8), 'chest': (0, 0, -8), 'head': (0, 0, 34),
        'upperarm.L': (4, -92, 0), 'forearm.L': (0, -4, 0), 'hand.L': (0, 0, 0),
        'upperarm.R': (-60, -78, 0), 'forearm.R': (0, -128, 0), 'hand.R': (0, 10, 0), 'weapon_socket': (0, 110, 0),
        'thigh.L': (0, -6, 0), 'shin.L': (0, 8, 0), 'thigh.R': (0, 6, 0), 'shin.R': (0, 10, 0)},
     {'pelvis': (-.02, 0, 0)}, 'smooth'),
    (22, {   # hold, a touch of tremor
        'pelvis': (0, 0, -27), 'spine1': (0, 0, -8), 'spine2': (0, 0, -8), 'chest': (0, 0, -9), 'head': (0, 0, 34),
        'upperarm.L': (5, -94, 0), 'forearm.L': (0, -3, 0),
        'upperarm.R': (-62, -80, 0), 'forearm.R': (0, -132, 0), 'hand.R': (0, 10, 0), 'weapon_socket': (0, 110, 0),
        'thigh.L': (0, -6, 0), 'shin.L': (0, 8, 0), 'thigh.R': (0, 6, 0), 'shin.R': (0, 10, 0)},
     {'pelvis': (-.02, 0, 0)}, 'lin'),
    (26, {   # release: right elbow snaps back, shoulders open
        'pelvis': (0, 0, -22), 'spine1': (0, -2, -6), 'spine2': (0, -2, -8), 'chest': (0, -2, -12), 'head': (0, 0, 30),
        'upperarm.L': (4, -92, 0), 'forearm.L': (0, -2, 0),
        'upperarm.R': (-74, -62, 0), 'forearm.R': (0, -100, 0), 'hand.R': (0, 0, 0), 'weapon_socket': (0, 95, 0),
        'thigh.L': (0, -6, 0), 'shin.L': (0, 8, 0), 'thigh.R': (0, 6, 0), 'shin.R': (0, 10, 0)},
     {'pelvis': (-.03, 0, 0)}, 'in'),
    (36, {}, None, 'smooth'),
]

CAST = [   # gather, raise the focus, thrust
    (0, {}, None, 'smooth'),
    (12, {   # arms sweep up and out, chest opens, head lifts
        'pelvis': (0, -2, 0), 'spine1': (0, -4, 0), 'spine2': (0, -5, 0), 'chest': (0, -7, 0), 'neck': (0, -4, 0), 'head': (0, -6, 0),
        'upperarm.R': (-50, -40, 0), 'forearm.R': (0, -62, 0), 'hand.R': (0, 30, 0), 'weapon_socket': (0, 40, 0),
        'upperarm.L': (50, -96, 0), 'forearm.L': (0, -24, 0), 'hand.L': (0, -10, 0),
        'thigh.L': (0, -4, 0), 'shin.L': (0, 8, 0), 'thigh.R': (0, 6, 0), 'shin.R': (0, 10, 0)},
     {'pelvis': (-.04, 0, 0)}, 'smooth'),
    (22, {   # thrust forward: both hands push, staff points at the target
        'pelvis': (0, 6, 0), 'spine1': (0, 7, 0), 'spine2': (0, 8, 0), 'chest': (0, 9, 0), 'neck': (0, 2, 0), 'head': (0, 4, 0),
        'upperarm.R': (-14, -95, 0), 'forearm.R': (0, -6, 0), 'hand.R': (0, 20, 0), 'weapon_socket': (0, 70, 0),
        'upperarm.L': (8, -100, 0), 'forearm.L': (0, -6, 0), 'hand.L': (0, -16, 0),
        'thigh.L': (0, -22, 0), 'shin.L': (0, 24, 0), 'thigh.R': (0, 14, 0), 'shin.R': (0, 10, 0)},
     {'pelvis': (.08, 0, 0)}, 'in'),
    (30, {   # hold, the hands shiver
        'pelvis': (0, 6, 0), 'spine1': (0, 7, 0), 'spine2': (0, 8, 0), 'chest': (0, 9, 0), 'neck': (0, 2, 0), 'head': (0, 4, 0),
        'upperarm.R': (-14, -97, 0), 'forearm.R': (0, -6, 0), 'hand.R': (0, 20, 0), 'weapon_socket': (0, 74, 0),
        'upperarm.L': (8, -102, 0), 'forearm.L': (0, -8, 0), 'hand.L': (0, -16, 0),
        'thigh.L': (0, -22, 0), 'shin.L': (0, 24, 0), 'thigh.R': (0, 14, 0), 'shin.R': (0, 10, 0)},
     {'pelvis': (.08, 0, 0)}, 'lin'),
    (42, {}, None, 'smooth'),
]

HIT = [   # flinch: chest and head snap back, arms fling out, recover
    (0, {}, None, 'smooth'),
    (3, {'pelvis': (0, -3, 3), 'spine1': (0, -5, 3), 'spine2': (0, -6, 3), 'chest': (0, -7, 3), 'neck': (0, -6, 0), 'head': (0, -10, -5),
         'upperarm.R': (-26, -4, 0), 'forearm.R': (0, -26, 0), 'upperarm.L': (30, 6, 0), 'forearm.L': (0, -36, 0),
         'thigh.L': (0, -4, 0), 'shin.L': (0, 16, 0), 'thigh.R': (0, 10, 0), 'shin.R': (0, 20, 0)},
     {'pelvis': (-.08, 0, 0)}, 'out'),
    (8, {'pelvis': (0, -2, 1), 'spine1': (0, -3, 2), 'spine2': (0, -4, 2), 'chest': (0, -4, 2), 'neck': (0, -3, 0), 'head': (0, -5, -2)},
     {'pelvis': (-.03, 0, 0)}, 'smooth'),
    (15, {}, None, 'smooth'),
]

KNEEL = {   # upright kneel, torso slumping, head down
    'pelvis': (0, 12, 0), 'spine1': (0, 18, 0), 'spine2': (0, 22, 0), 'chest': (0, 22, 0), 'neck': (0, 20, 0), 'head': (0, 24, 0),
    'upperarm.R': (-10, 8, 0), 'forearm.R': (0, -16, 0), 'hand.R': (0, 0, 0),
    'upperarm.L': (10, 8, 0), 'forearm.L': (0, -16, 0), 'hand.L': (0, 0, 0),
    'thigh.L': (-6, -14, 0), 'shin.L': (0, 90, 0), 'foot.L': (0, 92, 0),
    'thigh.R': (6, -14, 0), 'shin.R': (0, 90, 0), 'foot.R': (0, 92, 0)}

DOWNED = {   # held pose: slumped forward over the knees, hands on the ground, head hanging
    'pelvis': (0, 24, 0), 'spine1': (0, 26, 0), 'spine2': (0, 28, 0), 'chest': (0, 26, 0), 'neck': (0, 22, 0), 'head': (0, 20, 0),
    'upperarm.R': (-14, -46, 0), 'forearm.R': (0, -12, 0), 'hand.R': (0, -4, 0),
    'upperarm.L': (14, -46, 0), 'forearm.L': (0, -12, 0), 'hand.L': (0, -4, 0),
    'thigh.L': (-10, -30, 0), 'shin.L': (0, 96, 0), 'foot.L': (0, 90, 0),
    'thigh.R': (10, -30, 0), 'shin.R': (0, 96, 0), 'foot.R': (0, 90, 0), 'weapon_socket': (0, 70, 0)}

DOWNED_CLIP = [
    (0, {}, None, 'smooth'),
    (8, {'pelvis': (0, -4, 0), 'spine1': (0, -6, 0), 'spine2': (0, -8, 0), 'chest': (0, -8, 0), 'head': (0, -10, 0),
         'upperarm.R': (-30, -6, 0), 'upperarm.L': (34, 0, 0),
         'thigh.L': (0, -6, 0), 'shin.L': (0, 22, 0), 'thigh.R': (0, 8, 0), 'shin.R': (0, 26, 0)},
     {'pelvis': (-.05, 0, 0)}, 'out'),
    (24, KNEEL, {'pelvis': (-.14, 0, 0)}, 'in'),
    (40, DOWNED, {'pelvis': (-.20, 0, 0)}, 'out'),
    (48, DOWNED, {'pelvis': (-.20, 0, 0)}, 'lin'),
    (72, DOWNED, {'pelvis': (-.20, 0, 0)}, 'lin'),
]

DEAD = {   # flat on the back, head toward -X, arms thrown out, one knee up
    'pelvis': (0, -90, 0), 'spine1': (0, 0, 0), 'spine2': (0, 2, 0), 'chest': (0, 2, 0), 'neck': (0, 4, 0), 'head': (0, 10, 12),
    'upperarm.R': (-40, 20, 0), 'forearm.R': (0, -10, 0), 'hand.R': (0, 0, 0),
    'upperarm.L': (50, 24, 0), 'forearm.L': (0, -6, 0), 'hand.L': (0, 0, 0),
    'thigh.L': (-4, -6, 0), 'shin.L': (0, 8, 0), 'foot.L': (0, 80, 0),
    'thigh.R': (14, -24, 0), 'shin.R': (0, 70, 0), 'foot.R': (0, 20, 0)}

DEATH = [
    (0, {}, None, 'smooth'),
    (6, {'pelvis': (0, -4, 0), 'spine1': (0, -8, 0), 'spine2': (0, -10, 0), 'chest': (0, -12, 0), 'neck': (0, -10, 0), 'head': (0, -16, 0),
         'upperarm.R': (-36, -2, 0), 'forearm.R': (0, -22, 0), 'upperarm.L': (40, 4, 0), 'forearm.L': (0, -30, 0),
         'thigh.L': (0, -8, 0), 'shin.L': (0, 26, 0), 'thigh.R': (0, 12, 0), 'shin.R': (0, 30, 0)},
     {'pelvis': (-.08, 0, 0)}, 'out'),
    (20, {'pelvis': (0, -30, 0), 'spine1': (0, -10, 0), 'spine2': (0, -10, 0), 'chest': (0, -8, 0), 'neck': (0, -6, 0), 'head': (0, -10, 0),
          'upperarm.R': (-50, 10, 0), 'forearm.R': (0, -16, 0), 'upperarm.L': (56, 14, 0), 'forearm.L': (0, -16, 0),
          'thigh.L': (0, -50, 0), 'shin.L': (0, 80, 0), 'thigh.R': (0, 4, 0), 'shin.R': (0, 90, 0)},
     {'pelvis': (-.12, 0, 0)}, 'in'),
    (38, {'pelvis': (0, -72, 0), 'spine1': (0, -2, 0), 'spine2': (0, 0, 0), 'chest': (0, 0, 0), 'neck': (0, 0, 0), 'head': (0, 6, 4),
          'upperarm.R': (-50, 16, 0), 'forearm.R': (0, -14, 0), 'upperarm.L': (56, 18, 0), 'forearm.L': (0, -10, 0),
          'thigh.L': (-6, -20, 0), 'shin.L': (0, 40, 0), 'thigh.R': (10, -8, 0), 'shin.R': (0, 60, 0)},
     {'pelvis': (-.08, 0, 0)}, 'in'),
    (54, DEAD, {'pelvis': (-.04, 0, 0)}, 'out'),
    (78, DEAD, {'pelvis': (-.04, 0, 0)}, 'lin'),
]

RECOVER = [   # from the downed pose: push up, kneel, rise stooped, stand
    (0, DOWNED, {'pelvis': (-.20, 0, 0)}, 'smooth'),
    (14, {**DOWNED, 'pelvis': (0, 30, 0), 'spine1': (0, 30, 0), 'spine2': (0, 24, 0), 'chest': (0, 18, 0), 'head': (0, 6, 0),
          'upperarm.R': (-12, -62, 0), 'upperarm.L': (12, -62, 0)}, {'pelvis': (-.16, 0, 0)}, 'smooth'),
    (30, KNEEL, {'pelvis': (-.14, 0, 0)}, 'smooth'),
    (46, {'pelvis': (0, 20, 0), 'spine1': (0, 18, 0), 'spine2': (0, 14, 0), 'chest': (0, 12, 0), 'neck': (0, 8, 0), 'head': (0, 4, 0),
          'upperarm.R': (-10, 4, 0), 'forearm.R': (0, -34, 0), 'upperarm.L': (10, 4, 0), 'forearm.L': (0, -40, 0),
          'thigh.L': (0, -66, 0), 'shin.L': (0, 80, 0), 'thigh.R': (0, 12, 0), 'shin.R': (0, 120, 0), 'foot.R': (0, 50, 0)},
     {'pelvis': (.05, 0, 0)}, 'smooth'),
    (66, {}, None, 'out'),
]

CLIPS = {   # name: (frames, loop, pose_at)
    'Idle': (60, True, idle_at),
    'Walk': (30, True, walk_at),
    'Melee': (30, False, keyed(MELEE)),
    'Ranged': (36, False, keyed(RANGED)),
    'Cast': (42, False, keyed(CAST)),
    'Hit': (15, False, keyed(HIT)),
    'Downed': (72, False, keyed(DOWNED_CLIP)),
    'Death': (78, False, keyed(DEATH)),
    'Recover': (66, False, keyed(RECOVER)),
}
