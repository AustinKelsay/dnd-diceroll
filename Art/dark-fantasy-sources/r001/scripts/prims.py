"""Numpy primitives that mirror ps1-src mesh.py (CCW outward winding, world-scale UVs, top-down v)."""
import math

import numpy as np


class Soup:
    def __init__(self):
        self.P, self.UV, self.M = [], [], []

    def tri(self, mat, a, b, c, ua, ub, uc):
        self.P.append((a, b, c))
        self.UV.append((ua, ub, uc))
        self.M.append(mat)

    def quad(self, mat, a, b, c, d, ua=(0, 0), ub=(1, 0), uc=(1, 1), ud=(0, 1)):
        self.tri(mat, a, b, c, ua, ub, uc)
        self.tri(mat, a, c, d, ua, uc, ud)

    def grid(self, mat, o, du, dv, nu=1, nv=1, uv=(1, 1), uvo=(0, 0)):
        o, du, dv = np.array(o, float), np.array(du, float), np.array(dv, float)
        P = lambda i, j: tuple(o + du * i / nu + dv * j / nv)
        U = lambda i, j: (uvo[0] + uv[0] * i / nu, uvo[1] + uv[1] * j / nv)
        for i in range(nu):
            for j in range(nv):
                self.quad(mat, P(i, j), P(i + 1, j), P(i + 1, j + 1), P(i, j + 1), U(i, j), U(i + 1, j), U(i + 1, j + 1), U(i, j + 1))

    def box(self, mat, c, s, tile=1.0, top=None, bottom=True):
        cx, cy, cz = c
        sx, sy, sz = s[0] / 2, s[1] / 2, s[2] / 2
        fx, fy, fz = 2 * sx, 2 * sy, 2 * sz
        self.grid(mat, (cx - sx, cy - sy, cz - sz), (fx, 0, 0), (0, 0, fz), 1, 1, (fx / tile, fz / tile))
        self.grid(mat, (cx + sx, cy + sy, cz - sz), (-fx, 0, 0), (0, 0, fz), 1, 1, (fx / tile, fz / tile))
        self.grid(mat, (cx + sx, cy - sy, cz - sz), (0, fy, 0), (0, 0, fz), 1, 1, (fy / tile, fz / tile))
        self.grid(mat, (cx - sx, cy + sy, cz - sz), (0, -fy, 0), (0, 0, fz), 1, 1, (fy / tile, fz / tile))
        self.grid(top or mat, (cx - sx, cy - sy, cz + sz), (fx, 0, 0), (0, fy, 0), 1, 1, (fx / tile, fy / tile))
        if bottom:
            self.grid(mat, (cx - sx, cy + sy, cz - sz), (fx, 0, 0), (0, -fy, 0), 1, 1, (fx / tile, fy / tile))

    def loft(self, mat, rings, sides=6, patch=(0, 0, 1, 1), phase=0.0, cap=(True, True), jitter=None):
        """rings: (cx, cy, cz, rx, ry). u around, v along."""
        u0, v0, u1, v1 = patch
        n = len(rings)
        L = [0.0]
        for a, b in zip(rings, rings[1:]):
            L.append(L[-1] + math.dist(a[:3], b[:3]) + 1e-6)
        L = [x / L[-1] for x in L]

        def P(i, k):
            r = rings[i]
            a = math.radians(-90 + phase) + 2 * math.pi * k / sides
            j = 1.0 if jitter is None else jitter[i][k % sides]
            return (r[0] + r[3] * j * math.cos(a), r[1] + r[4] * j * math.sin(a), r[2])

        U = lambda k, t: (u0 + (u1 - u0) * k / sides, v0 + (v1 - v0) * t)
        for i in range(n - 1):
            for k in range(sides):
                self.quad(mat, P(i, k), P(i, k + 1), P(i + 1, k + 1), P(i + 1, k), U(k, L[i]), U(k + 1, L[i]), U(k + 1, L[i + 1]), U(k, L[i + 1]))
        if cap[0]:
            r = rings[0]
            c = (r[0], r[1], r[2])
            for k in range(sides):
                self.tri(mat, c, P(0, k + 1), P(0, k), (u0, v0), (u1, v0), (u0, v0))
        if cap[1]:
            r = rings[-1]
            c = (r[0], r[1], r[2])
            for k in range(sides):
                self.tri(mat, c, P(n - 1, k), P(n - 1, k + 1), (u0, v1), (u1, v1), (u0, v1))

    def lathe(self, mat, c, prof, sides=8, tile=None, patch=(0, 0, 1, 1), cap=(True, True), phase=0.0, jitter=None):
        rings = [(c[0], c[1], c[2] + z, r, r) for r, z in prof]
        if tile:
            circ = 2 * math.pi * max(r for r, _ in prof)
            H = abs(prof[-1][1] - prof[0][1])
            patch = (0, 0, max(1, round(circ / tile)), max(H / tile, .25))
        self.loft(mat, rings, sides, patch, phase, cap, jitter)

    def tube(self, mat, path, radii, sides=6, tile=None, patch=(0, 0, 1, 1), cap=(False, False)):
        path = [np.array(p, float) for p in path]
        n = len(path)
        frames = []
        for i in range(n):
            t = path[min(i + 1, n - 1)] - path[max(i - 1, 0)]
            t /= np.linalg.norm(t)
            ref = np.array((0, 0, 1.0)) if abs(t[2]) < .9 else np.array((1.0, 0, 0))
            a = np.cross(t, ref)
            a /= np.linalg.norm(a)
            frames.append((a, np.cross(a, t)))
        vs = [0.0]
        for a, b in zip(path, path[1:]):
            vs.append(vs[-1] + np.linalg.norm(b - a))
        u0, v0, u1, v1 = patch
        if tile:
            v1 = vs[-1] / tile

        def P(i, k):
            ang = 2 * math.pi * k / sides
            return tuple(path[i] + radii[i] * (math.cos(ang) * frames[i][0] - math.sin(ang) * frames[i][1]))

        U = lambda kk, ii: (u0 + (u1 - u0) * kk / sides, v0 + (v1 - v0) * vs[ii] / (vs[-1] or 1))
        for i in range(n - 1):
            for k in range(sides):
                self.quad(mat, P(i, k), P(i, k + 1), P(i + 1, k + 1), P(i + 1, k), U(k, i), U(k + 1, i), U(k + 1, i + 1), U(k, i + 1))
        if cap[0]:
            for k in range(sides):
                self.tri(mat, tuple(path[0]), P(0, k + 1), P(0, k), (u0, v0), (u1, v0), (u0, v0))
        if cap[1]:
            for k in range(sides):
                self.tri(mat, tuple(path[-1]), P(n - 1, k), P(n - 1, k + 1), (u0, v1), (u1, v1), (u0, v1))

    def soup(self):
        return dict(P=np.array(self.P, float), UV=np.array(self.UV, float), C=None, mat=np.array(self.M, dtype=object))


def rock(rng, c, r, squash=(1, 1, .7), detail=1):
    """Faceted boulder: perturbed icosphere. Returns (verts, faces)."""
    t = (1 + 5 ** .5) / 2
    V = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t), (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
    F = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
         (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    V = [np.array(v, float) / np.linalg.norm(v) for v in V]
    for _ in range(detail):
        mid = {}
        NF = []
        def m(a, b):
            k = (min(a, b), max(a, b))
            if k not in mid:
                p = V[a] + V[b]
                V.append(p / np.linalg.norm(p))
                mid[k] = len(V) - 1
            return mid[k]
        for a, b, cc in F:
            ab, bc, ca = m(a, b), m(b, cc), m(cc, a)
            NF += [(a, ab, ca), (b, bc, ab), (cc, ca, bc), (ab, bc, ca)]
        F = NF
    V = np.array(V) * (1 + rng.uniform(-.18, .18, (len(V), 1)))
    V = V * np.array(squash) * r + np.array(c)
    return V, F
