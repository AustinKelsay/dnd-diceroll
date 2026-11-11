"""Write the versioned asset records (asset.json) for every Vault r002 export, from the metrics the build scripts wrote.

  python3 Art/dark-fantasy-sources/r002-vault/scripts/write_records_vault_r002.py     (run after build_vault_r002.py, build_kit_vault_r002.py, build_lods_vault_r002.py and validate_vault_r002.py)

Standard library only.  Records follow docs/agents/asset-manifests.md; hashes are of the files on disk when this runs, so rerun
it after any rebuild.  validation.state stays 'pending': nothing here has been imported into Unreal.
"""
import hashlib
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
SRC = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r002-vault')
ISSUE = 'https://github.com/AustinKelsay/dnd-diceroll/issues/149'


def sha(path):
    return hashlib.sha256(open(os.path.join(ROOT, path), 'rb').read()).hexdigest()


def src_files(scripts):
    paths = [f'Art/dark-fantasy-sources/r001/scripts/{n}' for n in ('tlib.py', 'prims.py', 'render_lib.py')] + \
            [f'Art/dark-fantasy-sources/r002-vault/scripts/{n}' for n in scripts]
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
                        origin='Authored by script in Blender 5.2.3 LTS, dark high fantasy with a PS1 look: a roofless crypt hall of cold basalt ashlar with reliquary alcoves, sarcophagi, chains and the Waystone, lit by a single cold-blue accent (the colour the finale already uses for its text); all textures and geometry procedural; no third-party, kit, AI-service or downloaded content'),
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
    for extra in ('lod_r002.py', 'build_lods_vault_r002.py', 'relabel_lods_vault_r002.py'):
        rec['files'].insert(len([f for f in rec['files'] if f['role'] == 'source']), dict(role='source', path=f'Art/dark-fantasy-sources/r002-vault/scripts/{extra}', sha256=sha(f'Art/dark-fantasy-sources/r002-vault/scripts/{extra}')))
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
    world_scripts = ['r002_lib.py', 'camera_volume.py', 'paving_vault.py', 'vault_lib.py', 'build_vault_r002.py', 'validate_vault_r002.py']
    shared = dict(units='metres, 1 cell = 1.50 m, footprint 18 x 12 cells = 27 x 18 m centred on the origin (the 16 x 11 board sits inside it)', axes='+X forward, Z up', material='one baseColor atlas 2048x2048 plus COLOR_0 RGBA (A = glow mask on the cold-blue rune and crystal materials, 0 elsewhere)',
                  sampling='Linear (trilinear + mips) with 16 px wrapped padding per atlas region')
    rule = ('Camera clear volume (camera_volume.py): SIGHT VOLUME = convex hull of the footprint (+ half-cell margin, lifted 12 cm) and every camera eye of the #143 pose law (framed 1400-4600, zoom 900-6400, pitch 35/45/60, 36 yaws) '
            'plus the lead envelope (reach 55 m, height 66 m); the world shell keeps 1.0 m outside it except ground within 12 cm of the floor; meshes that enter it are the occluders. '
            'EYE SPACE = every eye, 1.5 m sphere: no mesh contains or approaches an eye.')
    out.append(record('world-vault', 1, '/Game/DnDDiceRoll/Theme/Worlds/SM_World_Vault', 'Art/world-vault/r001/SM_World_Vault.glb', world_scripts,
                      'Authored from scratch in metres, a new package (the Vault never had a world of its own; it drew the Vigil kit): continuous irregular flagstone paving (35-100 cm, random courses and patch angles, the Root/#145 recipe in cold blue-grey) over a dark slab floor, cold-blue rune inlays (glow only via COLOR_0.a), a far hall of ashlar walls with lancet glows and capped piers outside the sight volume, low dressing (sarcophagus rows, a collapsed vault rib, drums, urns, chests, chains, bones, skulls, candle stubs, crystal chips) at the open front and corners; vertex light at about 75 cm near the board; everything that enters the sight volume goes to SM_World_Vault_Occluders',
                      dict(shared, pivot='origin = board centre at floor level, geometry in place (not per-cell)', triangles=wm['world']['triangles'], stones=wm['stones'], clear_volume_rule=rule,
                           content=wm['content_counts'],
                           camera_check=dict(eyes=cc['eye_positions'], nearest_geometry_to_any_eye_m=cc['nearest_geometry_to_any_eye_m'], rays_72_cameras=cc['rays_72_cameras'], rays_envelope_sweep=cc['rays_envelope_sweep'],
                                             highest_point_inside_footprint_m=cc['world_max_height_inside_footprint_m'], world_points_inside_sight_volume=cc['world_points_inside_sight_volume_margin_0p5m']),
                           stone_equiv_diameter_cm_p5_p50_p95=wm['stone_equiv_diameter_cm_p5_p50_p95'], stone_long_side_cm_p5_p50_p95_max=wm['stone_long_side_cm_p5_p50_p95_max'], stone_aspect_p5_p50_p95_max=wm['stone_aspect_long_over_short_p5_p50_p95_max'],
                           stone_vertex_count_histogram=wm['stone_vertex_count_histogram'], edge_orientation_resultants_R2_R4_R6=wm['edge_orientation_resultants_R2_R4_R6'],
                           edge_length_share_within_3deg_of_cell_axes=wm['edge_length_share_within_3deg_of_cell_axes'], atlas_fill=wm['atlas']['fill_fraction'], paired_asset='world-vault-occluders')))
    out.append(record('world-vault-occluders', 1, '/Game/DnDDiceRoll/Theme/Worlds/SM_World_Vault_Occluders', 'Art/world-vault-occluders/r001/SM_World_Vault_Occluders.glb', world_scripts,
                      'Near architecture that enters the camera sight volume (so it can come between camera and board), exported as a separate mesh for dither fade: the back wall with the Waystone apse and dais, the declining side walls, pilasters, reliquary alcoves, corbel stubs, hanging chains, the front-corner sarcophagus rows and the collapsed rib; never within 1.5 m of any camera eye. LOD1 only: sharp-edge vertices are pinned, so a collapse stops at about 54 % and LOD2 would equal LOD1',
                      dict(shared, pivot='origin = board centre at floor level; place at identity together with SM_World_Vault', triangles=wm['occluders']['triangles'], paired_asset='world-vault', clear_volume_rule=rule,
                           walls_cells_outside_18x12=wm['wall_inner_faces_m']['cells_outside_18x12'], walls_cells_outside_16x11=wm['wall_inner_faces_m']['cells_outside_16x11'],
                           atlas='identical image to SM_World_Vault (same pixels)', pieces=wm['pieces']['occluder_names'])))
    desc = {
        'prop-vault-pillar': 'broken fluted column on a plinth with a rune ring and a jagged crown, a fallen drum at its foot', 'prop-vault-pillar-b': 'ossuary pier: a square pier with skull bands, a corbelled capital and a hanging chain', 'prop-vault-pillar-c': 'rune stele: a tapered basalt slab on a stepped base with a cold-blue glyph',
        'prop-vault-wall': 'broken ashlar run with a ragged top, cornice and a rune keystone', 'prop-vault-wall-b': 'stacked sarcophagi: two stone coffins end to end across the cell with a third on top', 'prop-vault-wall-c': 'iron grille on a low ashlar wall with swagged chains and a crystal lantern shard',
        'prop-vault-stones': 'rubble heap of fallen ashlar and broken stone', 'prop-vault-stones-b': 'bone mound of skulls and long bones around a cold crystal', 'prop-vault-stones-c': 'toppled hooded funerary statue in rubble',
        'prop-vault-arch-2cell': 'crypt arch spanning two adjacent cells (2.9 m, long axis Y; rotate 90 degrees about Z for X-adjacent pairs): ashlar piers, pointed voussoir arch, rune keystone line, a hanging chain'}
    for name, m in km.items():
        aid, rev = m['asset_id'], m['revision']
        kind = 'fx' if aid.startswith('fx-') else 'scatter' if aid.startswith('scatter-') else 'obstacle'
        if kind == 'obstacle':
            pivot = ('midpoint of the shared edge of two Y-adjacent cells, at floor level' if 'arch' in aid else 'centre of its 150 cm cell, at floor level')
            method = 'Authored in Blender by script (build_kit_vault_r002.py): ' + desc[aid] + '; conforming subdivision to 45 cm, vertex colour = ray-cast ambient occlusion x height gradient; glow only via COLOR_0.a on the rune and crystal materials'
        elif kind == 'scatter':
            pivot = 'centre of its cell, at floor level; non-blocking dressing, may be rotated freely'
            method = 'Authored in Blender by script (build_kit_vault_r002.py): small non-blocking Vault scatter prop'
        else:
            pivot = 'see recipe note'
            method = 'Authored in Blender by script (build_kit_vault_r002.py): translucent card with per-vertex opacity in COLOR_0.a'
        p = dict(units='metres at true scale for a 150 cm cell', axes='+X forward, Z up', pivot=pivot, triangles=m['triangles'], size_m=m['size_m'],
                 bbox_min_m=m['bbox_min_m'], bbox_max_m=m['bbox_max_m'], atlas_px=m['atlas_px'], sampling='Linear, 8 px wrapped padding per atlas region', theme='the Vault finale (encounter 5 only; the Vigil and Root kits are separate)')
        if aid.startswith('fx-vault-mist-card'):
            p['pivot'] = {'fx-vault-mist-card': 'base centre of a 3.0 x 1.1 m upright card, plane YZ facing +-X',
                          'fx-vault-mist-card-b': 'base centre of a 2.2 x 2.2 m card leaning back about 15 degrees (top toward -X)',
                          'fx-vault-mist-card-c': 'floor centre of a 3.0 x 3.0 m flat card at z = 0.12 m'}[aid]
        if aid.startswith('fx-vault-light-shaft'):
            p['pivot'] = 'centre of the lit floor patch; the shaft rises along its lean toward -X (light travels toward +X); opacity fades at floor and tip; cold moonlight'
        if aid.startswith('fx-'):
            p['opacity'] = 'COLOR_0.a'
        out.append(record(aid, rev, m['unreal_package'], m['file'].replace(os.sep, '/'), ['r002_lib.py', 'vault_lib.py', 'build_kit_vault_r002.py'], method, p, alpha_is_opacity=(kind == 'fx')))
    lod_metrics = json.load(open(os.path.join(SRC, 'lod_metrics.json'))) if os.path.exists(os.path.join(SRC, 'lod_metrics.json')) else {}
    for rec in out:
        add_lods(rec, lod_metrics)
    for rec in out:
        print('wrote', os.path.relpath(write(rec), ROOT))


main()
