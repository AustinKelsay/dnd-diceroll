#!/usr/bin/env python3
"""Write the asset records for the foe batch (stdlib only). Run after build_foes.py and review_foes.py validate:
    python3 Art/rig-pilot/cast/scripts/make_records_foes.py

Writes Art/<asset-id>/r001/asset.json for the scout, archer, skirmisher, brute and warden (new assets). The script REFUSES (non-zero exit, nothing written for
any character) unless every validation JSON it reads exists, has `pass` true and every entry of `checks` true: a failed check can never be recorded as a
passed validation. Every script and doc that produced the assets is listed as a hashed source file.
"""
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
CAST = 'Art/rig-pilot/cast'
SHARED = [f'{CAST}/README_FOES.md'] + [f'{CAST}/scripts/{n}' for n in (
    'cast_characters.py', 'cast_batch2.py', 'cast_foes.py', 'brace.py', 'overrides.py', 'overrides_foes.py', 'build_foes.py', 'review_foes.py',
    'review2.py', 'review.py', 'make_records_foes.py')] + \
    ['Art/rig-pilot/r001/README.md'] + [f'Art/rig-pilot/r001/scripts/{n}' for n in ('rigkit.py', 'clips.py', 'characters.py', 'review.py')]
NEW = {
    'sk-foe-scout': ('SK_Foe_Scout', 'scout', 'Scout: agile body in tan hide, hide cowl lined in red with a red scarf over the lower face, bone charms, hide mantle and pelt, red sash and loincloth, short leaf-bladed spear with red tassels on weapon_socket', (2, 4), None),
    'sk-foe-archer': ('SK_Foe_Archer', 'archer', 'Archer: agile body in tan hide, hide hood lined in red with a bone-white mask, bone-fletched red-flighted arrows in a quiver, hunting knife on weapon_socket (hidden during the draw), shortbow on shield_socket authored at the Ranged_Archer hold', (2, 5), ('Ranged_Archer', 'Ranged', 36, 11)),
    'sk-foe-skirmisher': ('SK_Foe_Skirmisher', 'skirmisher', 'Skirmisher: lean agile body with bare skin painted with a red war stripe, hide harness, bone fetishes and feather crest, red streamers, twin sickles (right on weapon_socket, left on shield_socket)', (2, 6), None),
    'sk-foe-brute': ('SK_Foe_Brute', 'brute', 'Brute: heavy body bulked like the barbarian but hostile, bare painted skin, huge spiked hide pauldron, red ragged kilt and cape, raider-family helm with bull horns, stone maul on weapon_socket carried two-handed in Melee_Brute', (3, 7), ('Melee_Brute', 'Melee', 30, 12)),
    'sk-foe-warden': ('SK_Foe_Warden', 'warden', 'Warden: heavy body in cold pale stone and verdigris, massive finned pauldrons, Vault-teal tabard and cape, great helm with a cyan glowing mask and a crown with a tall crest, tower shield on shield_socket, halberd on weapon_socket', (3, 8), None),
}
ISSUE = 'https://github.com/AustinKelsay/dnd-diceroll/issues/150'


def sha(p):
    return hashlib.sha256((ROOT / p).read_bytes()).hexdigest()


def load(p):
    return json.loads((ROOT / p).read_text())


def refuse(msg):
    sys.stderr.write(f'make_records_foes: REFUSED: {msg}\n')
    sys.exit(1)


valid = {}
for aid, (name, key, desc, sheets, ov) in NEW.items():
    p = f'{CAST}/metrics/validate_{key}.json'
    if not (ROOT / p).exists():
        refuse(f'{p} is missing (run review_foes.py validate {key})')
    v = load(p)
    checks = v.get('checks')
    if not isinstance(checks, dict) or not checks:
        refuse(f'{p} has no checks table')
    failed = [k for k, ok in checks.items() if ok is not True]
    if v.get('pass') is not True or failed:
        refuse(f'{p} did not pass (pass={v.get("pass")}, failed={failed})')
    valid[aid] = v

