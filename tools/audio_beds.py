#!/usr/bin/env python3
"""Build and measure long recorded loops and one-shot pieces: the ambience beds (#244) and the music (#245).

It follows the cue pipeline of #242 (tools/audio_cues.py, whose measuring and gain rules it reuses): a recipe lists every
downloaded CC0 source file with its SHA-256 and, for each piece, layers cut from those files (trim, tape-style pitch, filters,
fades, gain, pan, repeats and placements). ``build`` renders each piece with ffmpeg to a 48 kHz stereo float mix and:

  - for a loop, renders ``crossfade`` seconds past the end and folds that tail over the head with an equal-power crossfade, so
    the last sample flows into the first one (no click at the loop point);
  - for a one-shot (a sting), fades the end out;
  - measures integrated loudness (EBU R128) and true peak and applies one gain that reaches the piece's category target without
    passing the true-peak ceiling, then dithers once to 16-bit PCM under the recipe's ``output`` folder.

``measure`` prints the loudness, peak and loop-seam table of WAV files. The seam check compares the jump from the last sample to
the first with the file's own sample-to-sample movement (the 99.9th percentile of adjacent differences) and the level of the
50 ms either side of the seam; a loop fails when the jump is an outlier or the level steps by more than 1.5 dB.

Downloaded files are untrusted data: this tool only hashes them and hands their paths to ffmpeg/ffprobe as arguments.

  python3 tools/audio_beds.py build --recipe Art/audio-ambience/r001/recipe.json --downloads .local/ambience-downloads
  python3 tools/audio_beds.py measure --recipe Art/audio-ambience/r001/recipe.json Art/audio-ambience/r001/beds/*.wav
"""
import argparse
import array
import json
import math
import shutil
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from audio_cues import measure, normalising_gain, sample_peak, sha256  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
RATE = 48000
LAYER_KEYS = {"source", "file", "start", "dur", "at", "gain", "rate", "lowpass", "highpass", "fade_in", "fade_out", "reverse",
              "pan", "repeat", "mono"}
SEAM_PERCENTILE = 99.9
SEAM_WINDOW = 0.05
SEAM_LEVEL_DB = 1.5


def validate_recipe(recipe, names=None):
    """Check the recipe's shape (and, given the names the game declares, that every one has a piece). Returns a list of problems."""
    problems = []
    sources = recipe.get("sources", {})
    categories = recipe.get("categories", {})
    pieces = recipe.get("pieces", {})
    order = recipe.get("order", [])
    if not isinstance(recipe.get("output"), str) or not recipe.get("output"):
        problems.append("output: a folder name is required")
    if sorted(order) != sorted(pieces):
        problems.append("order must list every piece exactly once")
    for name in names or []:
        if name not in pieces:
            problems.append(f"{name}: no piece")
    for name, plan in pieces.items():
        if names is not None and name not in names:
            problems.append(f"{name}: not declared by the game")
        category = categories.get(plan.get("category"))
        if not isinstance(category, dict) or not isinstance(category.get("lufs"), (int, float)):
            problems.append(f"{name}: unknown category {plan.get('category')!r}")
        length = plan.get("length", 0)
        if not (isinstance(length, (int, float)) and length > 0):
            problems.append(f"{name}: length must be positive")
        if plan.get("loop"):
            fade = plan.get("crossfade", 0)
            if not (isinstance(fade, (int, float)) and 0 < fade < length / 2):
                problems.append(f"{name}: a loop needs a crossfade between 0 and half its length")
        if not isinstance(plan.get("volume", 1.0), (int, float)) or not 0 < plan.get("volume", 1.0) <= 2:
            problems.append(f"{name}: volume must be in (0, 2]")
        if not plan.get("layers"):
            problems.append(f"{name}: no layers")
        for index, layer in enumerate(plan.get("layers", [])):
            unknown = set(layer) - LAYER_KEYS
            if unknown:
                problems.append(f"{name}[{index}]: unknown layer keys {sorted(unknown)}")
            source = sources.get(layer.get("source"))
            if source is None:
                problems.append(f"{name}[{index}]: unknown source {layer.get('source')!r}")
            elif layer.get("file") not in source.get("files", {}):
                problems.append(f"{name}[{index}]: {layer.get('file')!r} has no checksum in source {layer.get('source')}")
            if not layer.get("dur", 0) > 0:
                problems.append(f"{name}[{index}]: layer dur must be positive")
            if not -1 <= layer.get("pan", 0) <= 1:
                problems.append(f"{name}[{index}]: pan must be in [-1, 1]")
            if not (isinstance(layer.get("repeat", 1), int) and layer.get("repeat", 1) >= 1):
                problems.append(f"{name}[{index}]: repeat must be a whole number of at least 1")
    return problems


