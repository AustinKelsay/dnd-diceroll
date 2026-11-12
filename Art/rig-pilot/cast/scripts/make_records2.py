#!/usr/bin/env python3
"""Write the asset records for batch 2 (stdlib only). Run after build2.py and review2.py validate:
    python3 Art/rig-pilot/cast/scripts/make_records2.py

* Art/<asset-id>/r001/asset.json for barbarian, monk, paladin and ranger (new assets).
* Art/<asset-id>/r002/asset.json for fighter, wizard, raider, rogue and cleric: the same meshes, atlases and nine clips plus the tenth
  clip, Brace. Records are immutable per revision, so the re-export is a new revision; the LOD glbs carry no clips and are unchanged,
  so r002 references r001's. Nothing is reimported into Unreal from these (see import_policy).
"""
import copy
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
CAST = 'Art/rig-pilot/cast'
SHARED = [f'{CAST}/README_BATCH2.md'] + [f'{CAST}/scripts/{n}' for n in ('cast_characters.py', 'cast_batch2.py', 'brace.py', 'build2.py', 'review2.py', 'make_records2.py')] + \
    ['Art/rig-pilot/r001/README.md'] + [f'Art/rig-pilot/r001/scripts/{n}' for n in ('rigkit.py', 'clips.py', 'characters.py')]
# Per-class clip overrides (overrides.py, review round 1): the action exported in the class's own LOD0 glb, the package it is imported to once, and the
# theme entry that names it. overrides.py also authors the ranger's bow (the string's nock is skinned to hand.R at the override's hold pose), so it is a
# retained source of exactly these two assets.
OVERRIDE = {
    'ranger': dict(shared='Ranged', action='Ranged_Ranger', frames=36, package='A_Ranger_Ranged', entry='ClassRigs[Ranger].Overrides[Ranged]'),
    'barbarian': dict(shared='Melee', action='Melee_Barbarian', frames=30, package='A_Barbarian_Melee', entry='ClassRigs[Barbarian].Overrides[Melee]'),
}
OVERRIDE_SOURCE = f'{CAST}/scripts/overrides.py'
NEW = {
    'sk-hero-barbarian': ('SK_Hero_Barbarian', 'barbarian', 'Barbarian: heavy skeleton fit bulked up 9%, bare painted torso and arms, fur mantle and kilt, greataxe on weapon_socket'),
    'sk-hero-monk': ('SK_Hero_Monk', 'monk', 'Monk: agile body, saffron gi, golden-ochre short tabard and trousers, sash, wrapped hands, bo staff slung on the back, unarmed in hand'),
    'sk-hero-paladin': ('SK_Hero_Paladin', 'paladin', 'Paladin: heavy body in polished plate, heraldic tabard, white cape, crested helm, warhammer on weapon_socket, tower shield on shield_socket'),
    'sk-hero-ranger': ('SK_Hero_Ranger', 'ranger', 'Ranger: agile body, hooded cape, quiver, short blade on weapon_socket, longbow on shield_socket'),
}
OLD = {'sk-hero-fighter': ('SK_Hero_Fighter', 'fighter'), 'sk-hero-wizard': ('SK_Hero_Wizard', 'wizard'), 'sk-foe-raider': ('SK_Foe_Raider', 'raider'),
       'sk-hero-rogue': ('SK_Hero_Rogue', 'rogue'), 'sk-hero-cleric': ('SK_Hero_Cleric', 'cleric')}
ISSUE = 'https://github.com/AustinKelsay/dnd-diceroll/issues/150'


def sha(p):
    return hashlib.sha256((ROOT / p).read_bytes()).hexdigest()


def require_passed():
    """No record says `passed` unless every validation JSON says so: each carries `pass` (review2.py's gate) and the r002 compare passes. Exits 1 otherwise."""
    problems = []
    for key in [v[1] for v in NEW.values()]:
        p = ROOT / CAST / 'metrics' / f'validate_{key}.json'
        d = json.loads(p.read_text()) if p.exists() else {}
        if d.get('pass') is not True:
            problems.append(f'{p.name}: ' + ('missing' if not d else 'no `pass` (run review2.py validate)' if 'pass' not in d else 'failed: ' + '; '.join(d.get('failures', []))))
    for key in [v[1] for v in OLD.values()]:
        p = ROOT / CAST / 'metrics' / f'validate_{key}_r002.json'
        d = json.loads(p.read_text()) if p.exists() else {}
        if d.get('pass') is not True:
            problems.append(f'{p.name}: ' + ('missing' if not d else 'no `pass` (run review2.py validate --r002)' if 'pass' not in d else 'failed: ' + '; '.join(d.get('failures', []))))
    p = ROOT / CAST / 'metrics' / 'compare_r002.json'
    d = json.loads(p.read_text()) if p.exists() else {}
    for key in [v[1] for v in OLD.values()]:
        if not d.get(key, {}).get('pass'):
            problems.append(f'compare_r002.json: {key} ' + ('missing' if key not in d else 'failed'))
    if problems:
        print('Refusing to write asset records marked passed:\n  ' + '\n  '.join(problems))
        sys.exit(1)


