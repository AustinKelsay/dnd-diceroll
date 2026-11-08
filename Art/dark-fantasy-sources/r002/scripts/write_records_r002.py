"""Write the versioned asset records (asset.json) for every r002 export, from the metrics the build scripts wrote.

  python3 Art/dark-fantasy-sources/r002/scripts/write_records_r002.py     (run after build_vigil_r002.py and build_kit_r002.py)

Standard library only.  Records follow docs/agents/asset-manifests.md; hashes are of the files on disk when this runs, so rerun
it after any rebuild.  validation.state stays 'pending': nothing here has been imported into Unreal.
"""
import hashlib
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
SRC = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r002')
ISSUE = 'https://github.com/AustinKelsay/dnd-diceroll/issues/145'


def sha(path):
    return hashlib.sha256(open(os.path.join(ROOT, path), 'rb').read()).hexdigest()


def src_files(scripts):
    paths = ['Art/dark-fantasy-sources/r001/vigil.blend'] + [f'Art/dark-fantasy-sources/r001/scripts/{n}' for n in ('tlib.py', 'glass.py', 'prims.py', 'render_lib.py')] + \
            [f'Art/dark-fantasy-sources/r002/scripts/{n}' for n in scripts]
    return [dict(role='source', path=p, sha256=sha(p)) for p in paths]


POLICY = dict(
    name='dnd-diceroll-dark-fantasy-theme', version=2,
    settings=dict(
        importer='Interchange glTF via Content/Python/import_dark_fantasy_theme.py, explicit reviewed reimport (#145 phase 2)',
        material='MI_* child of M_DiceRollTheme: texture x COLOR_0.rgb, glow from COLOR_0.a',
        texture_filter='trilinear with anisotropic 8, mips generated, never nearest',
        texture_compression='default (sRGB, mips); no nearest or editor-icon compression',
        nanite='off, set explicitly on import',
        lods='explicit LODs required on every themed mesh (#105); none authored in r002',
        collision='none (presentation only; blocking comes from gameplay data)',
        vertex_colour='COLOR_0 VEC4 normalised uint16 (RGB light, A glow mask)',
        axes='metres, +X forward, Z up (glTF Y-up export)'))


def record(asset, rev, package, glb, scripts, method, params, prev_note=None, alpha_is_opacity=False):
    pol = json.loads(json.dumps(POLICY))
    if alpha_is_opacity:
        pol['settings']['material'] = 'translucent card material: texture x COLOR_0.rgb, opacity = COLOR_0.a (no glow); two-sided geometry already doubled, depth write off'
        pol['settings']['vertex_colour'] = 'COLOR_0 VEC4 normalised uint16 (RGB tint, A opacity)'
    return dict(
        schema_version=1, asset_id=asset, revision=rev, unreal_package=package, issue=ISSUE,
        provenance=dict(author='dnd-diceroll project agents (Claude Opus 5.5, Claude Sonnet 5.5) for the repository owner',
                        license='Project-owned original work; same terms as this repository',
                        origin='Authored by script in Blender from the owner-approved Vigil look (issue #38) and the #140 gate-1 decisions; textures are r001 swatches (project-owned) or procedural; no third-party or downloaded content'),
        recipe=dict(method=method, approved_reference='Gate 1 decision on #140 (2026-11-08); parent #138', parameters=params, model_revision='not applicable (procedural Blender scripts)'),
        tools=dict(blender='5.2.3 LTS', imagemagick='7 (previews only)'),
        files=src_files(scripts) + [dict(role='export', path=glb, sha256=sha(glb))],
        import_policy=pol, validation=dict(state='pending', evidence=[]))


def write(rec):
    d = os.path.join(ROOT, 'Art', rec['asset_id'], f"r{rec['revision']:03d}")
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, 'asset.json'), 'w') as fh:
        json.dump(rec, fh, indent=2)
        fh.write('\n')
    return os.path.join(d, 'asset.json')


