"""Import the recorded expedition cues into /Game/DnDDiceRoll/Audio (issue #242).

Run headless with the editor closed (under the Ultra machine lock):
  UnrealEditor-Cmd DnDDiceRoll.uproject -run=pythonscript -script=<repo>/Content/Python/import_expedition_cues.py -unattended -nullrhi -corelimit=2

Reads Art/audio-sources/r001/recipe.json (cue order, variation counts and volumes) and the 16-bit 48 kHz WAVs that
tools/audio_cues.py rendered beside it in cues/, then:
  - creates /Game/DnDDiceRoll/Audio/SC_Effects, a plain SoundClass every cue plays through (the stub #246 builds the mix on);
  - imports each <Cue>_<NN>.wav as /Game/DnDDiceRoll/Audio/Cues/SW_<Cue>_<NN>, tagged DiceRollSourceSha256 with the WAV's hash,
    and sets its sound class to SC_Effects;
  - creates or updates /Game/DnDDiceRoll/Audio/DA_ExpeditionCueSounds (UExpeditionCueSounds): each cue's variations, in order, and volume.
Reruns preserve existing assets: a wave whose tag matches its WAV is left alone; one whose WAV changed is reported (CUES_STALE) and
replaced only when DND_CUES_REIMPORT=1. The data asset and sound class assignments are written only when they differ. Only dirty
packages are saved, and a failed save raises. The last line is CUES_IMPORT_COMPLETE <cues> cues, <waves> waves.
"""
import hashlib
import json
import os
import re

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SOURCE_DIR = os.path.join(ROOT, "Art", "audio-sources", "r001")
AUDIO = "/Game/DnDDiceRoll/Audio"
CUES = AUDIO + "/Cues"
SOUND_CLASS = AUDIO + "/SC_Effects"
DATA_ASSET = AUDIO + "/DA_ExpeditionCueSounds"
HASH_TAG = "DiceRollSourceSha256"
REIMPORT = os.environ.get("DND_CUES_REIMPORT") == "1"

library = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def sha256(path):
    with open(path, "rb") as stream:
        return hashlib.sha256(stream.read()).hexdigest()


def enum_value(cue):
    """EExpeditionCue::DiceRoll is unreal.ExpeditionCue.DICE_ROLL in Python."""
    return getattr(unreal.ExpeditionCue, re.sub(r"(?<!^)(?=[A-Z])", "_", cue).upper())


def save(path):
    if not library.save_asset(path, only_if_is_dirty=True):
        raise RuntimeError("Unable to save " + path)


def sound_class():
    if library.does_asset_exist(SOUND_CLASS):
        unreal.log("CUES_ASSET kept " + SOUND_CLASS)
        return unreal.load_asset(SOUND_CLASS)
    created = tools.create_asset("SC_Effects", AUDIO, unreal.SoundClass, unreal.SoundClassFactory())
    if not created:
        raise RuntimeError("Unable to create " + SOUND_CLASS)
    save(SOUND_CLASS)
    unreal.log("CUES_ASSET created " + SOUND_CLASS)
    return created


def import_wave(source, name, effects):
    package = CUES + "/" + name
    digest = sha256(source)
    if library.does_asset_exist(package):
        wave = unreal.load_asset(package)
        if library.get_metadata_tag(wave, HASH_TAG) == digest:
            action = "kept"
        elif not REIMPORT:
            unreal.log_warning("CUES_STALE {} differs from {}; rerun with DND_CUES_REIMPORT=1 to replace it".format(package, source))
            action = "stale"
        else:
            wave = None
            action = "replaced"
    else:
        wave = None
        action = "imported"
    if wave is None:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source)
        task.set_editor_property("destination_path", CUES)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        tools.import_asset_tasks([task])
        waves = [unreal.load_asset(path) for path in task.get_editor_property("imported_object_paths")]
        waves = [asset for asset in waves if isinstance(asset, unreal.SoundWave)]
        if len(waves) != 1:
            raise RuntimeError("Expected one SoundWave from {}, got {}".format(source, waves))
        wave = waves[0]
        library.set_metadata_tag(wave, HASH_TAG, digest)
        wave.modify()
    if wave.get_editor_property("sound_class_object") != effects:
        wave.set_editor_property("sound_class_object", effects)
        wave.modify()
    save(package)
    unreal.log("CUES_ASSET {} {}".format(action, package))
    return wave


def data_asset():
    if library.does_asset_exist(DATA_ASSET):
        return unreal.load_asset(DATA_ASSET), "kept"
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.ExpeditionCueSounds)
    created = tools.create_asset("DA_ExpeditionCueSounds", AUDIO, unreal.ExpeditionCueSounds, factory)
    if not created:
        raise RuntimeError("Unable to create " + DATA_ASSET)
    return created, "created"


def describe(cues_map):
    """A comparable snapshot of the asset's map: cue name -> (variation paths, volume)."""
    snapshot = {}
    for key, entry in cues_map.items():
        paths = tuple(str(variation.get_path_name()) if variation else "" for variation in entry.get_editor_property("variations"))
        snapshot[str(key)] = (paths, round(float(entry.get_editor_property("volume")), 4))
    return snapshot


def main():
    with open(os.path.join(SOURCE_DIR, "recipe.json")) as stream:
        recipe = json.load(stream)
    effects = sound_class()
    desired = unreal.Map(unreal.ExpeditionCue, unreal.ExpeditionCueSound)
    total = 0
    for cue in recipe["cue_order"]:
        plan = recipe["cues"][cue]
        waves = []
        for index in range(len(plan["variations"])):
            name = "{}_{:02d}".format(cue, index + 1)
            source = os.path.join(SOURCE_DIR, "cues", name + ".wav")
            if not os.path.isfile(source):
                raise RuntimeError("Missing rendered cue {}; run tools/audio_cues.py build".format(source))
            waves.append(import_wave(source, "SW_" + name, effects))
        total += len(waves)
        entry = unreal.ExpeditionCueSound()
        entry.set_editor_property("variations", waves)
        entry.set_editor_property("volume", float(plan["volume"]))
        desired[enum_value(cue)] = entry
    asset, action = data_asset()
    changed = action == "created"
    if describe(asset.get_editor_property("cues")) != describe(desired):
        asset.set_editor_property("cues", desired)
        changed = True
    if asset.get_editor_property("effects_class") != effects:
        asset.set_editor_property("effects_class", effects)
        changed = True
    if changed:
        asset.modify()
        action = "updated" if action == "kept" else action
    save(DATA_ASSET)
    unreal.log("CUES_ASSET {} {}".format(action, DATA_ASSET))
    unreal.log("CUES_IMPORT_COMPLETE {} cues, {} waves".format(len(recipe["cue_order"]), total))


main()