require_passed()


def load(p):
    return json.loads((ROOT / p).read_text())


for aid, (name, key, desc) in NEW.items():
    base = f'Art/{aid}/r001'
    v = load(f'{CAST}/metrics/validate_{key}.json')
    files = [('source', f'{base}/{name}_source.blend')] + [('source', p) for p in SHARED]
    files += [('export', f'{base}/{name}.glb'), ('export', f'{base}/{name}_LOD1.glb'), ('export', f'{base}/{name}_LOD2.glb')]
    ov = OVERRIDE.get(key)
    if ov:
        files.insert(1 + len(SHARED), ('source', OVERRIDE_SOURCE))      # after the shared sources, before the exports
    rec = {
        'schema_version': 1, 'asset_id': aid, 'revision': 1,
        'unreal_package': f'/Game/DnDDiceRoll/Theme/Characters/{name}',
        'issue': ISSUE,
        'provenance': {
            'author': 'dnd-diceroll project agents (Claude Opus 5.5 lead, Claude Sonnet 5.5 subagent) for the repository owner',
            'license': 'Project-owned original work; same terms as this repository',
            'origin': 'Authored by script in Blender; no third-party, downloaded, kit, AI-service or commercial-game content. The r001 look (palette, motifs) is imitated, no r001 mesh or texture is reused.'},
        'recipe': {
            'method': desc + '. Procedural meshes, atlas and skin weights from Art/rig-pilot/cast/scripts (build2.py, cast_batch2.py) on the r001 skeleton; the nine shared hand-keyed clips of Art/rig-pilot/r001/scripts/clips.py plus the tenth, Brace (brace.py)' + (
                f"; one per-class override of the shared {ov['shared']} clip, {ov['action']} ({ov['frames']} frames), authored on the shared skeleton by overrides.py with a small IK over rigkit's FK (the ranger's bow, whose string nock is skinned to hand.R, is authored there too)" if ov else '') + '; no external motion data',
            'approved_reference': 'Gate 1 decision on #140 (25-bone skeleton); #147 pilot contract (Art/rig-pilot/r001/README.md); #150 batch 2 design calls by the graphics lead (class looks, Brace clip, Rim 1.6 and Fill 1.25)',
            'parameters': {'skeleton': '25 bones, shared contract in Art/rig-pilot/r001/README.md (unchanged)', 'units': 'metres, +X forward, Z up, foot pivot at origin',
                           'fps': 30, 'atlas': '1024x1024, 1 material', 'vertex_color': 'COLOR_0 RGBA, alpha = glow mask',
                           'lods': 'LOD0 with clips; LOD1 and LOD2 as separate skinned glbs without clips',
                           'clips': 'ten: Idle, Walk, Melee, Ranged, Cast, Hit, Downed, Death, Recover, Brace (24 frames, 0.8 s, not a loop)' + (
                               f"; plus the override action {ov['action']} in this glb (0 to {ov['frames']} frames, starts and ends on Idle frame 0)" if ov else ''),
                           'blender_validation': {'tris': v['lods'], 'lod_ratio': v['lod_ratio'], 'body_height_m': v['body_height_m_excluding_held_items'],
                                                  'seam_mm_idle_walk': [v['seam_mm_first_vs_last_max_vertex']['Idle'], v['seam_mm_first_vs_last_max_vertex']['Walk']]}},
            'model_revision': 'not applicable (procedural Blender scripts)'},
        'tools': {'blender': '5.2.3 LTS', 'imagemagick': '7 (contact sheets only)'},
        'files': [{'role': r, 'path': p, 'sha256': sha(p)} for r, p in files],
        'import_policy': {'name': 'dnd-diceroll-rigged-character', 'version': 1, 'settings': {
            'importer': 'glTF skeletal mesh; reuse the existing shared skeleton (SKEL_Humanoid from the #147 fighter import); no new skeleton' + (
                ' and, apart from the one override clip named under override_clip, no clips are imported for this character' if ov else ' and no clips are imported for this character'),
            'shared_skeleton': '/Game/DnDDiceRoll/Theme/Characters/SKEL_Humanoid',
            'nanite': 'off, set explicitly (gate 1 pipeline rule)', 'lods': 'LOD1/LOD2 glbs imported as LOD meshes of the same skeletal mesh',
            'animations': 'the shared A_Humanoid_* clips are reused (nine from #147, A_Humanoid_Brace from #150 batch 2); the other clips baked into this glb are not imported' + (
                f" (the baked shared {ov['shared']} is not used for this class: its override replaces it)" if ov else ''),
            'material': 'MI_* child of M_DiceRollTheme with rim and fill terms; glow from vertex alpha only',
            'texture_filter': 'trilinear, anisotropic 8', 'collision': 'none (presentation only)',
            **({'override_clip': {
                'source_action': ov['action'], 'source_file': f'{base}/{name}.glb (the LOD0 export, the same file the mesh is imported from)',
                'shared_clip_replaced': ov['shared'], 'destination_package': f"/Game/DnDDiceRoll/Theme/Characters/Animations/{ov['package']}",
                'skeleton': '/Game/DnDDiceRoll/Theme/Characters/SKEL_Humanoid (the shared one: the glb is imported onto it, the sequence is checked to be on it, nothing else of that import is kept)',
                'policy': 'import once: an existing destination package is kept and never replaced or deleted; the staging import is deleted after the sequence is moved',
                'theme_entry': ov['entry'] + ' (data, UDiceRollTheme / FDiceRollCharacter::Overrides); a class with no entry plays the shared clip',
                'script': 'Content/Python/import_dark_fantasy_theme.py, CLIP_OVERRIDES and import_override_clips'}} if ov else {})}},
        'validation': {'state': 'passed', 'evidence': [
            f'{CAST}/metrics/validate_{key}.json', f'{CAST}/metrics/build_{key}.json',
            f'{CAST}/previews/sheet_07_batch2_turnaround.png', f'{CAST}/previews/sheet_{ {"barbarian": 8, "monk": 9, "paladin": 10, "ranger": 11}[key]:02d}_{key}_clips.png',
            *([f'{CAST}/previews/sheet_{ {"barbarian": 16, "ranger": 15}[key]}_{key}_override_clip.png'] if ov else []),
            ISSUE]},
    }
    (ROOT / base / 'asset.json').write_text(json.dumps(rec, indent=2) + '\n')
    print('wrote', base + '/asset.json')

