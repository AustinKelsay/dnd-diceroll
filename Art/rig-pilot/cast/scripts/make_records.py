#!/usr/bin/env python3
"""Write Art/<asset-id>/r001/asset.json for the cast characters (stdlib only). Run after build.py and review.py validate:
    python3 Art/rig-pilot/cast/scripts/make_records.py
"""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SHARED = ['Art/rig-pilot/cast/README.md'] + [f'Art/rig-pilot/cast/scripts/{n}' for n in ('cast_characters.py', 'build.py', 'review.py', 'make_records.py')] + \
    ['Art/rig-pilot/r001/README.md'] + [f'Art/rig-pilot/r001/scripts/{n}' for n in ('rigkit.py', 'clips.py', 'characters.py')]
CHARS = {
    'sk-hero-rogue': ('SK_Hero_Rogue', 'rogue', 'Rogue: agile body, hood and cloak, twin daggers on weapon_socket and shield_socket'),
    'sk-hero-cleric': ('SK_Hero_Cleric', 'cleric', 'Cleric: robed body with a steel cuirass and tabard, mace on weapon_socket, round shield on shield_socket'),
}


def sha(p):
    return hashlib.sha256((ROOT / p).read_bytes()).hexdigest()


for aid, (name, key, desc) in CHARS.items():
    base = f'Art/{aid}/r001'
    v = json.loads((ROOT / 'Art/rig-pilot/cast/metrics' / f'validate_{key}.json').read_text())
    files = [('source', f'{base}/{name}_source.blend')]
    files += [('source', p) for p in SHARED]
    files += [('export', f'{base}/{name}.glb'), ('export', f'{base}/{name}_LOD1.glb'), ('export', f'{base}/{name}_LOD2.glb')]
    rec = {
        'schema_version': 1, 'asset_id': aid, 'revision': 1,
        'unreal_package': f'/Game/DnDDiceRoll/Theme/Characters/{name}',
        'issue': 'https://github.com/AustinKelsay/dnd-diceroll/issues/150',
        'provenance': {
            'author': 'dnd-diceroll project agents (Claude Opus 5.5 lead, Claude Sonnet 5.5 subagent) for the repository owner',
            'license': 'Project-owned original work; same terms as this repository',
            'origin': 'Authored by script in Blender; no third-party, downloaded, kit, AI-service or commercial-game content. The r001 look (palette, motifs) is imitated, no r001 mesh or texture is reused.'},
        'recipe': {
            'method': desc + '. Procedural meshes, atlas and skin weights from Art/rig-pilot/cast/scripts (build.py, cast_characters.py) on the r001 skeleton; the nine shared hand-keyed clips of Art/rig-pilot/r001/scripts/clips.py; no external motion data',
            'approved_reference': 'Gate 1 decision on #140 (25-bone skeleton); #147 pilot contract (Art/rig-pilot/r001/README.md); gate 2 readability follow-up on #148/#138 (pale trim, lighter hood lining or sash, Rim 1.6 and Fill 1.25)',
            'parameters': {'skeleton': '25 bones, shared contract in Art/rig-pilot/r001/README.md (unchanged)', 'units': 'metres, +X forward, Z up, foot pivot at origin',
                           'fps': 30, 'atlas': '1024x1024, 1 material', 'vertex_color': 'COLOR_0 RGBA, alpha = glow mask',
                           'lods': 'LOD0 with clips; LOD1 and LOD2 as separate skinned glbs without clips',
                           'blender_validation': {'tris': v['lods'], 'lod_ratio': v['lod_ratio'], 'body_height_m': v['body_height_m_excluding_held_items'],
                                                  'seam_mm_idle_walk': [v['seam_mm_first_vs_last_max_vertex']['Idle'], v['seam_mm_first_vs_last_max_vertex']['Walk']]}},
            'model_revision': 'not applicable (procedural Blender scripts)'},
        'tools': {'blender': '5.2.3 LTS', 'imagemagick': '7 (contact sheets only)'},
        'files': [{'role': r, 'path': p, 'sha256': sha(p)} for r, p in files],
        'import_policy': {'name': 'dnd-diceroll-rigged-character', 'version': 1, 'settings': {
            'importer': 'glTF skeletal mesh; reuse the existing shared skeleton (SKEL_Humanoid from the #147 fighter import); no new skeleton and no clips are imported for this character',
            'shared_skeleton': '/Game/DnDDiceRoll/Theme/Characters/SKEL_Humanoid',
            'nanite': 'off, set explicitly (gate 1 pipeline rule)', 'lods': 'LOD1/LOD2 glbs imported as LOD meshes of the same skeletal mesh',
            'animations': 'the nine shared A_Humanoid_* clips from #147 are reused; the clips baked into this glb are not imported',
            'material': 'MI_* child of M_DiceRollTheme with rim and fill terms; glow from vertex alpha only',
            'texture_filter': 'trilinear, anisotropic 8', 'collision': 'none (presentation only)'}},
        'validation': {'state': 'passed', 'evidence': [
            f'Art/rig-pilot/cast/metrics/validate_{key}.json',
            f'Art/rig-pilot/cast/metrics/build_{key}.json',
            'Art/rig-pilot/cast/previews/sheet_02_cast_turnaround.png',
            f'Art/rig-pilot/cast/previews/sheet_0{3 if key == "rogue" else 4}_{key}_clips.png',
            'https://github.com/AustinKelsay/dnd-diceroll/issues/150']},
    }
    (ROOT / base / 'asset.json').write_text(json.dumps(rec, indent=2) + '\n')
    print('wrote', base + '/asset.json')
