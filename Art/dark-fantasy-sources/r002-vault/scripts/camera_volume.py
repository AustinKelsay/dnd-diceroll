"""Camera envelope and clear-volume rules for the Root of the World r002 world (issue #149).

The battle camera (#143, Source/DnDDiceRoll/Public/Presentation/CameraRig.h) orbits the board centre on an arm of length
ArmLength x Distance x Scale, Scale = 150 / 110, pitch tied to the zoom (35 close, 45 default, 60 far).  The poses used here are
every pitch/zoom pair that law can produce for framed distances 1400..4600 and zoom 900..6400 (reference units), at 36 yaws,
plus the lead's design envelope (reach up to 55 m, height up to 66 m, pitch 35-60 degrees) so the world holds even if the zoom
range grows.  All distances are metres, the origin is the board centre at floor level, +X forward, Z up.

Two rules come out of it (both verified by validate_root_r002.py with a ray and a point-in-volume check):

  SIGHT VOLUME  = convex hull of the board footprint (plus a half-cell margin) lifted to z = 0.12 m, and every camera eye of the
                  envelope.  Every sight line from any eye to any floor cell lies inside it.  The world shell (SM_World_Root)
                  never enters it (with a 1 m margin), except the ground itself within 12 cm of the floor level.  Meshes that do
                  enter it are occluders (SM_World_Root_Occluders): the game dithers them away, they never block for good.
  EYE SPACE     = every eye of the envelope, as a 1.5 m sphere: no mesh at all (world or occluder) may contain an eye or come
                  closer than 1.5 m to one, so the camera is never inside geometry.
"""
import math

import numpy as np

ARM_LENGTH = 1.1768
SCALE = 150.0 / 110.0
FOV = 40.0
FX, FY = 13.5, 9.0                     # 18 x 12 cells at 1.5 m, half extents (the 16 x 11 board sits inside)
MARGIN = 0.75                          # half a cell of ground around the board
EYE_RADIUS = 1.5
MIN_D, MAX_D = 900.0, 6400.0
NEAR_D, FAR_D = 1300.0, 3000.0
NEAR_P, DEF_P, FAR_P = 35.0, 45.0, 60.0
MIN_F, MAX_F = 1400.0, 4600.0
FAR_RATIO = 1.4
BOARD_TOP = 31.0 * SCALE / 100.0        # the camera focus sits at the board top, about 42 cm above the floor
LEAD_ENVELOPE = dict(reach_m=55.0, height_m=66.0)


def far_stop(framed):
    return min(max(FAR_D, framed * FAR_RATIO), MAX_D)


def pitch_for(distance, framed):
    f = min(max(framed, MIN_F), MAX_F)
    if distance <= f:
        return NEAR_P + (DEF_P - NEAR_P) * min(max((distance - NEAR_D) / (f - NEAR_D), 0.0), 1.0)
    return DEF_P + (FAR_P - DEF_P) * min(max((distance - f) / (far_stop(f) - f), 0.0), 1.0)


def eye_curve():
    """(reach, height) pairs in metres above the focus: the arm law over every framed distance and zoom, plus the lead envelope."""
    pts = set()
    for framed in np.arange(MIN_F, MAX_F + 1, 400.0):
        for d in np.arange(MIN_D, MAX_D + 1, 100.0):
            p = math.radians(pitch_for(d, framed))
            arm = d * SCALE * ARM_LENGTH / 100.0
            pts.add((round(arm * math.cos(p), 3), round(arm * math.sin(p) + BOARD_TOP, 3)))
    L = LEAD_ENVELOPE
    for pitch in (35.0, 45.0, 60.0):
        r_max = min(L['reach_m'], L['height_m'] / math.tan(math.radians(pitch)))
        h_max = r_max * math.tan(math.radians(pitch))
        pts.add((round(r_max, 3), round(h_max, 3)))
        r_min = 12.0
        pts.add((r_min, round(r_min * math.tan(math.radians(pitch)), 3)))
    pts.add((L['reach_m'], L['reach_m'] * math.tan(math.radians(35.0))))
    pts.add((L['height_m'] / math.tan(math.radians(60.0)), L['height_m']))
    return sorted(pts)


def eye_positions(yaw_step=5.0):
    """Every eye (x, y, z): the curve revolved about the board centre."""
    out = []
    for r, z in eye_curve():
        for yaw in np.arange(0, 360, yaw_step):
            a = math.radians(yaw)
            out.append((r * math.cos(a), r * math.sin(a), z))
    return np.array(out)


def hull_points(yaw_step=10.0):
    pts = [tuple(p) for p in eye_positions(yaw_step)]
    zf = 0.12
    for sx in (-1, 1):
        for sy in (-1, 1):
            pts.append((sx * (FX + MARGIN), sy * (FY + MARGIN), zf))
    return np.array(pts)


