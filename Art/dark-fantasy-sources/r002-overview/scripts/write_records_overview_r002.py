"""Asset records (asset.json) for the #149 overview set: the war table, the three route dioramas and the Vigil occluders with the high-resolution rose.

  python3 Art/dark-fantasy-sources/r002-overview/scripts/write_records_overview_r002.py     (after the builds and validate_rose_vigil.py; standard library only)

Four new asset ids (r001 of each, new packages under /Game/DnDDiceRoll/Theme/Overview) and world-vigil-occluders r002 (same package, a reimport).
Records follow docs/agents/asset-manifests.md; hashes are of the files on disk when this runs.  validation.state stays 'pending' until the import and the
on-screen checks are recorded.
"""
import copy
import hashlib
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
SRC = os.path.join(ROOT, 'Art', 'dark-fantasy-sources')
OV = os.path.join(SRC, 'r002-overview')
RS = os.path.join(SRC, 'r002-rose')
ISSUE = 'https://github.com/AustinKelsay/dnd-diceroll/issues/149'


def sha(path):
    return hashlib.sha256(open(os.path.join(ROOT, path), 'rb').read()).hexdigest()


def srcs(paths):
    return [dict(role='source', path=p, sha256=sha(p)) for p in paths]


POLICY = dict(
    name='dnd-diceroll-dark-fantasy-theme', version=2,
    settings=dict(
        importer='Interchange glTF via Content/Python/import_dark_fantasy_theme.py, explicit reviewed import (#149 overview set)',
        material='MI_* child of M_DiceRollTheme: texture x COLOR_0.rgb, glow from COLOR_0.a',
        texture_filter='trilinear with anisotropic 8, mips generated, never nearest',
        texture_compression='default (sRGB, mips); no nearest or editor-icon compression',
        nanite='off, set explicitly on import',
        lods='none: one low-count mesh drawn by the overview only (the table is placed once, the dioramas at most twice)',
        collision='none (presentation only)',
        vertex_colour='COLOR_0 VEC4 normalised uint16 (RGB baked light, A glow mask: flames and route markers)',
        axes='metres, the asset faces +X (the overview turns it -90 degrees about Z toward the camera), +Y is the viewer\'s right, Z up (glTF Y-up export)'))

COMMON_SCRIPTS = [f'Art/dark-fantasy-sources/r001/scripts/{n}' for n in ('tlib.py', 'prims.py', 'render_lib.py', 'glass.py')] + \
                 [f'Art/dark-fantasy-sources/r002-vault/scripts/{n}' for n in ('r002_lib.py', 'vault_lib.py', 'lod_r002.py')] + \
                 [f'Art/dark-fantasy-sources/r002-overview/scripts/{n}' for n in ('ov_lib.py',)]


def record(asset, package, glb, scripts, method, params):
    return dict(
        schema_version=1, asset_id=asset, revision=1, unreal_package=package, issue=ISSUE,
        provenance=dict(author='dnd-diceroll project agents (Claude Opus 5.5, Claude Sonnet 5.5) for the repository owner',
                        license='Project-owned original work; same terms as this repository',
                        origin='Authored by script in Blender 5.2.3 LTS from the r002 world kits (diorama crops) and procedural geometry and textures (table); no third-party, kit, AI-service or downloaded content'),
        recipe=dict(method=method, approved_reference='#138 look (candlelit tabletop overview); #149 lead calls; the r002 standard of #145, the Root and the Vault', parameters=params,
                    model_revision='not applicable (procedural Blender scripts)'),
        tools=dict(blender='5.2.3 LTS', imagemagick='7 (previews only)'),
        files=srcs(COMMON_SCRIPTS + scripts) + [dict(role='export', path=glb, sha256=sha(glb))],
        import_policy=copy.deepcopy(POLICY), validation=dict(state='pending', evidence=[]))


def write(rec):
    d = os.path.join(ROOT, 'Art', rec['asset_id'], f"r{rec['revision']:03d}")
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, 'asset.json'), 'w') as fh:
        json.dump(rec, fh, indent=2)
        fh.write('\n')
    return os.path.join(d, 'asset.json')