def placements(layer):
    """Where a layer sounds: "at" is one time or a list of times (seconds)."""
    at = layer.get("at", 0.0)
    return list(at) if isinstance(at, (list, tuple)) else [at]


def pan_gains(pan):
    """Constant-power pan: -1 left, 0 centre (-3 dB each side), 1 right."""
    angle = (max(-1.0, min(1.0, pan)) + 1) * math.pi / 4
    return math.cos(angle), math.sin(angle)


def channel_map(channels, layer):
    """The ffmpeg pan filter that turns a source into stereo, panned (mono, or a stereo source folded with "mono": true) or balanced."""
    left, right = pan_gains(layer.get("pan", 0.0))
    if channels == 1 or layer.get("mono"):
        def side(gain):
            gain *= math.sqrt(2)
            return f"{gain:.4f}*c0" if channels == 1 else f"{gain / 2:.4f}*c0+{gain / 2:.4f}*c1"
        return f"pan=stereo|c0={side(left)}|c1={side(right)}"
    # Balance for a stereo source: centre keeps both channels at unity.
    return f"pan=stereo|c0={min(1.0, left * math.sqrt(2)):.4f}*c0|c1={min(1.0, right * math.sqrt(2)):.4f}*c1"


def layer_filter(label, index, layer, channels, sample_rate, at, total):
    rate = float(layer.get("rate", 1.0))
    played = layer["dur"] / rate
    repeat = int(layer.get("repeat", 1))
    chain = [f"[{index}:a]aformat=sample_fmts=fltp", channel_map(channels, layer),
             f"atrim=start={layer.get('start', 0.0)}:duration={layer['dur']}", "asetpts=PTS-STARTPTS"]
    if layer.get("reverse"):
        chain.append("areverse")
    chain.append(f"asetrate={sample_rate * rate:.3f}")
    chain.append(f"aresample={RATE}")
    if repeat > 1:
        chain.append(f"aloop=loop={repeat - 1}:size={int(round(played * RATE))}")
    if layer.get("highpass"):
        chain.append(f"highpass=f={layer['highpass']}")
    if layer.get("lowpass"):
        chain.append(f"lowpass=f={layer['lowpass']}")
    if layer.get("fade_in"):
        chain.append(f"afade=t=in:d={layer['fade_in']}")
    fade_out = layer.get("fade_out", 0.01)
    chain.append(f"afade=t=out:st={max(0.0, played * repeat - fade_out):.4f}:d={fade_out}")
    chain.append(f"volume={layer.get('gain', 0.0)}dB")
    delay = int(round(at * 1000))
    if delay:
        chain.append(f"adelay={delay}:all=1")
    chain.append(f"apad=whole_dur={total}")
    return ",".join(chain) + f"[{label}]"


def probe(path):
    out = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "a:0", "-show_entries", "stream=sample_rate,channels", "-of", "json", str(path)],
                         capture_output=True, text=True, check=True).stdout
    stream = json.loads(out)["streams"][0]
    return int(stream["sample_rate"]), int(stream["channels"])


def fold_loop(samples, channels, length_frames, crossfade_frames):
    """Folds the frames past length_frames over the head with an equal-power crossfade and returns length_frames frames.

    The head's first frame becomes (almost all) the frame that followed the last one in the render, so the loop's end runs straight
    into its start. samples is interleaved; the result is a new interleaved array of the same type.
    """
    if len(samples) < (length_frames + crossfade_frames) * channels:
        raise ValueError("The render is shorter than the loop plus its crossfade")
    out = array.array(samples.typecode, samples[:length_frames * channels])
    for frame in range(crossfade_frames):
        weight = frame / crossfade_frames
        fade_in, fade_out = math.sin(weight * math.pi / 2), math.cos(weight * math.pi / 2)
        for channel in range(channels):
            head = frame * channels + channel
            tail = (length_frames + frame) * channels + channel
            out[head] = samples[head] * fade_in + samples[tail] * fade_out
    return out


def percentile(sorted_values, percent):
    if not sorted_values:
        return 0.0
    index = min(len(sorted_values) - 1, max(0, int(math.ceil(percent / 100 * len(sorted_values))) - 1))
    return sorted_values[index]


