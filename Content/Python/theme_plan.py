"""Pure-Python half of the theme import (no `unreal` import, so unit tests can run it): what the Drowned Vigil
world kit r002, the Root of the World kit r002 and the Vault kit r002 import, which existing assets a run may replace, and how to read an export.

Used by import_dark_fantasy_theme.py (issue #145 phase 2 for the Vigil, #149 phase 2 for the Root and the Vault). The rules:
  - Every r002 asset has a table row (R002 for the Vigil, ROOT_R002 for the Root, VAULT_R002 for the Vault). A row whose package already exists is
    SKIPPED, never overwritten.
  - Only the ids in REPLACEABLE (the four revised Vigil r001 packages), ROOT_REPLACEABLE (the Root world) and ROSE_REPLACEABLE (the Vigil occluders with the
    high-resolution rose, #149) may be replaced, and
    only when the run is told so explicitly with the environment variable DND_THEME_REIMPORT=<id>[,<id>...] (or `all`, meaning
    exactly every replaceable id). An id outside those lists is an error, not a silent no-op.
  - Nothing here touches other r001 assets.
"""
import hashlib
import json
import os
import re
import struct

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
REIMPORT_ENV = "DND_THEME_REIMPORT"
NANITE_FIX_ENV = "DND_THEME_FIX_R001_NANITE"
REVISION_TAG = "DiceRollAssetRevision"

# Kinds pick the material parent: lit = M_DiceRollTheme, occluder = M_DiceRollOccluder (dithered mask), card = M_DiceRollCard.
LIT, OCCLUDER, CARD = "lit", "occluder", "card"