for aid, (name, key, desc, sheets, ov) in NEW.items():
    base = f'Art/{aid}/r001'
    v = valid[aid]
    files = [('source', f'{base}/{name}_source.blend')] + [('source', p) for p in SHARED]
    files += [('export', f'{base}/{name}.glb'), ('export', f'{base}/{name}_LOD1.glb'), ('export', f'{base}/{name}_LOD2.glb')]
    clips = 'ten: Idle, Walk, Melee, Ranged, Cast, Hit, Downed, Death, Recover, Brace' + (f'; plus the class override {ov[0]} (replaces {ov[1]}, {ov[2]} frames)' if ov else '')
    animations = ('the ten shared A_Humanoid_* clips are reused (nine from #147, A_Humanoid_Brace from #150 batch 2); '
                  + (f'{ov[0]} is imported as A_{key.capitalize()}_{ov[1]} and named by the rig definition\'s Overrides; ' if ov else '')
                  + 'the shared clips baked into this glb are not imported')
    rec = {
        'schema_version': 1, 'asset_id': aid, 'revision': 1,
        'unreal_package': f'/Game/DnDDiceRoll/Theme/Characters/{name}',
        'issue': ISSUE,
        'provenance': {
            'author': 'dnd-diceroll project agents (Claude Opus 5.5 lead, Claude Sonnet 5.5 subagent) for the repository owner',
            'license': 'Project-owned original work; same terms as this repository',
            'origin': 'Authored by script in Blender; no third-party, downloaded, kit, AI-service or commercial-game content. The r001 raider look (tan hide, red cloth, bone-white) is imitated, no r001 mesh or texture is reused.'},
        'recipe': {
            'method': desc + '. Procedural meshes, atlas and skin weights from Art/rig-pilot/cast/scripts (build_foes.py, cast_foes.py) on the r001 skeleton; the ten shared hand-keyed clips of Art/rig-pilot/r001/scripts/clips.py plus Brace (brace.py)'
                      + (f'; {ov[0]} from overrides_foes.py (a reuse of overrides.py\'s IK solver); ' if ov else '; ') + 'no external motion data',
            'approved_reference': 'Gate 1 decision on #140 (25-bone skeleton); #147 pilot contract (Art/rig-pilot/r001/README.md); #150 foe batch design calls by the graphics lead (hostile palette family, silhouettes, height bands, rigged Rim 1.2 and Fill 1.0); #188 encounter-identity contract for the skirmisher, brute and warden roles',
            'parameters': {'skeleton': '25 bones, shared contract in Art/rig-pilot/r001/README.md (unchanged)', 'units': 'metres, +X forward, Z up, foot pivot at origin',
                           'fps': 30, 'atlas': '1024x1024, 1 material', 'vertex_color': 'COLOR_0 RGBA, alpha = glow mask',
                           'lods': 'LOD0 with clips; LOD1 and LOD2 as separate skinned glbs without clips',
                           'clips': clips,
                           'blender_validation': {'tris': v['lods'], 'lod_ratio': v['lod_ratio'], 'body_height_m': v['body_height_m_excluding_held_items'],
                                                  'seam_mm_idle_walk': [v['seam_mm_first_vs_last_max_vertex']['Idle'], v['seam_mm_first_vs_last_max_vertex']['Walk']],
                                                  'checks': v['checks']}},
            'model_revision': 'not applicable (procedural Blender scripts)'},
        'tools': {'blender': '5.2.3 LTS', 'imagemagick': '7 (contact sheets only)'},
        'files': [{'role': r, 'path': p, 'sha256': sha(p)} for r, p in files],
        'import_policy': {'name': 'dnd-diceroll-rigged-character', 'version': 1, 'settings': {
            'importer': 'glTF skeletal mesh; reuse the existing shared skeleton (SKEL_Humanoid from the #147 fighter import); no new skeleton',
            'shared_skeleton': '/Game/DnDDiceRoll/Theme/Characters/SKEL_Humanoid',
            'nanite': 'off, set explicitly (gate 1 pipeline rule)', 'lods': 'LOD1/LOD2 glbs imported as LOD meshes of the same skeletal mesh',
            'animations': animations,
            'material': 'MI_* child of M_DiceRollTheme with rim and fill terms (rigged 1.2 and 1.0); glow from vertex alpha only',
            'texture_filter': 'trilinear, anisotropic 8', 'collision': 'none (presentation only)'}},
        'validation': {'state': 'passed', 'evidence': [
            f'{CAST}/metrics/validate_{key}.json', f'{CAST}/metrics/build_{key}.json',
            f'{CAST}/previews/sheet_f{sheets[0]:02d}_foes_turnaround_{"a" if sheets[0] == 2 else "b"}.png', f'{CAST}/previews/sheet_f{sheets[1]:02d}_{key}_clips.png',
            *( [f'{CAST}/previews/sheet_f{ov[3]}_{key}_override_clip.png'] if ov else []),
            ISSUE]},
    }
    (ROOT / base / 'asset.json').write_text(json.dumps(rec, indent=2) + '\n')
    print('wrote', base + '/asset.json')
