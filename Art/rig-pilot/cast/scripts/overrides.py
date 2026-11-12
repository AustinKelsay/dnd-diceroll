"""Per-class clip overrides (#150 batch 2 review): the ranger's bow draw and the barbarian's two-handed swing.

The shared clips cannot put a hand on a bow string or on a long haft for one character only, so a class may carry an override of one shared
clip on the same skeleton: `Ranged_Ranger` and `Melee_Barbarian`, exported in that class's LOD0 glb beside the ten shared clips and imported
as `A_Ranger_Ranged` and `A_Barbarian_Melee`. The theme's rig definition names them (`FDiceRollCharacter::Overrides`, data), and a class
with no entry plays the shared clip. The shared skeleton and the shared clips do not change.

Both are keyed in the frame counts of the clips they replace (Ranged 36, Melee 30), start and end on Idle frame 0 and are in place. A small
Gauss-Newton IK (numpy, over rigkit's FK) puts a hand where the prop is: the targets are written for the standing figure and ride on the chest
(M[chest] @ inv(M0[chest])), so they follow the body's twist and lunge.

* Ranger Ranged: the left arm raises the bow level at the target, the right hand goes to the string, draws to the cheek, holds, releases (the string snaps
  forward) and the arm drops. The string's nock vertex is skinned to hand.R (see cast_batch2.longbow), so the string visibly draws. The short blade is
  skinned to weapon_socket, and this clip scales that bone to 1/1000 while the bow is out, so the blade is gone from the hand.
* Barbarian Melee: the right hand carries the axe through a coil over the shoulder, a chop and a follow-through; the left hand is solved onto the haft
  0.2 m below it every frame (the haft rides on weapon_socket), so both hands swing it.
"""
import math

import numpy as np

import clips as CL
import rigkit as K
from clips import finish

HIDE = 0.001          # weapon_socket scale that hides the blade


