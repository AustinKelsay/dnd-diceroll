#!/usr/bin/env python3
"""Write the asset records for batch 3 (stdlib only). Run after build3.py and review3.py validate:
    python3 Art/rig-pilot/cast/scripts/make_records3.py

Writes Art/<asset-id>/r001/asset.json for the sorcerer, warlock and druid (new assets). The script REFUSES (non-zero exit, nothing written for
any character) unless every validation JSON it reads exists, has `pass` true and every entry of `checks` true: a failed check can never be
recorded as a passed validation.
"""
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
CAST = 'Art/rig-pilot/cast'
SHARED = [f'{CAST}/README_BATCH3.md'] + [f'{CAST}/scripts/{n}' for n in ('cast_characters.py', 'cast_batch2.py', 'cast_batch3.py', 'brace.py', 'build3.py', 'review3.py', 'make_records3.py')] + \
    ['Art/rig-pilot/r001/README.md'] + [f'Art/rig-pilot/r001/scripts/{n}' for n in ('rigkit.py', 'clips.py', 'characters.py')]
NEW = {
    'sk-hero-sorcerer': ('SK_Hero_Sorcerer', 'sorcerer', 'Sorcerer: agile body under a fitted crimson coat open at the front, black mantle, flame collar, ember veins on the sleeves, crackling crystal focus on weapon_socket, no staff', 19),
    'sk-hero-warlock': ('SK_Hero_Warlock', 'warlock', 'Warlock: agile body, long plum hooded coat, pale hood lining and trim, bone pauldron, pact tome on a chain and eye pendant, rod with a green orb on weapon_socket', 20),
    'sk-hero-druid': ('SK_Hero_Druid', 'druid', 'Druid: robed body, bark-brown tunic under a split moss-green robe, layered leaf mantle, small leafy circlet, gnarled staff with a glowing amber seed on weapon_socket', 21),
}
ISSUE = 'https://github.com/AustinKelsay/dnd-diceroll/issues/150'


def sha(p):
    return hashlib.sha256((ROOT / p).read_bytes()).hexdigest()


def load(p):
    return json.loads((ROOT / p).read_text())


def refuse(msg):
    sys.stderr.write(f'make_records3: REFUSED: {msg}\n')
    sys.exit(1)


valid = {}
for aid, (name, key, desc, sheet) in NEW.items():
    p = f'{CAST}/metrics/validate_{key}.json'
    if not (ROOT / p).exists():
        refuse(f'{p} is missing (run review3.py validate {key})')
    v = load(p)
    checks = v.get('checks')
    if not isinstance(checks, dict) or not checks:
        refuse(f'{p} has no checks table')
    failed = [k for k, ok in checks.items() if ok is not True]
    if v.get('pass') is not True or failed:
        refuse(f'{p} did not pass (pass={v.get("pass")}, failed={failed})')
    valid[aid] = v

for aid, (name, key, desc, sheet) in NEW.items():
    base = f'Art/{aid}/r001'
    v = valid[aid]
    files = [('source', f'{base}/{name}_source.blend')] + [('source', p) for p in SHARED]
    files += [('export', f'{base}/{name}.glb'), ('export', f'{base}/{name}_LOD1.glb'), ('export', f'{base}/{name}_LOD2.glb')]
    rec = {
        'schema_version': 1, 'asset_id': aid, 'revision': 1,
        'unreal_package': f'/Game/DnDDiceRoll/Theme/Characters/{name}',
        'issue': ISSUE,
        'provenance': {
            'author': 'dnd-diceroll project agents (Claude Opus 5.5 lead, Claude Sonnet 5.5 subagent) for the repository owner',
            'license': 'Project-owned original work; same terms as this repository',
            'origin': 'Authored by script in Blender; no third-party, downloaded, kit, AI-service or commercial-game content. The r001 look (palette, motifs) is imitated, no r001 mesh or texture is reused.'},
        'recipe': {
            'method': desc + '. Procedural meshes, atlas and skin weights from Art/rig-pilot/cast/scripts (build3.py, cast_batch3.py) on the r001 skeleton; the ten shared hand-keyed clips of Art/rig-pilot/r001/scripts/clips.py plus Brace (brace.py); no external motion data',
            'approved_reference': 'Gate 1 decision on #140 (25-bone skeleton); #147 pilot contract (Art/rig-pilot/r001/README.md); #150 batch 3 design calls by the graphics lead (class looks, rigged Rim 1.2 and Fill 1.0)',
            'parameters': {'skeleton': '25 bones, shared contract in Art/rig-pilot/r001/README.md (unchanged)', 'units': 'metres, +X forward, Z up, foot pivot at origin',
                           'fps': 30, 'atlas': '1024x1024, 1 material', 'vertex_color': 'COLOR_0 RGBA, alpha = glow mask',
                           'lods': 'LOD0 with clips; LOD1 and LOD2 as separate skinned glbs without clips',
                           'clips': 'ten: Idle, Walk, Melee, Ranged, Cast, Hit, Downed, Death, Recover, Brace',
                           'blender_validation': {'tris': v['lods'], 'lod_ratio': v['lod_ratio'], 'body_height_m': v['body_height_m_excluding_held_items'],
                                                  'seam_mm_idle_walk': [v['seam_mm_first_vs_last_max_vertex']['Idle'], v['seam_mm_first_vs_last_max_vertex']['Walk']],
                                                  'checks': v['checks']}},
            'model_revision': 'not applicable (procedural Blender scripts)'},
        'tools': {'blender': '5.2.3 LTS', 'imagemagick': '7 (contact sheets only)'},
        'files': [{'role': r, 'path': p, 'sha256': sha(p)} for r, p in files],
        'import_policy': {'name': 'dnd-diceroll-rigged-character', 'version': 1, 'settings': {
            'importer': 'glTF skeletal mesh; reuse the existing shared skeleton (SKEL_Humanoid from the #147 fighter import); no new skeleton and no clips are imported for this character',
            'shared_skeleton': '/Game/DnDDiceRoll/Theme/Characters/SKEL_Humanoid',
            'nanite': 'off, set explicitly (gate 1 pipeline rule)', 'lods': 'LOD1/LOD2 glbs imported as LOD meshes of the same skeletal mesh',
            'animations': 'the ten shared A_Humanoid_* clips are reused (nine from #147, A_Humanoid_Brace from #150 batch 2); the clips baked into this glb are not imported',
            'material': 'MI_* child of M_DiceRollTheme with rim and fill terms (rigged 1.2 and 1.0); glow from vertex alpha only',
            'texture_filter': 'trilinear, anisotropic 8', 'collision': 'none (presentation only)'}},
        'validation': {'state': 'passed', 'evidence': [
            f'{CAST}/metrics/validate_{key}.json', f'{CAST}/metrics/build_{key}.json',
            f'{CAST}/previews/sheet_18_batch3_turnaround.png', f'{CAST}/previews/sheet_{sheet:02d}_{key}_clips.png',
            ISSUE]},
    }
    (ROOT / base / 'asset.json').write_text(json.dumps(rec, indent=2) + '\n')
    print('wrote', base + '/asset.json')