def main():
    tm = json.load(open(os.path.join(OV, 'tabletop_metrics.json')))
    dm = json.load(open(os.path.join(OV, 'diorama_metrics.json')))
    rm = json.load(open(os.path.join(RS, 'rose_metrics.json')))
    out = []
    out.append(record('overview-tabletop', '/Game/DnDDiceRoll/Theme/Overview/SM_Overview_Tabletop', 'Art/overview-tabletop/r001/SM_Overview_Tabletop.glb',
                      ['Art/dark-fantasy-sources/r002-overview/scripts/build_tabletop_r002.py'],
                      'Authored from scratch by script: a deep war table (planks at 256 texels/m, 3.4 x 2.65 m) with a bookcase of leather spines behind it, a hand-drawn vellum map of the five-stop route held by stone weights with five glowing route markers, a candlestick, a pillar candle and a candle cluster (vertex-alpha flame glow), a tankard, dagger, dice, coins, pouch, inkwell and quill, scrolls and an hourglass; conforming subdivision (3.5 cm near props), ray-cast ambient occlusion and a warm candle bake',
                      dict(units='metres', pivot='middle of the table top; the table occupies x -0.60 to +2.05 (front toward +X) and y +-1.70', triangles=tm['triangles'], size_m=tm['size_m'], atlas_px=tm['atlas_px'],
                           atlas_fill=tm['atlas_fill'], glow_triangles=tm['glow_tris'], texel_density_per_m=tm['texel_density'], candle_flames_m=tm['lights_m'], route_markers_xy_m=tm['route_marker_xy_m'],
                           replaces='SM_Tabletop (r001) stays as the fallback')))
    for key in ('root', 'vigil', 'vault'):
        name = f'SM_Overview_Diorama_{key.capitalize()}'
        m = dm[name]
        world = {'root': 'the Root of the World r002 world (SM_World_Root LOD2 and its occluders)', 'vigil': 'the Drowned Vigil r002 world (SM_World_Vigil LOD2 and the occluders with the high-resolution rose)', 'vault': 'the Vault r002 world (SM_World_Vault LOD2 and its occluders)'}[key]
        out.append(record(f'overview-diorama-{key}', f'/Game/DnDDiceRoll/Theme/Overview/{name}', f'Art/overview-diorama-{key}/r001/{name}.glb',
                          ['Art/dark-fantasy-sources/r002-overview/scripts/build_dioramas_r002.py'],
                          f'A miniature of {world}: the world cropped to a window with exact clipping, scaled to a 58 cm tray (about 1:{round(1 / m["scale"])}), its baked vertex light and glow mask kept, kit pieces placed where the game puts obstacles, set on a dark-oak tray with brass corners and a plaque, packed into one 1024 atlas and decimated to a triangle target',
                          dict(units='metres', pivot='middle of the underside of the tray; placed on the table top', triangles=m['triangles'], size_m=m['size_m'], scale=m['scale'], tray_footprint_m=m['tray_footprint_m'],
                               atlas_px=1024, atlas_fill=m['atlas_fill'], replaces=f'SM_Diorama_{"Root" if key == "root" else "Vigil"} (r001) stays as the fallback' if key != 'vault' else 'new: no r001 Vault diorama (the finale drew the Vigil diorama)')))
    for rec in out:
        print(write(rec))
    # world-vigil-occluders r002: the r001 record with the rose patch applied (same package, a reimport)
    prev = json.load(open(os.path.join(ROOT, 'Art', 'world-vigil-occluders', 'r001', 'asset.json')))
    rec = copy.deepcopy(prev)
    rec['revision'] = 2
    rec['recipe']['method'] = prev['recipe']['method'] + '; r002: the stained-glass rose (about 9 m across) is re-pointed from a 128 x 128 atlas region (14.2 texels/m) to a new 2000 x 2000 region at atlas pixel (2072, 24) with 24 px of wrapped padding, the same design redrawn at full resolution (222.2 texels/m); the atlas becomes 4096 x 2048 with the old 2048 square pixel for pixel in the left half, so every non-rose vertex keeps its texel (its U is rescaled by 1/2); no position, normal, vertex colour or index changes (patch_rose_vigil.py, proved by validate_rose_vigil.py)'
    rec['recipe']['parameters'] = dict(prev['recipe']['parameters'])
    rec['recipe']['parameters']['material'] = 'one baseColor atlas 4096x2048 plus COLOR_0 RGBA (A = glow mask)'
    rec['recipe']['parameters']['atlas'] = 'the r001 2048x2048 atlas unchanged in the left half (pixel for pixel; non-rose U coordinates rescaled by 1/2 to keep their texels) plus a 2000 x 2000 rose region at atlas pixel (2072, 24) with 24 px wrapped padding in the right half; SM_World_Vigil keeps its own 2048 atlas, which has no rose'
    f0 = next(iter(rm['files'].values()))
    rec['recipe']['parameters']['rose_window'] = dict(texels_per_m_before=f0['texels_per_m_before'], texels_per_m_after=f0['texels_per_m_after'], region_px=[2000, 2000], region_origin_px=[2072, 24], padding_px=24, atlas_px=[4096, 2048], derived_from='world-vigil-occluders r001')
    exports = [f'Art/world-vigil-occluders/r002/SM_World_Vigil_Occluders{lod}.glb' for lod in ('', '_LOD1', '_LOD2')]
    keep = [f for f in prev['files'] if f['role'] != 'export']
    for f in keep:
        f['sha256'] = sha(f['path'])
    extra = srcs(['Art/dark-fantasy-sources/r002-rose/scripts/rose_lib.py', 'Art/dark-fantasy-sources/r002-rose/scripts/patch_rose_vigil.py', 'Art/dark-fantasy-sources/r002-rose/scripts/validate_rose_vigil.py'])
    rec['files'] = keep + extra + [dict(role='reference', path='Art/dark-fantasy-sources/r002-rose/T_World_Vigil_Atlas_rose.png', sha256=sha('Art/dark-fantasy-sources/r002-rose/T_World_Vigil_Atlas_rose.png'))] + \
        [dict(role='export', path=p, sha256=sha(p)) for p in exports]
    lods = rec['recipe']['parameters'].get('lods')
    if lods:
        lods['tool'] = lods['tool'].replace('16-bit COLOR_0 and atlas unchanged', '16-bit COLOR_0 unchanged, the atlas embedded as the r002 export (4096 x 2048)')
        for entry in lods['lods']:
            entry['file'] = entry['file'].replace('world-vigil-occluders/r001', 'world-vigil-occluders/r002')
    rec['validation'] = dict(state='pending', evidence=[])
    print(write(rec))


if __name__ == '__main__':
    main()