def rms_db(values):
    if not values:
        return -math.inf
    power = sum(v * v for v in values) / len(values)
    return 10 * math.log10(power) if power > 0 else -math.inf


def seam_report(samples, channels, rate=RATE):
    """How the end of a loop meets its start, per channel and worst over channels (sample values as read, e.g. int16).

    jump: |first - last|; typical: the 99.9th percentile of |x[i+1] - x[i]| inside the file; ratio: jump / typical (at most 1 means
    the seam moves no more than the material itself does); level_db: the RMS step between the 50 ms before and after the seam.
    """
    frames = len(samples) // channels
    window = max(1, int(rate * SEAM_WINDOW))
    worst = {"jump": 0.0, "typical": 0.0, "ratio": 0.0, "level_db": 0.0}
    for channel in range(channels):
        x = samples[channel::channels]
        diffs = sorted(abs(x[i + 1] - x[i]) for i in range(frames - 1))
        jump = abs(x[0] - x[-1])
        typical = percentile(diffs, SEAM_PERCENTILE)
        ratio = jump / typical if typical > 0 else (0.0 if jump == 0 else math.inf)
        before, after = rms_db(x[-window:]), rms_db(x[:window])
        level = abs(before - after) if math.isfinite(before) and math.isfinite(after) else 0.0
        if ratio >= worst["ratio"]:
            worst.update(jump=float(jump), typical=float(typical), ratio=ratio)
        worst["level_db"] = max(worst["level_db"], level)
    worst["ok"] = worst["ratio"] <= 1.0 and worst["level_db"] <= SEAM_LEVEL_DB
    return worst


def read_wav(path):
    with wave.open(str(path), "rb") as stream:
        if stream.getsampwidth() != 2:
            raise ValueError(f"{path}: expected 16-bit PCM")
        channels, rate = stream.getnchannels(), stream.getframerate()
        data = array.array("h", stream.readframes(stream.getnframes()))
    if sys.byteorder != "little":
        data.byteswap()
    return data, channels, rate


def read_f32(path):
    data = array.array("f", Path(path).read_bytes())
    if sys.byteorder != "little":
        data.byteswap()
    return data


def write_f32(path, data):
    out = array.array("f", data)
    if sys.byteorder != "little":
        out.byteswap()
    Path(path).write_bytes(out.tobytes())


def render(plan, recipe, downloads, workdir):
    """The piece's float mix (raw f32le stereo at 48 kHz): for a loop, its length plus the crossfade tail."""
    length = plan["length"]
    total = length + (plan["crossfade"] if plan.get("loop") else 0.0)
    inputs, filters, labels = [], [], []
    for layer in plan["layers"]:
        source = recipe["sources"][layer["source"]]
        path = downloads / source["folder"] / layer["file"]
        sample_rate, channels = probe(path)
        for at in placements(layer):
            index = len(labels)
            inputs += ["-i", str(path)]
            labels.append(f"l{index}")
            filters.append(layer_filter(labels[-1], index, layer, channels, sample_rate, at, total))
    mix = "".join(f"[{label}]" for label in labels)
    tail = "" if plan.get("loop") else f",afade=t=out:st={max(0.0, length - 0.05):.4f}:d=0.05"
    filters.append(f"{mix}amix=inputs={len(labels)}:normalize=0:duration=longest,atrim=duration={total}{tail}[mix]")
    raw = Path(workdir) / "mix.f32"
    # The graph can be long (one chain per placement), so ffmpeg reads it from a file ("-/option file").
    script = Path(workdir) / "graph.txt"
    script.write_text(";".join(filters))
    subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-loglevel", "error", "-y", *inputs, "-/filter_complex", str(script),
                    "-map", "[mix]", "-ac", "2", "-ar", str(RATE), "-f", "f32le", str(raw)], check=True)
    samples = read_f32(raw)
    if plan.get("loop"):
        samples = fold_loop(samples, 2, int(round(length * RATE)), int(round(plan["crossfade"] * RATE)))
    else:
        samples = samples[:int(round(length * RATE)) * 2]
    folded = Path(workdir) / "piece.f32"
    write_f32(folded, samples)
    wav = Path(workdir) / "piece.wav"
    subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-loglevel", "error", "-y", "-f", "f32le", "-ar", str(RATE), "-ac", "2", "-i", str(folded),
                    "-c:a", "pcm_f32le", str(wav)], check=True)
    return wav