cmp_ = load(f'{CAST}/metrics/compare_r002.json')
for aid, (name, key) in OLD.items():
    prev = load(f'Art/{aid}/r001/asset.json')
    rec = copy.deepcopy(prev)
    rec['revision'] = 2
    rec['issue'] = ISSUE
    v = load(f'{CAST}/metrics/validate_{key}_r002.json')
    r2 = f'Art/{aid}/r002'
    r1 = f'Art/{aid}/r001'
    keep = [f for f in prev['files'] if f['role'] == 'source' and not f['path'].endswith('_source.blend')]
    files = [('source', f'{r2}/{name}_source.blend')] + [(f['role'], f['path']) for f in keep]
    files += [('source', p) for p in SHARED if p not in {f['path'] for f in keep}]
    files += [('export', f'{r2}/{name}.glb'), ('export', f'{r1}/{name}_LOD1.glb'), ('export', f'{r1}/{name}_LOD2.glb')]
    rec['files'] = [{'role': r, 'path': p, 'sha256': sha(p)} for r, p in files]
    rec['recipe']['method'] = prev['recipe']['method'] + '. Revision 2 re-exports LOD0 with the tenth clip, Brace (Art/rig-pilot/cast/scripts/brace.py, build2.py --r002); meshes, atlas, weights and the nine earlier clips are unchanged (compare_r002.json: vertices 0 mm, keyframes 0 difference)'
    rec['recipe']['parameters'] = copy.deepcopy(prev['recipe']['parameters'])
    rec['recipe']['parameters']['clips'] = 'ten: the nine of r001 plus Brace (24 frames, 0.8 s, not a loop)'
    rec['recipe']['parameters']['blender_validation'] = {'tris': v['lods'], 'lod_ratio': v['lod_ratio'], 'body_height_m': v['body_height_m_excluding_held_items'],
                                                         'seam_mm_idle_walk': [v['seam_mm_first_vs_last_max_vertex']['Idle'], v['seam_mm_first_vs_last_max_vertex']['Walk']],
                                                         'seam_mm_brace': v['seam_mm_first_vs_last_max_vertex']['Brace'],
                                                         'unchanged_vs_r001': {'max_vertex_diff_mm': cmp_[key]['max_vertex_diff_mm'], 'extra_clips': cmp_[key]['extra_clips'],
                                                                              'max_clip_key_diff': max(c['max_abs_key_diff'] for c in cmp_[key]['clips'].values())}}
    s = rec['import_policy']['settings']
    s['revision_note'] = ('r002 only adds the Brace clip to the LOD0 glb. The mesh in Unreal is not reimported from it (the r001 import stays in use, mesh identical, verified '
                          'by compare_r002.json)' + ('; A_Humanoid_Brace is imported from this fighter glb, the one source for the shared tenth clip' if key == 'fighter' else ''))
    rec['validation'] = {'state': 'passed', 'evidence': [f'{CAST}/metrics/validate_{key}_r002.json', f'{CAST}/metrics/compare_r002.json', f'{CAST}/metrics/build_{key}_r002.json',
                                                         f'{CAST}/previews/sheet_12_brace_all_rigs_a.png', f'{CAST}/previews/sheet_13_brace_all_rigs_b.png', ISSUE]}
    (ROOT / r2 / 'asset.json').write_text(json.dumps(rec, indent=2) + '\n')
    print('wrote', r2 + '/asset.json')
