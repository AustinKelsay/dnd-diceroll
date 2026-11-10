"""Write the versioned asset records (asset.json) for every Root r002 export, from the metrics the build scripts wrote.

  python3 Art/dark-fantasy-sources/r002-root/scripts/write_records_root_r002.py     (run after build_root_r002.py, build_kit_root_r002.py, build_lods_root_r002.py and validate_root_r002.py)

Standard library only.  Records follow docs/agents/asset-manifests.md; hashes are of the files on disk when this runs, so rerun
it after any rebuild.  validation.state stays 'pending': nothing here has been imported into Unreal.
"""
import hashlib
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
SRC = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r002-root')
ISSUE = 'https://github.com/AustinKelsay/dnd-diceroll/issues/149'


def sha(path):
    return hashlib.sha256(open(os.path.join(ROOT, path), 'rb').read()).hexdigest()


def src_files(scripts):
    paths = [f'Art/dark-fantasy-sources/r001/scripts/{n}' for n in ('tlib.py', 'prims.py', 'render_lib.py')] + \
            [f'Art/dark-fantasy-sources/r002-root/scripts/{n}' for n in scripts]
    return [dict(role='source', path=p, sha256=sha(p)) for p in paths]


POLICY = dict(
    name='dnd-diceroll-dark-fantasy-theme', version=2,
    settings=dict(
        importer='Interchange glTF via Content/Python/import_dark_fantasy_theme.py, explicit reviewed reimport (#149 phase 2, as #145)',
        material='MI_* child of M_DiceRollTheme: texture x COLOR_0.rgb, glow from COLOR_0.a',
        texture_filter='trilinear with anisotropic 8, mips generated, never nearest',
        texture_compression='default (sRGB, mips); no nearest or editor-icon compression',
        nanite='off, set explicitly on import',
        lods='explicit LODs for meshes over ~1k triangles (#105); cards, light shafts and small scatter exempt',
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
                        origin='Authored by script in Blender 5.2.3 LTS, deep high fantasy with a PS1 look: the Root of the World identity (giant tree, roots, mossy flagstones, fungus, canopy light) from the owner-approved r001 Root (issue #38) and the #140 gate-1 decisions; all textures and geometry procedural; no third-party, kit, AI-service or downloaded content'),
        recipe=dict(method=method, approved_reference='Gate 1 decision on #140 (2026-11-08); #145 r002 standard; parent #138', parameters=params, model_revision='not applicable (procedural Blender scripts)'),
        tools=dict(blender='5.2.3 LTS', imagemagick='7 (previews only)'),
        files=src_files(scripts) + [dict(role='export', path=glb, sha256=sha(glb))],
        import_policy=pol, validation=dict(state='pending', evidence=[]))


LOD_EXEMPT = 'exempt: under ~1k triangles or a translucent card (no LOD exports)'


