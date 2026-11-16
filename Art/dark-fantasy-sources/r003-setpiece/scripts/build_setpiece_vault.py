"""The Waystone set piece, r001 (issue #250, rescoping #104): the rune seal that guards the artifact in the Vault apse and shatters when
the finale is won.

Run from the repository root (one Blender process, a few seconds):
    blender -b --python Art/dark-fantasy-sources/r003-setpiece/scripts/build_setpiece_vault.py

The seal is six basalt rune shards floating in a ring around the artifact. Each shard is authored as two halves that meet on one jagged
crack (the same crack function on both halves, so they mate exactly when placed at the same transform: the pivot of both halves is the
centre of the crack plane). When the finale is won the game flies the halves and a few chips along pre-authored arcs onto the dais (no
physics: ExpeditionBattleView plays them from the table in WaystoneSetPiece.cpp). Three meshes:

  SM_SetPiece_Vault_SealUpper  the upper half: crack plane at z = 0, pointed tip up (0.30 m)
  SM_SetPiece_Vault_SealLower  the lower half: crack plane at z = 0, pointed tip down (0.20 m)
  SM_SetPiece_Vault_Chip       a faceted basalt chip (about 10 cm) with a rune fleck; pivot at its centre

Same conventions and finishing as the Vault kit r002 (build_kit_vault_r002.py, whose libraries this imports unchanged): metres, +X
forward, Z up, glTF Y-up export, one small atlas per glb, COLOR_0 RGBA with RGB = ambient occlusion x height gradient and A = glow mask
(1 on the cold-blue rune material, 0 elsewhere). The rune face of a shard faces +X; the game turns it outward. All geometry and textures
are procedural; no outside content.
Outputs  Art/<asset-id>/r001/<SM_name>.glb  and  Art/dark-fantasy-sources/r003-setpiece/setpiece_metrics.json
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
VAULT = os.path.abspath(os.path.join(HERE, '..', '..', 'r002-vault', 'scripts'))
sys.path.insert(0, VAULT)
import r002_lib as R
from r002_lib import L, bpy, np, make_bvh, tri_normals, bake_ao
import vault_lib as VL

from prims import Soup, rock

ROOT = R.ROOT
OUT_SRC = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r003-setpiece')
PAD = 8
GLOW = ('rune',)
METRICS = {}

# The shard's cross-section: a hexagon whose flat face looks down +X (loft phase 0 puts vertices at -90 + 60k degrees, so the face
# between -30 and +30 degrees is flat and centred on +X).
SIDES = 6
UPPER_RINGS = [(0.0, .085), (.14, .08), (.22, .058), (.30, .008)]
LOWER_RINGS = [(-.20, .010), (-.12, .07), (0.0, .085)]
RUNE_X = .085 * math.cos(math.radians(30)) - .002           # just proud of the flat +X face


def crack(x, y):
    """Height of the shared crack surface at (x, y): a fixed jagged ridge, identical for both halves so they mate exactly."""
    a = math.atan2(y, x)
    r = math.hypot(x, y) / .085
    return .022 * r * (math.sin(3 * a + .6) * .7 + math.sin(5 * a + 1.9) * .3) + .012 * (1 - r)


def apply_crack(s):
    """Moves every vertex on the crack plane (z == 0) onto the crack surface."""
    P = s['P']
    flat = np.abs(P[..., 2]) < 1e-7
    for t, c in zip(*np.nonzero(flat)):
        P[t, c, 2] = crack(P[t, c, 0], P[t, c, 1])
    return s


def textures():
    t = {}
    t['basalt'] = VL.basalt_tile(128, seed=251)
    t['rune'] = L.swatch((.50, .85, 1.0))
    return t


def half(upper):
    sp = Soup()
    rings = UPPER_RINGS if upper else LOWER_RINGS
    # loft rings are (cx, cy, cz, rx, ry); the crack ring (z = 0) is capped and then bent onto the crack surface
    sp.loft('basalt', [(0, 0, z, r, r) for z, r in rings], SIDES, patch=(0, 0, 2, 2), phase=0, cap=(True, True))
    # the rune channel on the +X face: it runs across the crack, so each half carries its part
    z0, z1 = (.035, .2) if upper else (-.13, -.035)
    sp.box('rune', (RUNE_X, 0, (z0 + z1) / 2), (.008, .024, z1 - z0), tile=1.0, bottom=True)
    sp.box('rune', (RUNE_X, 0, (z0 + z1) / 2 + (.05 if upper else -.03)), (.008, .07, .016), tile=1.0, bottom=True)
    s = apply_crack(sp.soup())
    return s


def chip():
    rng = np.random.default_rng(257)
    sp = Soup()
    V, F = rock(rng, (0, 0, 0), .055, squash=(1.2, .9, .7), detail=0)
    VL.planar_uv(sp, 'basalt', V, F, .4)
    sp.box('rune', (.0, 0, .036), (.03, .012, .006), tile=1.0, bottom=False)
    return sp.soup()


def bake(s):
    """Vertex light: ambient occlusion from the piece itself (it floats; no ground plane) x a soft height gradient."""
    s = dict(s)
    s['C'] = np.ones((len(s['P']), 3, 3))
    bvh = make_bvh(s['P'])
    s, _ = bake_ao(s, bvh, strength=.5, dist=.12, n=12)
    N = tri_normals(s['P'])
    z = s['P'][..., 2]
    grad = .85 + .15 * np.clip((z + .2) / .5, 0, 1)
    s['C'] = s['C'] * grad[..., None] * (.9 + .1 * np.clip(N[:, 2], 0, 1))[:, None, None]
    return s


def finish(asset, rev, package, name, s, texs, size):
    out_dir = os.path.join(ROOT, 'Art', asset, f'r{rev:03d}')
    s = bake(s)
    solid = [k for k in set(s['mat'].tolist()) if texs[k].shape[0] <= 8 and texs[k].shape[1] <= 8]
    s2, A, place = L.build_atlas(s, texs, size=size, pad=PAD, solid=solid, repeat={})
    used_px = sum((w + 2 * PAD) * (h + 2 * PAD) for (_, _, w, h) in place.values())
    bpy.ops.wm.read_factory_settings(use_empty=True)
    img = L.new_image('T_' + name[3:], A)
    ob, glow_tris = R.make_object_rgba(name, s2, img, glow_mats=GLOW)
    path = R.export_glb(ob, os.path.join(out_dir, name + '.glb'))
    st = R.glb_stats(path)
    st.update(asset_id=asset, revision=rev, unreal_package=package, glow_tris=glow_tris, atlas_px=size, atlas_fill=round(used_px / size / size, 3),
              file=os.path.relpath(path, ROOT).replace(os.sep, '/'))
    METRICS[name] = st
    print('SETPIECE', name, st['triangles'], 'tris', st['size_m'], 'bbox', st['bbox_min_m'], st['bbox_max_m'])


ASSETS = [
    ('setpiece-vault-seal-upper', 1, '/Game/DnDDiceRoll/Theme/SetPiece/SM_SetPiece_Vault_SealUpper', 'SM_SetPiece_Vault_SealUpper', lambda: half(True), 256),
    ('setpiece-vault-seal-lower', 1, '/Game/DnDDiceRoll/Theme/SetPiece/SM_SetPiece_Vault_SealLower', 'SM_SetPiece_Vault_SealLower', lambda: half(False), 256),
    ('setpiece-vault-chip', 1, '/Game/DnDDiceRoll/Theme/SetPiece/SM_SetPiece_Vault_Chip', 'SM_SetPiece_Vault_Chip', chip, 256),
]


def check_mating():
    """The two halves share one crack: the crack vertices of the upper half are exactly those of the lower half (so the intact seal shows
    no gap). Returns the largest distance from a crack vertex of either half to the nearest crack vertex of the other, in metres."""
    def crack_points(upper):
        P = half(upper)['P'].reshape(-1, 3)
        on = [p for p in P if math.hypot(p[0], p[1]) <= .0851 and abs(p[2] - crack(p[0], p[1])) < 1e-9]
        return np.unique(np.round(np.array(on), 6), axis=0)
    a, b = crack_points(True), crack_points(False)
    gap = lambda x, y: max(float(np.min(np.linalg.norm(y - p, axis=1))) for p in x)
    return max(gap(a, b), gap(b, a)), len(a), len(b)


def main():
    texs = textures()
    for asset, rev, package, name, fn, size in ASSETS:
        finish(asset, rev, package, name, fn(), texs, size)
    gap, na, nb = check_mating()
    METRICS['crack'] = dict(mismatch_m=round(gap, 9), upper_vertices=int(na), lower_vertices=int(nb))
    with open(os.path.join(OUT_SRC, 'setpiece_metrics.json'), 'w') as fh:
        json.dump(METRICS, fh, indent=1)
    print('SETPIECE_METRICS', json.dumps({k: v.get('triangles', v) for k, v in METRICS.items()}))


main()
