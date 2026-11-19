"""Import the ambience beds into /Game/DnDDiceRoll/Audio (issue #244).

Run headless with the editor closed (under the Ultra machine lock):
  UnrealEditor-Cmd DnDDiceRoll.uproject -run=pythonscript -script=<repo>/Content/Python/import_expedition_ambience.py -unattended -nullrhi -corelimit=2

Reads Art/audio-ambience/r001/recipe.json (bed order and volumes) and the 16-bit 48 kHz stereo loops that tools/audio_beds.py rendered
beside it in beds/, then:
  - creates /Game/DnDDiceRoll/Audio/SC_Ambience, the sound class every bed plays through (#246 builds the mix on it);
  - imports each <Bed>.wav as /Game/DnDDiceRoll/Audio/Ambience/SW_Amb_<Bed>, tagged DiceRollSourceSha256 with the WAV's hash, looping,
    kept playing while silent (a bed fades in from zero), and in SC_Ambience;
  - creates or updates /Game/DnDDiceRoll/Audio/DA_ExpeditionAmbience (UExpeditionAmbienceSounds): each bed's loop and volume.
It never touches the cue assets of #242/#243. Reruns preserve existing assets: a wave whose tag matches its WAV is left alone; one whose
WAV changed is reported (AMBIENCE_STALE) and replaced only when DND_AMBIENCE_REIMPORT=1. Properties are written only when they differ,
only dirty packages are saved, and a failed save raises. The last line is AMBIENCE_IMPORT_COMPLETE <beds> beds.
"""
import hashlib
import json
import os
import re

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SOURCE_DIR = os.path.join(ROOT, "Art", "audio-ambience", "r001")
AUDIO = "/Game/DnDDiceRoll/Audio"
BEDS = AUDIO + "/Ambience"
SOUND_CLASS = AUDIO + "/SC_Ambience"
DATA_ASSET = AUDIO + "/DA_ExpeditionAmbience"
HASH_TAG = "DiceRollSourceSha256"
REIMPORT = os.environ.get("DND_AMBIENCE_REIMPORT") == "1"

library = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def sha256(path):
    with open(path, "rb") as stream:
        return hashlib.sha256(stream.read()).hexdigest()


def enum_value(bed):
    """EExpeditionAmbience::WarTable is unreal.ExpeditionAmbience.WAR_TABLE in Python."""
    return getattr(unreal.ExpeditionAmbience, re.sub(r"(?<!^)(?=[A-Z])", "_", bed).upper())


def save(path):
    if not library.save_asset(path, only_if_is_dirty=True):
        raise RuntimeError("Unable to save " + path)


def sound_class():
    if library.does_asset_exist(SOUND_CLASS):
        unreal.log("AMBIENCE_ASSET kept " + SOUND_CLASS)
        return unreal.load_asset(SOUND_CLASS)
    created = tools.create_asset("SC_Ambience", AUDIO, unreal.SoundClass, unreal.SoundClassFactory())
    if not created:
        raise RuntimeError("Unable to create " + SOUND_CLASS)
    save(SOUND_CLASS)
    unreal.log("AMBIENCE_ASSET created " + SOUND_CLASS)
    return created


def set_if_different(asset, name, value):
    if asset.get_editor_property(name) != value:
        asset.set_editor_property(name, value)
        asset.modify()


def import_wave(source, name, ambience):
    package = BEDS + "/" + name
    digest = sha256(source)
    if library.does_asset_exist(package):
        wave = unreal.load_asset(package)
        if library.get_metadata_tag(wave, HASH_TAG) == digest:
            action = "kept"
        elif not REIMPORT:
            unreal.log_warning("AMBIENCE_STALE {} differs from {}; rerun with DND_AMBIENCE_REIMPORT=1 to replace it".format(package, source))
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
        task.set_editor_property("destination_path", BEDS)
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
    set_if_different(wave, "sound_class_object", ambience)
    set_if_different(wave, "looping", True)
    set_if_different(wave, "virtualization_mode", unreal.VirtualizationMode.PLAY_WHEN_SILENT)
    save(package)
    unreal.log("AMBIENCE_ASSET {} {}".format(action, package))
    return wave


def data_asset():
    if library.does_asset_exist(DATA_ASSET):
        return unreal.load_asset(DATA_ASSET), "kept"
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.ExpeditionAmbienceSounds)
    created = tools.create_asset("DA_ExpeditionAmbience", AUDIO, unreal.ExpeditionAmbienceSounds, factory)
    if not created:
        raise RuntimeError("Unable to create " + DATA_ASSET)
    return created, "created"


def describe(beds_map):
    """A comparable snapshot of the asset's map: bed name -> (loop path, volume)."""
    snapshot = {}
    for key, entry in beds_map.items():
        loop = entry.get_editor_property("loop")
        snapshot[str(key)] = (str(loop.get_path_name()) if loop else "", round(float(entry.get_editor_property("volume")), 4))
    return snapshot


def main():
    with open(os.path.join(SOURCE_DIR, "recipe.json")) as stream:
        recipe = json.load(stream)
    ambience = sound_class()
    desired = unreal.Map(unreal.ExpeditionAmbience, unreal.ExpeditionAmbienceBed)
    for bed in recipe["order"]:
        source = os.path.join(SOURCE_DIR, recipe["output"], bed + ".wav")
        if not os.path.isfile(source):
            raise RuntimeError("Missing rendered bed {}; run tools/audio_beds.py build".format(source))
        entry = unreal.ExpeditionAmbienceBed()
        entry.set_editor_property("loop", import_wave(source, "SW_Amb_" + bed, ambience))
        entry.set_editor_property("volume", float(recipe["pieces"][bed].get("volume", 1.0)))
        desired[enum_value(bed)] = entry
    asset, action = data_asset()
    changed = action == "created"
    if describe(asset.get_editor_property("beds")) != describe(desired):
        asset.set_editor_property("beds", desired)
        changed = True
    if asset.get_editor_property("ambience_class") != ambience:
        asset.set_editor_property("ambience_class", ambience)
        changed = True
    if changed:
        asset.modify()
        action = "updated" if action == "kept" else action
    save(DATA_ASSET)
    unreal.log("AMBIENCE_ASSET {} {}".format(action, DATA_ASSET))
    unreal.log("AMBIENCE_IMPORT_COMPLETE {} beds".format(len(recipe["order"])))


main()