def ease(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def grip(rig, M, side):
    h, t = rig.head(M, f'hand.{side}'), rig.tail(M, f'hand.{side}')
    return h + (t - h) * .6


def solve(rig, rot, loc, free, residual, init=None, iters=60, tol=3e-4):
    """Gauss-Newton on the (bone, axis) variables in `free` (degrees) so that residual(M) -> vector goes to zero. Returns (rot, |residual|)."""
    names = [b for b, _ in free]
    x = np.array([(init or rot).get(b, (0, 0, 0))[a] for b, a in free], float)
    lo = np.array([-135.0 if a == 0 else -175.0 if a == 1 else -100.0 for _, a in free])
    hi = np.array([135.0 if a == 0 else 45.0 if a == 1 else 100.0 for _, a in free])
    for i, (b, a) in enumerate(free):
        if b.startswith('forearm'):
            lo[i], hi[i] = -150.0, -3.0
        if b == 'weapon_socket':
            lo[i], hi[i] = -80.0, 120.0

    def pose_of(v):
        r = dict(rot)
        for b in set(names):
            cur = list(r.get(b, (0, 0, 0)))
            for k, (bb, a) in enumerate(free):
                if bb == b:
                    cur[a] = v[k]
            r[b] = tuple(cur)
        return r

    def res(v):
        return residual(rig.fk(*finish(pose_of(v), loc)))
    e = res(x)
    for _ in range(iters):
        if np.linalg.norm(e) < tol:
            break
        J = np.zeros((len(e), len(x)))
        for k in range(len(x)):
            d = np.zeros(len(x))
            d[k] = 1.0
            J[:, k] = res(x + d) - e
        step = np.linalg.solve(J.T @ J + 1e-3 * np.eye(len(x)), -J.T @ e)
        step = np.clip(step, -30, 30)
        for a in (1.0, .5, .25, .1):
            xn = np.clip(x + a * step, lo, hi)
            en = res(xn)
            if np.linalg.norm(en) < np.linalg.norm(e):
                x, e = xn, en
                break
        else:
            break
    return pose_of(x), float(np.linalg.norm(e))


def solve_best(rig, rot, loc, free, residual, init, starts=10, iters=50, tol=3e-4):
    """solve() from the given start and from a few seeded ones (arm configurations differ a lot between the coil and the chop); the best wins."""
    best = solve(rig, rot, loc, free, residual, init=init, iters=iters, tol=tol)
    if best[1] < tol * 3:
        return best
    rng = np.random.default_rng(7)
    for _ in range(starts):
        guess = {}
        for b, a in free:
            cur = list(guess.get(b, init.get(b, (0, 0, 0))))
            cur[a] = rng.uniform(-100, 60) if a == 1 else rng.uniform(-60, 80) if a == 0 else rng.uniform(-40, 90)
            if b.startswith('forearm'):
                cur[a] = rng.uniform(-140, -20)
            if b == 'weapon_socket':
                cur[a] = rng.uniform(-30, 90)
            guess[b] = tuple(cur)
        cand = solve(rig, rot, loc, free, residual, init=guess, iters=iters, tol=tol)
        if cand[1] < best[1]:
            best = cand
            if best[1] < tol * 3:
                break
    return best


class Frame:
    """Targets written for the standing figure, carried on the chest into the posed one."""

    def __init__(self, rig):
        self.rig = rig
        self.M0 = rig.fk(*CL.idle_at(0))

    def carry(self, M):
        T = M['chest'] @ np.linalg.inv(self.M0['chest'])
        return lambda p: (T @ np.append(np.asarray(p, float), 1.0))[:3]

    def carry_dir(self, M):
        T = M['chest'] @ np.linalg.inv(self.M0['chest'])
        return lambda d: T[:3, :3] @ np.asarray(d, float)


def lerp(a, b, t):
    return np.asarray(a, float) + (np.asarray(b, float) - np.asarray(a, float)) * t


# ------------------------------------------------------------------ ranger: Ranged (a bow draw)
RANGER_STANCE = {'pelvis': (0, 0, -6), 'spine1': (0, 0, -3), 'spine2': (0, 0, -3), 'chest': (0, 0, -3), 'head': (0, 0, 14),
                 'thigh.L': (0, -6, 0), 'shin.L': (0, 8, 0), 'thigh.R': (0, 6, 0), 'shin.R': (0, 10, 0), 'hand.L': (0, 0, 0), 'hand.R': (0, 0, 0)}
BOW_HAND = (0.52, -0.02, 1.50)        # standing-figure coordinates (x forward, y left, z up): where the bow hand holds the grip
STRING_REST = (0.32, -0.02, 1.52)     # the nock on the undrawn string, 0.2 m behind the grip
STRING_DRAWN = (0.17, -0.12, 1.52)    # the nock at full draw: the right cheek


def ranger_pose_fn(rig):
    """(pose_at(f), scale_at(f), info) for the ranger's Ranged override. pose_at returns (rot, loc)."""
    fr = Frame(rig)
    base = CL.idle_pose(0.0)[0]
    idleL = grip(rig, fr.M0, 'L')
    idleR = grip(rig, fr.M0, 'R')
    state = {'L': {'upperarm.L': (-6, -10, 0), 'forearm.L': (0, -50, 0)}, 'R': {'upperarm.R': (3, -8, 0), 'forearm.R': (0, -38, 0)}, 'worst': 0.0}

    def stance_weight(f):          # 0 at rest, 1 while the bow is up
        return ease(f / 8.0) if f < 8 else 1.0 if f <= 29 else 1.0 - ease((f - 29) / 7.0)

    def nock(f):
        if f <= 8:
            return lerp(idleR, STRING_REST, ease((f - 1) / 7.0)) if f >= 1 else idleR
        if f <= 20:
            return lerp(STRING_REST, STRING_DRAWN, ease((f - 8) / 12.0))
        if f <= 26:
            return np.asarray(STRING_DRAWN, float) + np.array((0, 0, .004 * math.sin((f - 20) * 2.3)))
        if f <= 29:
            return lerp(STRING_DRAWN, STRING_REST, ((f - 26) / 3.0) ** 2)      # the release: the string snaps forward
        if f <= 34:
            return lerp(STRING_REST, idleR, ease((f - 29) / 5.0))
        return idleR

    def bow_hand(f):
        t = ease((f - 1) / 7.0) if f < 8 else 1.0 if f <= 29 else 1.0 - ease((f - 29) / 5.0) if f < 34 else 0.0
        return lerp(idleL, BOW_HAND, t) if f >= 1 else idleL

    def pose_at(f):
        w = stance_weight(f)
        rot = {k: tuple(np.array(base.get(k, (0, 0, 0)), float) * (1 - w) + np.array(RANGER_STANCE.get(k, base.get(k, (0, 0, 0))), float) * w)
               for k in set(base) | set(RANGER_STANCE)}
        loc = {'pelvis': (0, 0, 0)}
        M = rig.fk(*finish(rot, loc))
        carry = fr.carry(M)
        tl, tr = carry(bow_hand(f)), carry(nock(f))
        rot, el = solve_best(rig, rot, loc, [('upperarm.L', 0), ('upperarm.L', 1), ('upperarm.L', 2), ('forearm.L', 1)],
                             lambda MM: grip(rig, MM, 'L') - tl, init=state['L'], iters=40)
        rot, er = solve_best(rig, rot, loc, [('upperarm.R', 0), ('upperarm.R', 1), ('upperarm.R', 2), ('forearm.R', 1)],
                             lambda MM: grip(rig, MM, 'R') - tr, init=state['R'], iters=40)
        state['L'] = {k: rot[k] for k in ('upperarm.L', 'forearm.L')}
        state['R'] = {k: rot[k] for k in ('upperarm.R', 'forearm.R')}
        state['worst'] = max(state['worst'], el, er)
        edge = ease((f - 30) / 6.0) if f > 30 else 0.0          # the last frames settle exactly on Idle frame 0
        if f <= 0 or edge > 0:
            idle = CL.idle_at(0)[0]
            wgt = 1.0 if f <= 0 else edge
            rot = {k: tuple(np.array(rot.get(k, (0, 0, 0)), float) * (1 - wgt) + np.array(idle.get(k, (0, 0, 0)), float) * wgt) for k in set(rot) | set(idle)}
        return finish(rot, loc if f > 0 and edge < 1.0 else CL.idle_at(0)[1])

    def scale_at(f):       # the blade vanishes while the bow is out: gone within two frames of the clip starting, back by the last frames
        if f <= 0 or f >= 35:
            return 1.0
        if f < 2:
            return 1.0 - (1.0 - HIDE) * ease(f / 2.0)
        if f <= 31:
            return HIDE
        return HIDE + (1.0 - HIDE) * ease((f - 31) / 4.0)
    return pose_at, scale_at, state


def ranger_hold_pose(rig):
    """The override's pose at full draw (frame 23): the bow is authored upright here and the string's nock sits at hand.R here."""
    pose_at, _, _ = ranger_pose_fn(rig)
    out = None
    for f in range(0, 24):
        out = pose_at(f)
    return out


# ------------------------------------------------------------------ barbarian: Melee (a two-handed swing)
HAFT_FROM_GRIP = .20          # metres from the right grip towards the butt: where the left hand goes
AX_IDLE = np.array((math.cos(math.radians(60)), 0, math.sin(math.radians(60))))

# keyed targets for the right hand (standing-figure coordinates) and the haft direction; the body keys are the shared Melee's.
SWING = [   # frame, right-hand point, haft direction (the axe head end), ease into the key
    (0, None, None, 'smooth'),
    (7, (0.02, -0.27, 1.62), (-0.45, -0.10, 0.88), 'smooth'),      # coil: the axe back over the right shoulder
    (11, (0.40, -0.10, 1.30), (0.80, 0.0, -0.20), 'in'),           # the chop
    (16, (0.42, -0.06, 1.18), (0.62, 0.0, -0.50), 'out'),          # follow through, the head low and ahead
    (24, None, None, 'smooth'),
]


def barbarian_pose_fn(rig, haft_point_idle):
    """(pose_at(f), info): both hands on the haft. haft_point_idle is the point on the haft (in the Idle pose) where the left hand goes."""
    fr = Frame(rig)
    body = CL.keyed([(f, o, l, e) for f, o, l, e in [
        (0, {}, None, 'smooth'),
        (7, {'pelvis': (0, -3, -8), 'spine1': (0, -3, -6), 'spine2': (0, -3, -6), 'chest': (0, -4, -6), 'head': (0, 0, 14),
             'thigh.L': (0, -10, 0), 'shin.L': (0, 16, 0), 'thigh.R': (0, 14, 0), 'shin.R': (0, 22, 0)}, {'pelvis': (-.05, 0, 0)}, 'smooth'),
        (11, {'pelvis': (0, 6, 10), 'spine1': (0, 8, 7), 'spine2': (0, 8, 7), 'chest': (0, 8, 6), 'head': (0, 4, -12),
              'thigh.L': (0, -42, 0), 'shin.L': (0, 44, 0), 'thigh.R': (0, 24, 0), 'shin.R': (0, 8, 0)}, {'pelvis': (.20, 0, 0)}, 'in'),
        (16, {'pelvis': (0, 5, 12), 'spine1': (0, 8, 8), 'spine2': (0, 8, 8), 'chest': (0, 8, 8), 'head': (0, 4, -12),
              'thigh.L': (0, -42, 0), 'shin.L': (0, 44, 0), 'thigh.R': (0, 24, 0), 'shin.R': (0, 8, 0)}, {'pelvis': (.21, 0, 0)}, 'out'),
        (24, {}, None, 'smooth'),
        (30, {}, None, 'smooth')]])
    idleR = grip(rig, fr.M0, 'R')
    idle_dir = AX_IDLE
    state = {'R': {'upperarm.R': (3, -8, 0), 'forearm.R': (0, -38, 0), 'hand.R': (0, 0, 0), 'weapon_socket': (0, 0, 0)},
             'L': {'upperarm.L': (-6, -10, 0), 'forearm.L': (0, -50, 0)}, 'worst': 0.0, 'worstR': 0.0}

    def swing_at(f):
        """right-hand point and haft direction at frame f (standing-figure coordinates)."""
        keys = [(k, p, d, e) for k, p, d, e in SWING]
        pts = [(k, np.array(p, float) if p is not None else idleR, np.array(d, float) if d is not None else idle_dir, e) for k, p, d, e in keys]
        for (f0, p0, d0, _), (f1, p1, d1, e) in zip(pts, pts[1:]):
            if f0 <= f <= f1:
                t = (f - f0) / (f1 - f0)
                t = t * t if e == 'in' else 1 - (1 - t) ** 2 if e == 'out' else ease(t)
                d = lerp(d0, d1, t)
                return lerp(p0, p1, t), d / np.linalg.norm(d)
        return idleR, idle_dir

    def pose_at(f):
        rot, loc = body(f)
        if f >= 24:       # the arms are back on Idle for the last frames (eased out of the follow-through)
            pass
        M = rig.fk(rot, loc)
        carry, carry_d = fr.carry(M), fr.carry_dir(M)
        pR, dR = swing_at(f)
        tp, td = carry(pR), carry_d(dR)
        td = td / np.linalg.norm(td)
        ws_inv0 = np.linalg.inv(fr.M0['weapon_socket'])

        def haft_dir(MM):
            return (MM['weapon_socket'] @ ws_inv0)[:3, :3] @ AX_IDLE

        def res_r(MM):
            return np.concatenate([grip(rig, MM, 'R') - tp, 0.12 * (haft_dir(MM) - td)])
        rot, er = solve_best(rig, rot, loc, [('upperarm.R', 0), ('upperarm.R', 1), ('upperarm.R', 2), ('forearm.R', 1), ('hand.R', 1), ('weapon_socket', 1)],
                             res_r, init=state['R'], iters=60)
        state['R'] = {k: rot[k] for k in ('upperarm.R', 'forearm.R', 'hand.R', 'weapon_socket')}
        MR = rig.fk(*finish(rot, loc))
        p = (MR['weapon_socket'] @ ws_inv0 @ np.append(haft_point_idle, 1.0))[:3]
        rot, el = solve_best(rig, rot, loc, [('upperarm.L', 0), ('upperarm.L', 1), ('upperarm.L', 2), ('forearm.L', 1)],
                             lambda MM: grip(rig, MM, 'L') - p, init=state['L'], iters=50)
        state['L'] = {k: rot[k] for k in ('upperarm.L', 'forearm.L')}
        state['worst'] = max(state['worst'], el)
        state['worstR'] = max(state['worstR'], er)
        # both arms ease in from, and back out to, Idle frame 0 (the clip starts and ends on it)
        w = ease(f / 5.0) if f < 5 else ease((30 - f) / 6.0) if f > 24 else 1.0
        idle = CL.idle_at(0)[0]
        for b in ('upperarm.R', 'forearm.R', 'hand.R', 'weapon_socket', 'upperarm.L', 'forearm.L', 'hand.L'):
            rot[b] = tuple(np.array(idle.get(b, (0, 0, 0)), float) * (1 - w) + np.array(rot.get(b, (0, 0, 0)), float) * w)
        return finish(rot, loc)
    return pose_at, state


# class key -> (shared clip name, action name, frames)
OVERRIDES = {'ranger': ('Ranged', 'Ranged_Ranger', 36), 'barbarian': ('Melee', 'Melee_Barbarian', 30)}