def convex_hull_planes(points):
    """(planes (m,4) with outward normal n and offset d: inside when n.p <= d, triangles (verts, faces)) by gift wrapping through
    Blender's bmesh convex hull (numpy only otherwise)."""
    import bmesh
    bm = bmesh.new()
    for p in points:
        bm.verts.new(p)
    bmesh.ops.convex_hull(bm, input=bm.verts[:], use_existing_faces=False)
    bm.verts.ensure_lookup_table()
    verts, faces = [], []
    vmap = {}
    for f in bm.faces:
        idx = []
        for v in f.verts:
            if v.index not in vmap:
                vmap[v.index] = len(verts)
                verts.append(tuple(v.co))
            idx.append(vmap[v.index])
        for k in range(1, len(idx) - 1):
            faces.append((idx[0], idx[k], idx[k + 1]))
    bm.free()
    V = np.array(verts)
    F = np.array(faces)
    c = V.mean(0)
    n = np.cross(V[F[:, 1]] - V[F[:, 0]], V[F[:, 2]] - V[F[:, 0]])
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    d = np.einsum('ij,ij->i', n, V[F[:, 0]])
    flip = (n @ c - d) > 0                 # normals must point away from the centre
    n[flip] *= -1
    d[flip] *= -1
    return np.concatenate([n, d[:, None]], 1), (V, F)


class Volume:
    """Sight hull with a margin; contains(p) is a numpy point-in-hull test."""

    def __init__(self, margin=1.5):
        pts = hull_points()
        self.planes, (self.V, self.F) = convex_hull_planes(pts)
        self.margin = margin

    def depth(self, P):
        """Signed distance to the hull boundary: positive inside (min over planes of d - n.p), metres."""
        P = np.asarray(P, float).reshape(-1, 3)
        s = self.planes[:, 3][None, :] - P @ self.planes[:, :3].T
        return s.min(1)

    def contains(self, P, margin=None):
        m = self.margin if margin is None else margin
        return self.depth(P) > -m

    def z_interval(self, X, Y, margin=None):
        """Vertical extent of the (inflated) hull above map points: (zlo, zhi) arrays, zlo = +inf where the vertical line misses it."""
        m = self.margin if margin is None else margin
        X = np.asarray(X, float).ravel()
        Y = np.asarray(Y, float).ravel()
        nx, ny, nz, d = (self.planes[:, i][None, :] for i in range(4))
        up = nz > 1e-9
        dn = nz < -1e-9
        flat = ~(up | dn)
        safe = np.where(up | dn, nz, 1.0)
        ZLO, ZHI = np.empty(len(X)), np.empty(len(X))
        for a in range(0, len(X), 2000):
            xs, ys = X[a:a + 2000, None], Y[a:a + 2000, None]
            rhs = d + m - nx * xs - ny * ys                                   # nz*z <= rhs
            zhi = np.where(up, rhs / safe, np.inf).min(1)
            zlo = np.where(dn, rhs / safe, -np.inf).max(1)
            bad = (flat & (rhs < 0)).any(1)
            empty = bad | (zlo > zhi)
            ZLO[a:a + 2000] = np.where(empty, np.inf, np.maximum(zlo, -1e9))
            ZHI[a:a + 2000] = np.where(empty, -np.inf, zhi)
        return ZLO, ZHI

    def lower_surface_height(self, d_out, ang_deg=0.0):
        """Height of the hull's lower surface above the floor at a horizontal distance (m) from the board centre along a yaw."""
        a = math.radians(ang_deg)
        x, y = d_out * math.cos(a), d_out * math.sin(a)
        zs = np.arange(0.0, 120.0, 0.05)
        pts = np.stack([np.full_like(zs, x), np.full_like(zs, y), zs], 1)
        inside = self.depth(pts) > -self.margin
        return float(zs[inside.argmax()]) if inside.any() else float('inf')


if __name__ == '__main__':
    import sys
    import os
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    v = Volume()
    print('eye curve points', len(eye_curve()), 'hull faces', len(v.F), 'hull verts', len(v.V))
    for d in (14, 16, 18, 20, 24, 30, 40, 50, 56, 60, 70):
        print(f'along +X, centre distance {d:3d} m: lowest hull height {v.lower_surface_height(d, 0):6.2f} m   along +Y: {v.lower_surface_height(d, 90):6.2f} m   diagonal: {v.lower_surface_height(d, 35):6.2f} m')
    print('hull z range', v.V[:, 2].min(), v.V[:, 2].max(), 'max radius', np.hypot(v.V[:, 0], v.V[:, 1]).max())
