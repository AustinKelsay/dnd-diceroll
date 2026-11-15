"""Import the score into /Game/DnDDiceRoll/Audio (issue #245).

Run headless with the editor closed (under the Ultra machine lock):
  UnrealEditor-Cmd DnDDiceRoll.uproject -run=pythonscript -script=<repo>/Content/Python/import_expedition_music.py -unattended -nullrhi -corelimit=2

Reads Art/audio-music/r001/recipe.json (piece order, loops and volumes) and the 16-bit 48 kHz stereo pieces that tools/audio_beds.py
rendered beside it in music/, then:
  - creates /Game/DnDDiceRoll/Audio/SC_Music, the sound class every track and sting plays through (#246 builds the mix on it);
  - imports each <Piece>.wav as /Game/DnDDiceRoll/Audio/Music/SW_Mus_<Piece>, tagged DiceRollSourceSha256 with the WAV's hash, in SC_Music;
    a track loops and keeps playing while silent (it fades in from zero), a sting plays once;
  - creates or updates /Game/DnDDiceRoll/Audio/DA_ExpeditionMusic (UExpeditionMusicSounds): each track's and sting's wave and volume.
It never touches the cue or ambience assets. Reruns preserve existing assets: a wave whose tag matches its WAV is left alone; one whose
WAV changed is reported (MUSIC_STALE) and replaced only when DND_MUSIC_REIMPORT=1. Properties are written only when they differ,
only dirty packages are saved, and a failed save raises. The last line is MUSIC_IMPORT_COMPLETE <tracks> tracks, <stings> stings.
"""
import hashlib
import json
import os
import re

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SOURCE_DIR = os.path.join(ROOT, "Art", "audio-music", "r001")
AUDIO = "/Game/DnDDiceRoll/Audio"
PIECES = AUDIO + "/Music"
SOUND_CLASS = AUDIO + "/SC_Music"
DATA_ASSET = AUDIO + "/DA_ExpeditionMusic"
HASH_TAG = "DiceRollSourceSha256"
REIMPORT = os.environ.get("DND_MUSIC_REIMPORT") == "1"

library = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def sha256(path):
    with open(path, "rb") as stream:
        return hashlib.sha256(stream.read()).hexdigest()


def enum_value(enum, name):
    """EExpeditionMusic::CombatA is unreal.ExpeditionMusic.COMBAT_A in Python."""
    return getattr(enum, re.sub(r"(?<!^)(?=[A-Z])", "_", name).upper())


def save(path):
    if not library.save_asset(path, only_if_is_dirty=True):
        raise RuntimeError("Unable to save " + path)


def sound_class():
    if library.does_asset_exist(SOUND_CLASS):
        unreal.log("MUSIC_ASSET kept " + SOUND_CLASS)
        return unreal.load_asset(SOUND_CLASS)
    created = tools.create_asset("SC_Music", AUDIO, unreal.SoundClass, unreal.SoundClassFactory())
    if not created:
        raise RuntimeError("Unable to create " + SOUND_CLASS)
    save(SOUND_CLASS)
    unreal.log("MUSIC_ASSET created " + SOUND_CLASS)
    return created


def set_if_different(asset, name, value):
    if asset.get_editor_property(name) != value:
        asset.set_editor_property(name, value)
        asset.modify()


def import_wave(source, name, music, looping):
    package = PIECES + "/" + name
    digest = sha256(source)
    if library.does_asset_exist(package):
        wave = unreal.load_asset(package)
        if library.get_metadata_tag(wave, HASH_TAG) == digest:
            action = "kept"
        elif not REIMPORT:
            unreal.log_warning("MUSIC_STALE {} differs from {}; rerun with DND_MUSIC_REIMPORT=1 to replace it".format(package, source))
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
        task.set_editor_property("destination_path", PIECES)
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
    set_if_different(wave, "sound_class_object", music)
    set_if_different(wave, "looping", looping)
    if looping:
        set_if_different(wave, "virtualization_mode", unreal.VirtualizationMode.PLAY_WHEN_SILENT)
    save(package)
    unreal.log("MUSIC_ASSET {} {}".format(action, package))
    return wave


def data_asset():
    if library.does_asset_exist(DATA_ASSET):
        return unreal.load_asset(DATA_ASSET), "kept"
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.ExpeditionMusicSounds)
    created = tools.create_asset("DA_ExpeditionMusic", AUDIO, unreal.ExpeditionMusicSounds, factory)
    if not created:
        raise RuntimeError("Unable to create " + DATA_ASSET)
    return created, "created"


def describe(pieces_map):
    """A comparable snapshot of a map: key name -> (wave path, volume)."""
    snapshot = {}
    for key, entry in pieces_map.items():
        wave = entry.get_editor_property("wave")
        snapshot[str(key)] = (str(wave.get_path_name()) if wave else "", round(float(entry.get_editor_property("volume")), 4))
    return snapshot


STINGS = ("Victory", "Defeat")


def main():
    with open(os.path.join(SOURCE_DIR, "recipe.json")) as stream:
        recipe = json.load(stream)
    music = sound_class()
    tracks = unreal.Map(unreal.ExpeditionMusic, unreal.ExpeditionMusicPiece)
    stings = unreal.Map(unreal.ExpeditionSting, unreal.ExpeditionMusicPiece)
    for name in recipe["order"]:
        plan = recipe["pieces"][name]
        source = os.path.join(SOURCE_DIR, recipe["output"], name + ".wav")
        if not os.path.isfile(source):
            raise RuntimeError("Missing rendered piece {}; run tools/audio_beds.py build".format(source))
        entry = unreal.ExpeditionMusicPiece()
        entry.set_editor_property("wave", import_wave(source, "SW_Mus_" + name, music, bool(plan.get("loop"))))
        entry.set_editor_property("volume", float(plan.get("volume", 1.0)))
        if name in STINGS:
            stings[enum_value(unreal.ExpeditionSting, name)] = entry
        else:
            tracks[enum_value(unreal.ExpeditionMusic, name)] = entry
    asset, action = data_asset()
    changed = action == "created"
    if describe(asset.get_editor_property("tracks")) != describe(tracks):
        asset.set_editor_property("tracks", tracks)
        changed = True
    if describe(asset.get_editor_property("stings")) != describe(stings):
        asset.set_editor_property("stings", stings)
        changed = True
    if asset.get_editor_property("music_class") != music:
        asset.set_editor_property("music_class", music)
        changed = True
    if changed:
        asset.modify()
        action = "updated" if action == "kept" else action
    save(DATA_ASSET)
    unreal.log("MUSIC_ASSET {} {}".format(action, DATA_ASSET))
    unreal.log("MUSIC_IMPORT_COMPLETE {} tracks, {} stings".format(len(tracks), len(stings)))


main()
