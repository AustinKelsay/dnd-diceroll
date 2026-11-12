"""The tenth shared clip, Brace (#150, batch 2): a short weapon-up rally or battle cry for non-spell self and ally effects.

Importing this module registers 'Brace' in the r001 clip table (clips.CLIPS) without editing r001/scripts/clips.py, so the r001
build still makes its nine clips and every cast build, validation and sheet sees ten. Brace is 24 frames (0.8 s at 30 fps), is
not a loop, starts and ends on Idle frame 0, and is in place like every clip (the pelvis carries the motion, the root never moves).

Pose: a small coil (weight settles, the weapon arm drops back a touch), then the weapon arm snaps up and forward above the head
(the held item rises with it), the free fist closes at the chest, the chest opens and the head lifts for the shout, a short hold,
and back to Idle. Written with the same helpers as the other action clips; nothing about the nine existing clips changes.
"""
import clips as CL
from clips import keyed

BRACE_FRAMES = 24

BRACE = [
    (0, {}, None, 'smooth'),
    (5, {   # coil: knees give, shoulders roll forward, the weapon arm drops back
        'pelvis': (0, 4, 0), 'spine1': (0, 5, 0), 'spine2': (0, 5, 0), 'chest': (0, 5, 0), 'neck': (0, 3, 0), 'head': (0, 4, 0),
        'upperarm.R': (-4, 6, 0), 'forearm.R': (0, -26, 0), 'hand.R': (0, 6, 0),
        'upperarm.L': (8, -14, 0), 'forearm.L': (0, -64, 0),
        'thigh.L': (0, -16, 0), 'shin.L': (0, 28, 0), 'thigh.R': (0, -12, 0), 'shin.R': (0, 26, 0)},
     {'pelvis': (-.03, 0, 0)}, 'smooth'),
    (11, {  # rally: the weapon snaps up and forward, the chest opens, the head lifts, a wide stance
        'pelvis': (0, -3, 0), 'spine1': (0, -4, 0), 'spine2': (0, -6, 0), 'chest': (0, -8, 0), 'neck': (0, -5, 0), 'head': (0, -9, 0),
        'upperarm.R': (-24, -150, 0), 'forearm.R': (0, -22, 0), 'hand.R': (0, 10, 0),
        'upperarm.L': (26, -64, 0), 'forearm.L': (0, -104, 0), 'hand.L': (0, -10, 0),
        'thigh.L': (-4, -8, 0), 'shin.L': (0, 12, 0), 'thigh.R': (4, 8, 0), 'shin.R': (0, 14, 0)},
     {'pelvis': (.02, 0, 0)}, 'out'),
    (17, {  # hold, a little tremor of effort
        'pelvis': (0, -3, 0), 'spine1': (0, -4, 0), 'spine2': (0, -6, 0), 'chest': (0, -9, 0), 'neck': (0, -5, 0), 'head': (0, -10, 0),
        'upperarm.R': (-24, -153, 0), 'forearm.R': (0, -24, 0), 'hand.R': (0, 12, 0),
        'upperarm.L': (26, -66, 0), 'forearm.L': (0, -106, 0), 'hand.L': (0, -10, 0),
        'thigh.L': (-4, -8, 0), 'shin.L': (0, 12, 0), 'thigh.R': (4, 8, 0), 'shin.R': (0, 14, 0)},
     {'pelvis': (.02, 0, 0)}, 'lin'),
    (24, {}, None, 'smooth'),
]

CL.CLIPS['Brace'] = (BRACE_FRAMES, False, keyed(BRACE))