def main():
    wm = json.load(open(os.path.join(SRC, 'world_metrics.json')))
    km = json.load(open(os.path.join(SRC, 'kit_metrics.json')))
    out = []
    world_scripts = ['r002_lib.py', 'build_vigil_r002.py']
    shared = dict(units='metres, 1 cell = 1.50 m, footprint 18 x 12 cells = 27 x 18 m centred on the origin', axes='+X forward, Z up', material='one baseColor atlas 2048x2048 plus COLOR_0 RGBA',
                  sampling='Linear (trilinear + mips) with 16 px wrapped padding per atlas region')
    out.append(record('world-vigil', 2, '/Game/DnDDiceRoll/Theme/Worlds/SM_World_Vigil', 'Art/world-vigil/r002/SM_World_Vigil.glb', world_scripts,
                      'Rebuilt from r001 vigil.blend: all architecture kept except what intrudes into the 18 x 12 footprint, near/occluding pieces split into SM_World_Vigil_Occluders, continuous irregular flagstone paving (35-90 cm, random courses and patch angles) within +-3 cm relief, terraces re-textured to 256 texels/m, rune ring moved off the playable area, vertex light at about 75 cm near the board, glow only from COLOR_0.a',
                      dict(shared, pivot='origin = board centre at floor level, geometry in place (not per-cell)', triangles=wm['world']['triangles'], stones=wm['stones'],
                           stone_equiv_diameter_cm_p5_p50_p95=wm['stone_equiv_diameter_cm_p5_p50_p95'], stone_long_side_cm_p5_p50_p95_max=wm['stone_long_side_cm_p5_p50_p95_max'], stone_aspect_p5_p50_p95_max=wm['stone_aspect_long_over_short_p5_p50_p95_max'], stone_vertex_count_histogram=wm['stone_vertex_count_histogram'], edge_orientation_resultants_R2_R4_R6=wm['edge_orientation_resultants_R2_R4_R6'], rune_ring=wm['rune_ring'],
                           atlas_fill=wm['atlas']['fill_fraction'], paired_asset='world-vigil-occluders', derived_from='world-vigil r001 (same package, reimport)')))
    out.append(record('world-vigil-occluders', 1, '/Game/DnDDiceRoll/Theme/Worlds/SM_World_Vigil_Occluders', 'Art/world-vigil-occluders/r001/SM_World_Vigil_Occluders.glb', world_scripts,
                      'Architecture pieces that block any of 72 candidate camera positions (3 distances x pitches 35/45/60 x 8 yaws) to any floor cell, or rise near the board, exported as a separate mesh for dither fade; conforming subdivision (no T-junctions)',
                      dict(shared, pivot='origin = board centre at floor level; place at identity together with SM_World_Vigil', triangles=wm['occluders']['triangles'], paired_asset='world-vigil',
                           atlas='identical image to SM_World_Vigil (same pixels)')))
    desc = {
        'prop-pillar': 'broken column on a plinth', 'prop-pillar-b': 'intact fluted column with capital', 'prop-pillar-c': 'stump with iron band and fallen drums',
        'prop-wall': 'stepped broken wall segment', 'prop-wall-b': 'wall with window slit and buttress', 'prop-wall-c': 'low crumbled wall with rubble',
        'prop-stones': 'split menhir pair', 'prop-stones-b': 'three leaning standing stones', 'prop-stones-c': 'mossy boulder cluster',
        'prop-arch-2cell': 'blocked pointed gate arch spanning two adjacent cells (3.0 m, long axis Y; rotate 90 degrees about Z for X-adjacent pairs)'}
    for name, m in km.items():
        aid, rev = m['asset_id'], m['revision']
        kind = 'fx' if aid.startswith('fx-') else 'scatter' if aid.startswith('scatter-') else 'obstacle'
        if kind == 'obstacle':
            pivot = ('midpoint of the shared edge of two Y-adjacent cells, at floor level' if aid == 'prop-arch-2cell' else 'centre of its 150 cm cell, at floor level')
            method = 'Authored in Blender by script (build_kit_r002.py): ' + desc[aid] + '; conforming subdivision to 45 cm (arch 90 cm), vertex colour = ray-cast ambient occlusion x height gradient x moss tint'
        elif kind == 'scatter':
            pivot = 'centre of its cell, at floor level; non-blocking dressing, may be rotated freely'
            method = 'Authored in Blender by script (build_kit_r002.py): small non-blocking scatter prop'
        else:
            pivot = 'see recipe note'
            method = 'Authored in Blender by script (build_kit_r002.py): translucent card with per-vertex opacity in COLOR_0.a'
        p = dict(units='metres at true scale for a 150 cm cell', axes='+X forward, Z up', pivot=pivot, triangles=m['triangles'], size_m=m['size_m'],
                 bbox_min_m=m['bbox_min_m'], bbox_max_m=m['bbox_max_m'], atlas_px=m['atlas_px'], sampling='Linear, 8 px wrapped padding per atlas region')
        if aid.startswith('fx-mist-card'):
            p['pivot'] = {'fx-mist-card': 'base centre of a 3.0 x 1.1 m upright card, plane YZ facing +-X',
                          'fx-mist-card-b': 'base centre of a 2.2 x 2.2 m card leaning back about 15 degrees (top toward -X)',
                          'fx-mist-card-c': 'floor centre of a 3.0 x 3.0 m flat card at z = 0.12 m'}[aid]
        if aid.startswith('fx-light-shaft'):
            p['pivot'] = 'centre of the lit floor patch; the shaft rises along its lean toward -X (light travels toward +X); opacity fades at floor and tip'
        if aid.startswith('fx-'):
            p['opacity'] = 'COLOR_0.a'
        rec = record(aid, rev, m['unreal_package'], m['file'].replace(os.sep, '/'), ['r002_lib.py', 'build_kit_r002.py'], method, p, alpha_is_opacity=(kind == 'fx'))
        if rev == 2:
            rec['recipe']['parameters']['derived_from'] = f'{aid} r001 (same package, reimport at true metric for a 150 cm cell; variant A of its kit)'
        out.append(rec)
    for rec in out:
        print('wrote', os.path.relpath(write(rec), ROOT))


main()