def add_lods(rec, lod_metrics):
    """Attach the LOD exports (role export, hashed) and their triangle counts, or record the exemption."""
    name = os.path.splitext(os.path.basename(next(f['path'] for f in rec['files'] if f['role'] == 'export')))[0]
    entry = lod_metrics.get(name)
    if entry is None:
        rec['import_policy']['settings']['lods'] = LOD_EXEMPT
        rec['recipe']['parameters']['lods'] = 'none (exempt)'
        return
    rec['import_policy']['settings']['lods'] = 'explicit LOD1 (about 50 %) and LOD2 (about 25 %, up to 40 % where the silhouette needs it) imported from <name>_LOD1.glb and <name>_LOD2.glb beside the LOD0 export'
    rec['recipe']['parameters']['lods'] = dict(lod0_triangles=entry['lod0_triangles'], tool='Blender collapse decimator, UV-island seams kept, 16-bit COLOR_0 and atlas unchanged, extreme vertices pinned',
                                               lods=[dict(lod=r['lod'], triangles=r['triangles'], fraction=r['fraction'], file=r['file'], deviation_m_p50_p95_max=r['deviation_m_p50_p95_max'],
                                                          appearance_rgb_mean_p95=[r['appearance']['rgb_mean'], r['appearance']['rgb_p95']]) for r in entry['lods']])
    for extra in ('lod_r002.py', 'build_lods_root_r002.py', 'relabel_lods_root_r002.py'):
        rec['files'].insert(len([f for f in rec['files'] if f['role'] == 'source']), dict(role='source', path=f'Art/dark-fantasy-sources/r002-root/scripts/{extra}', sha256=sha(f'Art/dark-fantasy-sources/r002-root/scripts/{extra}')))
    for r in entry['lods']:
        rec['files'].append(dict(role='export', path=r['file'], sha256=sha(r['file'])))


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
    cc = json.load(open(os.path.join(SRC, 'camera_check.json')))
    out = []
    world_scripts = ['r002_lib.py', 'camera_volume.py', 'paving_root.py', 'root_lib.py', 'build_root_r002.py', 'validate_root_r002.py']
    shared = dict(units='metres, 1 cell = 1.50 m, footprint 18 x 12 cells = 27 x 18 m centred on the origin (the 16 x 11 board sits inside it)', axes='+X forward, Z up', material='one baseColor atlas 2048x2048 plus COLOR_0 RGBA (A = glow mask on fungus caps and sap veins, 0 elsewhere)',
                  sampling='Linear (trilinear + mips) with 16 px wrapped padding per atlas region')
    rule = ('Camera clear volume (camera_volume.py): SIGHT VOLUME = convex hull of the footprint (+ half-cell margin, lifted 12 cm) and every camera eye of the #143 pose law (framed 1400-4600, zoom 900-6400, pitch 35/45/60, 36 yaws) '
            'plus the lead envelope (reach 55 m, height 66 m); the world shell keeps 1.0 m outside it except ground within 12 cm of the floor; meshes that enter it are the occluders. '
            'EYE SPACE = every eye, 1.5 m sphere: no mesh contains or approaches an eye.')
    out.append(record('world-root', 2, '/Game/DnDDiceRoll/Theme/Worlds/SM_World_Root', 'Art/world-root/r002/SM_World_Root.glb', world_scripts,
                      'Rebuilt from scratch in metres (r001 geometry not reused): continuous irregular flagstone paving (35-90 cm, random courses and patch angles, #145 recipe) thinning into root-wrapped mossy earth, a bowl of forest floor rising under the camera sight volume, surface roots, stumps, fallen logs, boulders, fungus (glow only via COLOR_0.a) and ferns, a mid ring of stumps capped under the sight volume, a ring of full-height trunks with canopy outside it, and the great tree; vertex light at about 75 cm near the board; arches that enter the sight volume go to SM_World_Root_Occluders',
                      dict(shared, pivot='origin = board centre at floor level, geometry in place (not per-cell)', triangles=wm['world']['triangles'], stones=wm['stones'], clear_volume_rule=rule,
                           camera_check=dict(eyes=cc['eye_positions'], nearest_geometry_to_any_eye_m=cc['nearest_geometry_to_any_eye_m'], rays_72_cameras=cc['rays_72_cameras'], rays_envelope_sweep=cc['rays_envelope_sweep'],
                                             highest_point_inside_footprint_m=cc['world_max_height_inside_footprint_m'], world_points_inside_sight_volume=cc['world_points_inside_sight_volume_margin_0p5m']),
                           stone_equiv_diameter_cm_p5_p50_p95=wm['stone_equiv_diameter_cm_p5_p50_p95'], stone_long_side_cm_p5_p50_p95_max=wm['stone_long_side_cm_p5_p50_p95_max'], stone_aspect_p5_p50_p95_max=wm['stone_aspect_long_over_short_p5_p50_p95_max'],
                           stone_vertex_count_histogram=wm['stone_vertex_count_histogram'], edge_orientation_resultants_R2_R4_R6=wm['edge_orientation_resultants_R2_R4_R6'],
                           edge_length_share_within_3deg_of_cell_axes=wm['edge_length_share_within_3deg_of_cell_axes'], atlas_fill=wm['atlas']['fill_fraction'], paired_asset='world-root-occluders',
                           derived_from='world-root r001 (same package, reimport; layout and geometry are new)')))
    out.append(record('world-root-occluders', 1, '/Game/DnDDiceRoll/Theme/Worlds/SM_World_Root_Occluders', 'Art/world-root-occluders/r001/SM_World_Root_Occluders.glb', world_scripts,
                      'Great roots arching over the clearing that enter the camera sight volume (so they can come between camera and board), exported as a separate lighter mesh for dither fade; never within 1.5 m of any camera eye',
                      dict(shared, pivot='origin = board centre at floor level; place at identity together with SM_World_Root', triangles=wm['occluders']['triangles'], paired_asset='world-root', clear_volume_rule=rule,
                           atlas='identical image to SM_World_Root (same pixels)', pieces=wm['pieces']['occluder_names'])))
    desc = {
        'prop-root-pillar': 'root spire: four root strands braided into a twisting column', 'prop-root-pillar-b': 'snapped stump with a jagged crown and glowing shelf fungus', 'prop-root-pillar-c': 'sawn mossy stump with branch stubs and toadstools',
        'prop-root-wall': 'root curtain: five root strands arching together with woven cross bands', 'prop-root-wall-b': 'fallen log with an upturned root plate', 'prop-root-wall-c': 'low root tangle with rubble',
        'prop-root-stones': 'mossy boulder cluster', 'prop-root-stones-b': 'three leaning standing stones wrapped in climbing roots', 'prop-root-stones-c': 'low boulders carrying glowing fungus',
        'prop-root-arch-2cell': 'root arch spanning two adjacent cells (3.0 m, long axis Y; rotate 90 degrees about Z for X-adjacent pairs)'}
    for name, m in km.items():
        aid, rev = m['asset_id'], m['revision']
        kind = 'fx' if aid.startswith('fx-') else 'scatter' if aid.startswith('scatter-') else 'obstacle'
        if kind == 'obstacle':
            pivot = ('midpoint of the shared edge of two Y-adjacent cells, at floor level' if 'arch' in aid else 'centre of its 150 cm cell, at floor level')
            method = 'Authored in Blender by script (build_kit_root_r002.py): ' + desc[aid] + '; conforming subdivision to 45 cm, vertex colour = ray-cast ambient occlusion x height gradient x moss tint; glow only via COLOR_0.a on fungus caps'
        elif kind == 'scatter':
            pivot = 'centre of its cell, at floor level; non-blocking dressing, may be rotated freely'
            method = 'Authored in Blender by script (build_kit_root_r002.py): small non-blocking Root scatter prop'
        else:
            pivot = 'see recipe note'
            method = 'Authored in Blender by script (build_kit_root_r002.py): translucent card with per-vertex opacity in COLOR_0.a'
        p = dict(units='metres at true scale for a 150 cm cell', axes='+X forward, Z up', pivot=pivot, triangles=m['triangles'], size_m=m['size_m'],
                 bbox_min_m=m['bbox_min_m'], bbox_max_m=m['bbox_max_m'], atlas_px=m['atlas_px'], sampling='Linear, 8 px wrapped padding per atlas region', theme='Root of the World (Root encounters only; the Vigil kit is separate)')
        if aid.startswith('fx-root-mist-card'):
            p['pivot'] = {'fx-root-mist-card': 'base centre of a 3.0 x 1.1 m upright card, plane YZ facing +-X',
                          'fx-root-mist-card-b': 'base centre of a 2.2 x 2.2 m card leaning back about 15 degrees (top toward -X)',
                          'fx-root-mist-card-c': 'floor centre of a 3.0 x 3.0 m flat card at z = 0.12 m'}[aid]
        if aid.startswith('fx-root-light-shaft'):
            p['pivot'] = 'centre of the lit floor patch; the shaft rises along its lean toward -X (light travels toward +X); opacity fades at floor and tip; warm gold canopy light'
        if aid.startswith('fx-'):
            p['opacity'] = 'COLOR_0.a'
        out.append(record(aid, rev, m['unreal_package'], m['file'].replace(os.sep, '/'), ['r002_lib.py', 'root_lib.py', 'build_kit_root_r002.py'], method, p, alpha_is_opacity=(kind == 'fx')))
    lod_metrics = json.load(open(os.path.join(SRC, 'lod_metrics.json'))) if os.path.exists(os.path.join(SRC, 'lod_metrics.json')) else {}
    for rec in out:
        add_lods(rec, lod_metrics)
    for rec in out:
        print('wrote', os.path.relpath(write(rec), ROOT))


main()