# (asset id, revision, export name, folder, kind, theme property, index or None, (roughness, rim, fill, glow, self_lit))
# Property + index: an index puts the mesh in that slot of an array property (variants, scatter, mist, shafts).
STONE = (0.92, 0.0, 0.05, 1.0, 0.0)
PILLAR = (0.9, 0.0, 0.05, 1.0, 0.0)
WALL = (0.9, 0.0, 0.05, 1.0, 0.0)
GROUND = (0.9, 0.0, 0.0, 1.0, 0.0)
CARDLOOK = (1.0, 0.0, 0.0, 0.0, 0.0)
R002 = [
    ("world-vigil", 2, "SM_World_Vigil", "Worlds", LIT, "vigil_world", None, GROUND),
    ("world-vigil-occluders", 2, "SM_World_Vigil_Occluders", "Worlds", OCCLUDER, "vigil_occluders", None, GROUND),
    ("prop-pillar", 2, "SM_Prop_Pillar", "Props", LIT, "pillar_variants", 0, PILLAR),
    ("prop-pillar-b", 1, "SM_Prop_Pillar_B", "Props", LIT, "pillar_variants", 1, PILLAR),
    ("prop-pillar-c", 1, "SM_Prop_Pillar_C", "Props", LIT, "pillar_variants", 2, PILLAR),
    ("prop-wall", 2, "SM_Prop_Wall", "Props", LIT, "wall_variants", 0, WALL),
    ("prop-wall-b", 1, "SM_Prop_Wall_B", "Props", LIT, "wall_variants", 1, WALL),
    ("prop-wall-c", 1, "SM_Prop_Wall_C", "Props", LIT, "wall_variants", 2, WALL),
    ("prop-stones", 2, "SM_Prop_Stones", "Props", LIT, "stones_variants", 0, STONE),
    ("prop-stones-b", 1, "SM_Prop_Stones_B", "Props", LIT, "stones_variants", 1, STONE),
    ("prop-stones-c", 1, "SM_Prop_Stones_C", "Props", LIT, "stones_variants", 2, STONE),
    ("prop-arch-2cell", 1, "SM_Prop_Arch2Cell", "Props", LIT, "arch_two_cell", None, WALL),
    ("scatter-rubble", 1, "SM_Scatter_Rubble", "Scatter", LIT, "scatter", 0, (0.92, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-candles", 1, "SM_Scatter_Candles", "Scatter", LIT, "scatter", 1, (0.8, 0.0, 0.0, 1.0, 0.0)),
    ("scatter-bones", 1, "SM_Scatter_Bones", "Scatter", LIT, "scatter", 2, (0.85, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-roots", 1, "SM_Scatter_Roots", "Scatter", LIT, "scatter", 3, (1.0, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-weapons", 1, "SM_Scatter_Weapons", "Scatter", LIT, "scatter", 4, (0.5, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-puddle", 1, "SM_Scatter_Puddle", "Scatter", LIT, "scatter", 5, (0.25, 0.0, 0.0, 1.0, 0.0)),
    ("scatter-urn", 1, "SM_Scatter_Urn", "Scatter", LIT, "scatter", 6, (0.7, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-moss", 1, "SM_Scatter_Moss", "Scatter", LIT, "scatter", 7, (1.0, 0.0, 0.0, 1.0, 0.0)),
    ("fx-mist-card", 1, "SM_Fx_MistCard", "Fx", CARD, "mist_cards", 0, CARDLOOK),
    ("fx-mist-card-b", 1, "SM_Fx_MistCard_B", "Fx", CARD, "mist_cards", 1, CARDLOOK),
    ("fx-mist-card-c", 1, "SM_Fx_MistCard_C", "Fx", CARD, "mist_cards", 2, CARDLOOK),
    ("fx-light-shaft", 1, "SM_Fx_LightShaft", "Fx", CARD, "light_shafts", 0, CARDLOOK),
    ("fx-light-shaft-b", 1, "SM_Fx_LightShaft_B", "Fx", CARD, "light_shafts", 1, CARDLOOK),
]

# The Root of the World kit r002 (#149). Same columns and checks as the Vigil rows. Only the world revises an r001 package
# (SM_World_Root, kept at the same path); everything else is a new id with a new package, because the r001 pillar, wall and
# stones packages are shared with the Vigil and are revised there. The three obstacle recipes are the Root's spires and stumps
# (pillar), root curtains and logs (wall) and boulders (stones).
ROOT_STONE = (0.92, 0.0, 0.05, 1.0, 0.0)
ROOT_PILLAR = (0.92, 0.0, 0.05, 1.0, 0.0)
ROOT_WALL = (0.92, 0.0, 0.05, 1.0, 0.0)
ROOT_R002 = [
    ("world-root", 2, "SM_World_Root", "Worlds", LIT, "root_world", None, GROUND),
    ("world-root-occluders", 1, "SM_World_Root_Occluders", "Worlds", OCCLUDER, "root_occluders", None, GROUND),
    ("prop-root-pillar", 1, "SM_Prop_Root_Pillar", "Props", LIT, "root_pillar_variants", 0, ROOT_PILLAR),
    ("prop-root-pillar-b", 1, "SM_Prop_Root_Pillar_B", "Props", LIT, "root_pillar_variants", 1, ROOT_PILLAR),
    ("prop-root-pillar-c", 1, "SM_Prop_Root_Pillar_C", "Props", LIT, "root_pillar_variants", 2, ROOT_PILLAR),
    ("prop-root-wall", 1, "SM_Prop_Root_Wall", "Props", LIT, "root_wall_variants", 0, ROOT_WALL),
    ("prop-root-wall-b", 1, "SM_Prop_Root_Wall_B", "Props", LIT, "root_wall_variants", 1, ROOT_WALL),
    ("prop-root-wall-c", 1, "SM_Prop_Root_Wall_C", "Props", LIT, "root_wall_variants", 2, ROOT_WALL),
    ("prop-root-stones", 1, "SM_Prop_Root_Stones", "Props", LIT, "root_stones_variants", 0, ROOT_STONE),
    ("prop-root-stones-b", 1, "SM_Prop_Root_Stones_B", "Props", LIT, "root_stones_variants", 1, ROOT_STONE),
    ("prop-root-stones-c", 1, "SM_Prop_Root_Stones_C", "Props", LIT, "root_stones_variants", 2, ROOT_STONE),
    ("prop-root-arch-2cell", 1, "SM_Prop_Root_Arch2Cell", "Props", LIT, "root_arch_two_cell", None, ROOT_WALL),
    ("scatter-root-mushrooms", 1, "SM_Scatter_Root_Mushrooms", "Scatter", LIT, "root_scatter", 0, (0.7, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-root-leaves", 1, "SM_Scatter_Root_Leaves", "Scatter", LIT, "root_scatter", 1, (0.95, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-root-twigs", 1, "SM_Scatter_Root_Twigs", "Scatter", LIT, "root_scatter", 2, (0.95, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-root-tendrils", 1, "SM_Scatter_Root_Tendrils", "Scatter", LIT, "root_scatter", 3, (0.9, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-root-ferns", 1, "SM_Scatter_Root_Ferns", "Scatter", LIT, "root_scatter", 4, (0.95, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-root-pebbles", 1, "SM_Scatter_Root_Pebbles", "Scatter", LIT, "root_scatter", 5, (0.9, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-root-glowmoss", 1, "SM_Scatter_Root_GlowMoss", "Scatter", LIT, "root_scatter", 6, (1.0, 0.0, 0.0, 1.0, 0.0)),
    ("scatter-root-husks", 1, "SM_Scatter_Root_Husks", "Scatter", LIT, "root_scatter", 7, (0.9, 0.0, 0.05, 1.0, 0.0)),
    ("fx-root-mist-card", 1, "SM_Fx_Root_MistCard", "Fx", CARD, "root_mist_cards", 0, CARDLOOK),
    ("fx-root-mist-card-b", 1, "SM_Fx_Root_MistCard_B", "Fx", CARD, "root_mist_cards", 1, CARDLOOK),
    ("fx-root-mist-card-c", 1, "SM_Fx_Root_MistCard_C", "Fx", CARD, "root_mist_cards", 2, CARDLOOK),
    ("fx-root-light-shaft", 1, "SM_Fx_Root_LightShaft", "Fx", CARD, "root_light_shafts", 0, CARDLOOK),
    ("fx-root-light-shaft-b", 1, "SM_Fx_Root_LightShaft_B", "Fx", CARD, "root_light_shafts", 1, CARDLOOK),
]
ROOT_REPLACEABLE = {"world-root": "r002 world at true metric"}
# The Root kit is switched on (UDiceRollTheme::bRootMetricKit) only when every one of these carries the root-kit-r002 tag.
ROOT_METRIC_GATE = ("world-root",)
ROOT_ARRAY_SIZES = {"root_pillar_variants": 3, "root_wall_variants": 3, "root_stones_variants": 3, "root_scatter": 8, "root_mist_cards": 3, "root_light_shafts": 2}

# The Vault world kit r002 (#149): the finale's own world. Every row is a new id and a new package (the Vault never had an r001
# world: it drew the Vigil kit with a cold grade), so nothing here replaces an existing package. The three obstacle recipes are the
# Vault's broken columns, ossuary piers and rune steles (pillar), ashlar runs, stacked sarcophagi and iron grilles (wall) and rubble,
# bone mounds and toppled statues (stones). Same columns and checks as the Root rows.
VAULT_STONE = (0.92, 0.0, 0.05, 1.0, 0.0)
VAULT_PILLAR = (0.92, 0.0, 0.05, 1.0, 0.0)
VAULT_WALL = (0.92, 0.0, 0.05, 1.0, 0.0)
# The Waystone halo, its rune channels and the alcove lines are in the occluder mesh: Glow 0.25 keeps them from clipping against the bloom. At the default zoom the apse peak luma stays under 250; at far zoom up to 0.7% of the apse core may clip, an accepted landmark highlight (lead call on #149).
VAULT_OCCLUDER = (0.9, 0.0, 0.0, 0.25, 0.0)
VAULT_R002 = [
    ("world-vault", 1, "SM_World_Vault", "Worlds", LIT, "vault_world", None, GROUND),
    ("world-vault-occluders", 1, "SM_World_Vault_Occluders", "Worlds", OCCLUDER, "vault_occluders", None, VAULT_OCCLUDER),
    ("prop-vault-pillar", 1, "SM_Prop_Vault_Pillar", "Props", LIT, "vault_pillar_variants", 0, VAULT_PILLAR),
    ("prop-vault-pillar-b", 1, "SM_Prop_Vault_Pillar_B", "Props", LIT, "vault_pillar_variants", 1, VAULT_PILLAR),
    ("prop-vault-pillar-c", 1, "SM_Prop_Vault_Pillar_C", "Props", LIT, "vault_pillar_variants", 2, VAULT_PILLAR),
    ("prop-vault-wall", 1, "SM_Prop_Vault_Wall", "Props", LIT, "vault_wall_variants", 0, VAULT_WALL),
    ("prop-vault-wall-b", 1, "SM_Prop_Vault_Wall_B", "Props", LIT, "vault_wall_variants", 1, VAULT_WALL),
    ("prop-vault-wall-c", 1, "SM_Prop_Vault_Wall_C", "Props", LIT, "vault_wall_variants", 2, VAULT_WALL),
    ("prop-vault-stones", 1, "SM_Prop_Vault_Stones", "Props", LIT, "vault_stones_variants", 0, VAULT_STONE),
    ("prop-vault-stones-b", 1, "SM_Prop_Vault_Stones_B", "Props", LIT, "vault_stones_variants", 1, VAULT_STONE),
    ("prop-vault-stones-c", 1, "SM_Prop_Vault_Stones_C", "Props", LIT, "vault_stones_variants", 2, VAULT_STONE),
    ("prop-vault-arch-2cell", 1, "SM_Prop_Vault_Arch2Cell", "Props", LIT, "vault_arch_two_cell", None, VAULT_WALL),
    ("scatter-vault-bones", 1, "SM_Scatter_Vault_Bones", "Scatter", LIT, "vault_scatter", 0, (0.85, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-vault-skulls", 1, "SM_Scatter_Vault_Skulls", "Scatter", LIT, "vault_scatter", 1, (0.85, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-vault-candles", 1, "SM_Scatter_Vault_Candles", "Scatter", LIT, "vault_scatter", 2, (0.8, 0.0, 0.0, 1.0, 0.0)),
    ("scatter-vault-rubble", 1, "SM_Scatter_Vault_Rubble", "Scatter", LIT, "vault_scatter", 3, (0.92, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-vault-chains", 1, "SM_Scatter_Vault_Chains", "Scatter", LIT, "vault_scatter", 4, (0.55, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-vault-urns", 1, "SM_Scatter_Vault_Urns", "Scatter", LIT, "vault_scatter", 5, (0.7, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-vault-relics", 1, "SM_Scatter_Vault_Relics", "Scatter", LIT, "vault_scatter", 6, (0.5, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-vault-crystals", 1, "SM_Scatter_Vault_Crystals", "Scatter", LIT, "vault_scatter", 7, (0.35, 0.0, 0.0, 1.0, 0.0)),
    ("fx-vault-mist-card", 1, "SM_Fx_Vault_MistCard", "Fx", CARD, "vault_mist_cards", 0, CARDLOOK),
    ("fx-vault-mist-card-b", 1, "SM_Fx_Vault_MistCard_B", "Fx", CARD, "vault_mist_cards", 1, CARDLOOK),
    ("fx-vault-mist-card-c", 1, "SM_Fx_Vault_MistCard_C", "Fx", CARD, "vault_mist_cards", 2, CARDLOOK),
    ("fx-vault-light-shaft", 1, "SM_Fx_Vault_LightShaft", "Fx", CARD, "vault_light_shafts", 0, CARDLOOK),
    ("fx-vault-light-shaft-b", 1, "SM_Fx_Vault_LightShaft_B", "Fx", CARD, "vault_light_shafts", 1, CARDLOOK),
]
# The Vault kit is switched on (UDiceRollTheme::bVaultMetricKit) only when its world carries the vault-kit-r002 tag.
VAULT_METRIC_GATE = ("world-vault",)
VAULT_ARRAY_SIZES = {"vault_pillar_variants": 3, "vault_wall_variants": 3, "vault_stones_variants": 3, "vault_scatter": 8, "vault_mist_cards": 3, "vault_light_shafts": 2}

# The overview set r002 (#149, the last part): the war table and the three route dioramas, four new ids and new packages under /Game/DnDDiceRoll/Theme/Overview
# (the r001 SM_Tabletop, SM_Diorama_Root and SM_Diorama_Vigil stay as the fallback). Nothing in this table replaces an existing package. The look is the r001
# overview's (roughness, no rim, no fill, glow on, self-lit 0.3 for the table and 0.6 for a diorama: on the route screens they stand at the front corners, far from the candles): the table sits in candlelight and the meshes lift themselves a little.
OVERVIEW_R002 = [
    ("overview-tabletop", 1, "SM_Overview_Tabletop", "Overview", LIT, "overview_tabletop", None, (0.8, 0.0, 0.0, 1.0, 0.3)),
    ("overview-diorama-root", 1, "SM_Overview_Diorama_Root", "Overview", LIT, "overview_root_diorama", None, (0.9, 0.0, 0.0, 1.0, 0.6)),
    ("overview-diorama-vigil", 1, "SM_Overview_Diorama_Vigil", "Overview", LIT, "overview_vigil_diorama", None, (0.9, 0.0, 0.0, 1.0, 0.6)),
    ("overview-diorama-vault", 1, "SM_Overview_Diorama_Vault", "Overview", LIT, "overview_vault_diorama", None, (0.9, 0.0, 0.0, 1.0, 0.6)),
]
# The overview set is switched on (UDiceRollTheme::bOverviewR002) only when all four carry the overview-r002 tag; the view then also loads them as a set.
OVERVIEW_METRIC_GATE = tuple(row[0] for row in OVERVIEW_R002)
OVERVIEW_ARRAY_SIZES = {}

# The Vigil occluders carry the stained-glass rose in a high-resolution atlas region (#149): world-vigil-occluders r002 replaces the r001 export
# at the same package and texture, with its two LODs. The mesh is tagged ROSE_TAG afterwards, so a run can tell it from the first r002 import.
ROSE_REPLACEABLE = {"world-vigil-occluders": "r002: the stained-glass rose at about 220 texels/m (was 14)"}
ROSE_TAG = "vigil-kit-r002-rose"
# The tag a row's mesh must carry to count as imported; the kit's own tag unless the row has a newer revision of its own.
ROW_TAGS = {"world-vigil-occluders": ROSE_TAG}

# Per-instance `Color` tint (linear RGB) written on a kit mesh's MI_*; the lever for "the stone competes with the figures".
# UDiceRollTheme's view multiplies it into the colour the part draws with (InstanceTint in ExpeditionBattleView.cpp), so
# lowering a number here and rerunning the plain import changes only that MI. 1.0 is neutral: the paving was already
# recalibrated in Blender (mean luminance 0.199, 27 % below the first pass), so the starting value leaves it as exported.
# Note the tint multiplies the whole mesh including the glow mask's fungus caps and sap veins.
INSTANCE_TINTS = {"world-root": (1.0, 1.0, 1.0), "world-root-occluders": (1.0, 1.0, 1.0),
                  "world-vault": (1.0, 1.0, 1.0), "world-vault-occluders": (1.0, 1.0, 1.0)}

# The kit tags the import script writes on a replaced or created mesh (DiceRollAssetRevision).
VIGIL_TAG = "vigil-kit-r002"
ROOT_TAG = "root-kit-r002"
VAULT_TAG = "vault-kit-r002"
OVERVIEW_TAG = "overview-r002"
KIT_TAGS = (VIGIL_TAG, ROOT_TAG, VAULT_TAG, OVERVIEW_TAG, ROSE_TAG)

# The only existing r001 packages a run may replace, each with the reason. They keep their package path (the asset
# records say so: a rename is a separate coordinated migration), so the r002 mesh replaces the r001 mesh in place.
REPLACEABLE = {
    "world-vigil": "r002 world at true metric",
    "prop-pillar": "variant A rebuilt at the 150 cm cell",
    "prop-wall": "variant A rebuilt at the 150 cm cell",
    "prop-stones": "variant A rebuilt at the 150 cm cell",
}
# The Vigil kit is switched on (UDiceRollTheme::bMetricKit) only when every one of these carries the r002 tag.
METRIC_GATE = ("world-vigil", "prop-pillar", "prop-wall", "prop-stones")
ARRAY_SIZES = {"pillar_variants": 3, "wall_variants": 3, "stones_variants": 3, "scatter": 8, "mist_cards": 3, "light_shafts": 2}


def export_path(asset_id, revision, name):
    return os.path.join(ROOT, "Art", asset_id, "r%03d" % revision, name + ".glb")


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


ALL_REPLACEABLE = {**REPLACEABLE, **ROOT_REPLACEABLE, **ROSE_REPLACEABLE}


def requested_replacements(value):
    """The set of asset ids a run may replace, from DND_THEME_REIMPORT. Raises on an id outside REPLACEABLE, ROOT_REPLACEABLE or ROSE_REPLACEABLE."""
    value = (value or "").strip()
    if not value:
        return set()
    if value.lower() == "all":
        return set(ALL_REPLACEABLE)
    ids = {part.strip() for part in re.split(r"[,\s]+", value) if part.strip()}
    unknown = sorted(ids - set(ALL_REPLACEABLE))
    if unknown:
        raise ValueError("Not replaceable (allow-list is %s): %s" % (", ".join(sorted(ALL_REPLACEABLE)), ", ".join(unknown)))
    return ids


def check_import_result(paths, package, filename):
    """Validates what an import task reported (task.imported_object_paths) for a reimport over `package`.

    The original object stays loaded when an import fails, so identity alone proves nothing: the import must have reported the
    expected package, and nothing else. Raises RuntimeError otherwise.
    """
    packages = [str(path).split(".")[0] for path in paths]
    if package not in packages:
        raise RuntimeError("Reimporting {} did not report {} among its imported objects ({}); the import failed".format(filename, package, packages))
    strays = [path for path in packages if path != package]
    if strays:
        raise RuntimeError("Reimporting {} over {} also created {}; remove them and fix the import".format(filename, package, strays))


def lod_exports(asset_id, revision, name):
    """Explicit LOD exports beside an export: <name>_LOD1.glb, <name>_LOD2.glb ... in order. Empty when none are authored."""
    folder = os.path.dirname(export_path(asset_id, revision, name))
    found = {}
    if os.path.isdir(folder):
        for entry in os.listdir(folder):
            match = re.fullmatch(re.escape(name) + r"_LOD(\d+)\.glb", entry)
            if match and int(match.group(1)) >= 1:
                found[int(match.group(1))] = os.path.join(folder, entry)
    return [found[key] for key in sorted(found)]


def record_lods(asset_id, revision):
    """The LOD exports (index, repository path, sha256) an asset record lists, in LOD order; empty for the exempt (cards, small scatter)."""
    with open(os.path.join(ROOT, "Art", asset_id, "r%03d" % revision, "asset.json")) as handle:
        record = json.load(handle)
    found = []
    for entry in record["files"]:
        match = re.search(r"_LOD(\d+)\.glb$", entry["path"])
        if entry["role"] == "export" and match:
            found.append((int(match.group(1)), entry["path"], entry["sha256"]))
    return sorted(found)


def read_glb(path):
    with open(path, "rb") as handle:
        data = handle.read()
    if data[:4] != b"glTF":
        raise ValueError(path + " is not a binary glTF file")
    json_length = struct.unpack("<I", data[12:16])[0]
    header = json.loads(data[20:20 + json_length])
    binary_at = 20 + json_length
    binary = data[binary_at + 8:binary_at + 8 + struct.unpack("<I", data[binary_at:binary_at + 4])[0]]
    return header, binary


_FORMATS = {5126: ("f", 4, 1.0), 5121: ("B", 1, 255.0), 5123: ("H", 2, 65535.0)}


def has_alpha_mask(path):
    """True when the export's COLOR_0 is VEC4 and its alpha really varies (a glow mask or opacity), else False.

    glTF COLOR_0 is optional and often RGB only; Unreal then reads alpha 1 everywhere, which would make a whole mesh
    glow. Only a VEC4 colour with a spread of alpha counts as a mask.
    """
    if not os.path.isfile(path):
        return False
    header, binary = read_glb(path)
    for mesh in header.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            index = primitive.get("attributes", {}).get("COLOR_0")
            if index is None:
                continue
            accessor = header["accessors"][index]
            if accessor["type"] != "VEC4":
                continue
            view = header["bufferViews"][accessor["bufferView"]]
            code, size, divisor = _FORMATS[accessor["componentType"]]
            start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
            stride = view.get("byteStride", 4 * size)
            alphas = [struct.unpack_from("<" + code, binary, start + i * stride + 3 * size)[0] / divisor for i in range(accessor["count"])]
            if alphas and max(alphas) - min(alphas) > 0.02 and min(alphas) < 0.99:
                return True
    return False


def triangle_count(path):
    """Triangles in an export (indexed triangle lists only; the exporters write nothing else)."""
    header, _ = read_glb(path)
    total = 0
    for mesh in header.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            if primitive.get("mode", 4) != 4:
                raise ValueError("Unsupported primitive mode in " + path)
            accessor = header["accessors"][primitive["indices"]] if "indices" in primitive else header["accessors"][primitive["attributes"]["POSITION"]]
            total += accessor["count"] // 3
    return total


def package_path(folder, name):
    return "/Game/DnDDiceRoll/Theme/%s/%s" % (folder, name)