def load_recipe(path):
    recipe_path = Path(path)
    if not recipe_path.is_absolute():
        recipe_path = (ROOT / recipe_path) if (ROOT / recipe_path).is_file() else recipe_path.resolve()
    return recipe_path, json.loads(recipe_path.read_text())


def build(args):
    recipe_path, recipe = load_recipe(args.recipe)
    problems = validate_recipe(recipe)
    if problems:
        raise SystemExit("Recipe problems:\n  " + "\n  ".join(problems))
    downloads = Path(args.downloads).resolve()
    for name, source in recipe["sources"].items():
        for file, digest in source["files"].items():
            path = downloads / source["folder"] / file
            if not path.is_file():
                raise SystemExit(f"Missing download for {name}: {path} (see the source's url in the recipe)")
            if sha256(path) != digest:
                raise SystemExit(f"Checksum mismatch for {name}: {file}")
    out_dir = recipe_path.parent / recipe["output"]
    out_dir.mkdir(exist_ok=True)
    ceiling = recipe["true_peak_ceiling"]
    for name in recipe["order"]:
        if args.only and name not in args.only:
            continue
        plan = recipe["pieces"][name]
        target = recipe["categories"][plan["category"]]["lufs"]
        out_path = out_dir / f"{name}.wav"
        with tempfile.TemporaryDirectory() as workdir:
            raw = render(plan, recipe, downloads, workdir)
            integrated, peak = measure(raw)
            gain = normalising_gain(integrated, peak, target, ceiling)
            subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-loglevel", "error", "-y", "-i", str(raw), "-af",
                            f"volume={gain:.3f}dB,aresample=osf=s16:dither_method=triangular", "-ac", "2", "-ar", str(RATE),
                            "-c:a", "pcm_s16le", "-map_metadata", "-1", "-fflags", "+bitexact", "-flags:a", "+bitexact", str(out_path)], check=True)
        final_i, final_tp = measure(out_path)
        print(f"{out_path.name:24s} target {target:6.1f}  gain {gain:+6.1f} dB  ->  {final_i:6.1f} LUFS  {final_tp:5.1f} dBTP")
    stale = sorted(p.name for p in out_dir.glob("*.wav") if p.stem not in recipe["pieces"])
    if stale:
        print("Not in the recipe (remove deliberately): " + ", ".join(stale))


def measure_command(args):
    _, recipe = load_recipe(args.recipe)
    ceiling = recipe["true_peak_ceiling"]
    print("| File | Target LUFS | Integrated LUFS | True peak dBTP | Sample peak dBFS | Length s | Loop seam: jump / p99.9 step | Seam level step dB |")
    print("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    failures = 0
    for name in args.paths:
        path = Path(name)
        plan = recipe["pieces"].get(path.stem, {})
        target = recipe["categories"].get(plan.get("category"), {}).get("lufs")
        integrated, peak = measure(path)
        over = peak > ceiling + 0.05
        failures += over
        samples, channels, rate = read_wav(path)
        seam, level = "one-shot", ""
        if plan.get("loop"):
            report = seam_report(samples, channels, rate)
            failures += not report["ok"]
            seam = f"{report['jump']:.0f} / {report['typical']:.0f} = {report['ratio']:.2f}{'' if report['ok'] else ' FAIL'}"
            level = f"{report['level_db']:.2f}"
        print(f"| {path.name} | {target if target is not None else '?'} | {integrated:.1f} | {peak:.1f}{' OVER' if over else ''} | "
              f"{sample_peak(path):.1f} | {len(samples) / channels / rate:.2f} | {seam} | {level} |")
    if failures:
        raise SystemExit(f"{failures} problem(s): a true peak above {ceiling} dBTP or a loop seam outside the limits")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)
    b = sub.add_parser("build", help="Render every piece of a recipe")
    b.add_argument("--recipe", required=True)
    b.add_argument("--downloads", required=True, help="Folder holding one subfolder per recipe source")
    b.add_argument("--only", nargs="*", help="Render only these pieces")
    m = sub.add_parser("measure", help="Print the loudness, peak and loop-seam table of WAV files")
    m.add_argument("--recipe", required=True)
    m.add_argument("paths", nargs="+")
    args = parser.parse_args(argv)
    for tool in ("ffmpeg", "ffprobe"):
        if not shutil.which(tool):
            raise SystemExit(f"{tool} is required on PATH")
    build(args) if args.command == "build" else measure_command(args)


if __name__ == "__main__":
    main()
