#!/usr/bin/env python3
"""Write Art/<asset-id>/r001/asset.json for the three rigged characters (stdlib only). Run after build.py:
    python3 Art/rig-pilot/r001/scripts/make_records.py
"""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SHARED = ['README.md'] + [f'scripts/{n}' for n in ('rigkit.py', 'clips.py', 'characters.py', 'build.py', 'review.py', 'make_records.py')]
CHARS = {
    'sk-hero-fighter': ('SK_Hero_Fighter', 'fighter', 'Fighter: heavy body, kite shield and longsword on the sockets'),
    'sk-hero-wizard': ('SK_Hero_Wizard', 'wizard', 'Wizard: robed body, staff on weapon_socket'),
    'sk-foe-raider': ('SK_Foe_Raider', 'raider', 'Raider: heavy body, antlered helm, axe on weapon_socket'),
}


def sha(p):
    return hashlib.sha256((ROOT / p).read_bytes()).hexdigest()


for aid, (name, key, desc) in CHARS.items():
    base = f'Art/{aid}/r001'
    v = json.loads((ROOT / 'Art/rig-pilot/r001/metrics' / f'validate_{key}.json').read_text())
    files = [('source', f'{base}/{name}_source.blend')]
    files += [('source', f'Art/rig-pilot/r001/{s}') for s in SHARED]
    files += [('export', f'{base}/{name}.glb'), ('export', f'{base}/{name}_LOD1.glb'), ('export', f'{base}/{name}_LOD2.glb')]
    rec = {
        'schema_version': 1, 'asset_id': aid, 'revision': 1,
        'unreal_package': f'/Game/DnDDiceRoll/Theme/Characters/{name}',
        'issue': 'https://github.com/AustinKelsay/dnd-diceroll/issues/147',
        'provenance': {
            'author': 'dnd-diceroll project agents (Claude Opus 5.5 lead, Claude Sonnet 5.5 subagent) for the repository owner',
            'license': 'Project-owned original work; same terms as this repository',
            'origin': 'Authored by script in Blender; no third-party, downloaded, kit, AI-service or commercial-game content. The r001 look (palette, motifs) is imitated, no r001 mesh or texture is reused.'},
        'recipe': {
            'method': desc + '. Procedural meshes, atlas and skin weights from Art/rig-pilot/r001/scripts (build.py); nine hand-keyed clips in clips.py; no external motion data',
            'approved_reference': 'Gate 1 decision on #140 (25-bone skeleton, FOV 40 camera, readability flagged); r001 hero/foe looks',
            'parameters': {'skeleton': '25 bones, shared contract in Art/rig-pilot/r001/README.md', 'units': 'metres, +X forward, Z up, foot pivot at origin',
                           'fps': 30, 'atlas': '1024x1024, 1 material', 'vertex_color': 'COLOR_0 RGBA, alpha = glow mask',
                           'lods': 'LOD0 with clips; LOD1 and LOD2 as separate skinned glbs without clips',
                           'blender_validation': {'tris': v['lods'], 'lod_ratio': v['lod_ratio'], 'body_height_m': v['body_height_m_excluding_held_items'],
                                                  'seam_mm_idle_walk': [v['seam_mm_first_vs_last_max_vertex']['Idle'], v['seam_mm_first_vs_last_max_vertex']['Walk']]}},
            'model_revision': 'not applicable (procedural Blender scripts)'},
        'tools': {'blender': '5.2.3 LTS', 'imagemagick': '7 (contact sheets only)'},
        'files': [{'role': r, 'path': p, 'sha256': sha(p)} for r, p in files],
        'import_policy': {'name': 'dnd-diceroll-rigged-character', 'version': 1, 'settings': {
            'importer': 'glTF skeletal mesh; import SK_Hero_Fighter first to create the shared skeleton, then reuse it for the others',
            'shared_skeleton': '/Game/DnDDiceRoll/Theme/Characters/SKEL_Humanoid',
            'nanite': 'off, set explicitly (gate 1 pipeline rule)', 'lods': 'LOD1/LOD2 glbs imported as LOD meshes of the same skeletal mesh',
            'animations': 'import the nine clips from the LOD0 glb only; 30 fps; root stays at the origin',
            'material': 'MI_* child of M_DiceRollTheme with rim and fill terms; glow from vertex alpha only',
            'texture_filter': 'trilinear, anisotropic 8', 'collision': 'none (presentation only)'}},
        'validation': {'state': 'pending', 'evidence': []},
    }
    (ROOT / base / 'asset.json').write_text(json.dumps(rec, indent=2) + '\n')
    print('wrote', base + '/asset.json')
