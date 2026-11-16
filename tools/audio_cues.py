#!/usr/bin/env python3
"""Build and measure the recorded expedition cues (#242).

The recipe (Art/audio-sources/r001/recipe.json) lists every downloaded CC0 source file with its SHA-256 and, for each
EExpeditionCue, the variations as layers cut from those files: trim, pitch, filters, fades, gain and placement. ``build``
renders each variation with ffmpeg to a 48 kHz mono float mix, optionally shaves its loudest transients with a
look-ahead limiter (at most the category's "limit_db", so a short hit can reach the loudness of a sustained cue), measures
integrated loudness (EBU R128) and true peak, applies one gain that reaches the category target without passing the
true-peak ceiling, and writes 16-bit WAVs under Art/audio-sources/r001/cues/. ``measure`` prints the loudness and peak table of WAV files.

Downloaded files are untrusted data: this tool only hashes them and hands their paths to ffmpeg/ffprobe as arguments.
It needs ffmpeg and ffprobe on PATH; the parsing and gain rules are plain functions with unit tests.

  python3 tools/audio_cues.py build --downloads .local/audio-downloads
  python3 tools/audio_cues.py measure Art/audio-sources/r001/cues/*.wav
"""
import argparse
import hashlib
import json
import math
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECIPE = ROOT / "Art" / "audio-sources" / "r001" / "recipe.json"
LAYER_KEYS = {"source", "file", "start", "dur", "at", "gain", "rate", "lowpass", "highpass", "fade_in", "fade_out", "reverse"}
SILENT = -70.0  # the EBU R128 absolute gate: anything at or below it measured nothing


def parse_ebur128(stderr):
    """Integrated loudness (LUFS) and true peak (dBTP) from the summary ffmpeg's ebur128=peak=true filter prints last."""
    summary = stderr[stderr.rfind("Summary:"):] if "Summary:" in stderr else ""
    loud = re.search(r"Integrated loudness:\s*I:\s*(-?[\d.]+|-inf|nan)\s*LUFS", summary, re.S)
    peak = re.search(r"True peak:\s*Peak:\s*(-?[\d.]+|-inf|nan)\s*dBFS", summary, re.S)
    if not loud or not peak:
        raise ValueError("No ebur128 summary in ffmpeg output")
    value = lambda text: -math.inf if text in ("-inf", "nan") else float(text)
    return value(loud.group(1)), value(peak.group(1))


def normalising_gain(integrated, true_peak, target, ceiling):
    """The one gain (dB) that brings a mix to its loudness target, capped so its true peak stays at or under the ceiling."""
    if integrated <= SILENT or true_peak == -math.inf:
        raise ValueError("The mix is silent; check the recipe's trims")
    return min(target - integrated, ceiling - true_peak)


def limiter_filter(true_peak, limit_db):
    """Bring the mix's peak to full scale, then let the limiter take at most limit_db off the transients (no auto make-up gain)."""
    if limit_db <= 0:
        return None
    limit = max(10 ** (-limit_db / 20), 0.0625)
    return f"volume={-true_peak:.3f}dB,alimiter=limit={limit:.5f}:attack=1:release=60:level=false:latency=true"


def validate_recipe(recipe, cues):
    """Check the recipe's shape against the cue names the game declares. Returns a list of problems (empty when valid)."""
    problems = []
    sources = recipe.get("sources", {})
    categories = recipe.get("categories", {})
    declared = recipe.get("cues", {})
    for cue in cues:
        if cue not in declared:
            problems.append(f"{cue}: no recipe")
    for cue, plan in declared.items():
        if cue not in cues:
            problems.append(f"{cue}: not an EExpeditionCue")
        category = categories.get(plan.get("category"))
        if not isinstance(category, dict) or not isinstance(category.get("lufs"), (int, float)) or category.get("limit_db", 0) < 0:
            problems.append(f"{cue}: unknown category {plan.get('category')!r}")
        variations = plan.get("variations", [])
        if not 1 <= len(variations) <= 4:
            problems.append(f"{cue}: {len(variations)} variations (1-4 allowed)")
        for index, variation in enumerate(variations):
            if not variation.get("length", 0) > 0:
                problems.append(f"{cue}[{index}]: length must be positive")
            if not variation.get("layers"):
                problems.append(f"{cue}[{index}]: no layers")
            for layer in variation.get("layers", []):
                unknown = set(layer) - LAYER_KEYS
                if unknown:
                    problems.append(f"{cue}[{index}]: unknown layer keys {sorted(unknown)}")
                source = sources.get(layer.get("source"))
                if source is None:
                    problems.append(f"{cue}[{index}]: unknown source {layer.get('source')!r}")
                elif layer.get("file") not in source.get("files", {}):
                    problems.append(f"{cue}[{index}]: {layer.get('file')!r} has no checksum in source {layer.get('source')}")
                if not layer.get("dur", 0) > 0:
                    problems.append(f"{cue}[{index}]: layer dur must be positive")
    return problems


def output_name(cue, index):
    return f"{cue}_{index + 1:02d}.wav"


def layer_filter(index, layer, channels, sample_rate, length):
    rate = float(layer.get("rate", 1.0))
    # asetrate reinterprets the samples at a new rate (pitch and speed together, like tape), then aresample returns to 48 kHz.
    played = layer["dur"] / rate
    chain = [f"[{index}:a]aformat=sample_fmts=fltp",
             "pan=mono|c0=c0" if channels == 1 else "pan=mono|c0=0.5*c0+0.5*c1",
             f"atrim=start={layer.get('start', 0.0)}:duration={layer['dur']}", "asetpts=PTS-STARTPTS"]
    if layer.get("reverse"):
        chain.append("areverse")
    chain.append(f"asetrate={sample_rate * rate:.3f}")
    chain.append("aresample=48000")
    if layer.get("highpass"):
        chain.append(f"highpass=f={layer['highpass']}")
    if layer.get("lowpass"):
        chain.append(f"lowpass=f={layer['lowpass']}")
    if layer.get("fade_in"):
        chain.append(f"afade=t=in:d={layer['fade_in']}")
    fade_out = layer.get("fade_out", 0.01)
    chain.append(f"afade=t=out:st={max(0.0, played - fade_out):.4f}:d={fade_out}")
    chain.append(f"volume={layer.get('gain', 0.0)}dB")
    delay = int(round(layer.get("at", 0.0) * 1000))
    if delay:
        chain.append(f"adelay={delay}")
    chain.append(f"apad=whole_dur={length}")
    return ",".join(chain) + f"[l{index}]"


def probe(path):
    out = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "stream=sample_rate,channels", "-of", "json", str(path)],
                         capture_output=True, text=True, check=True).stdout
    stream = json.loads(out)["streams"][0]
    return int(stream["sample_rate"]), int(stream["channels"])


def measure(path):
    """Integrated loudness and true peak of a file, padded with a second of silence so a sub-400 ms cue fills a gating block."""
    result = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", str(path), "-af", "apad=pad_dur=1,ebur128=peak=true",
                             "-f", "null", "-"], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"ffmpeg could not measure {path}: {result.stderr[-400:]}")
    return parse_ebur128(result.stderr)


def sample_peak(path):
    result = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", str(path), "-af", "astats=measure_overall=Peak_level:measure_perchannel=none",
                             "-f", "null", "-"], capture_output=True, text=True)
    found = re.findall(r"Peak level dB:\s*(-?[\d.]+|-inf)", result.stderr)
    return float(found[-1]) if found and found[-1] != "-inf" else -math.inf


def duration(path):
    return float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", str(path)],
                                capture_output=True, text=True, check=True).stdout)


def sha256(path):
    with open(path, "rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def render(variation, recipe, downloads, out_path, workdir):
    layers = variation["layers"]
    length = variation["length"]
    inputs, filters = [], []
    for index, layer in enumerate(layers):
        source = recipe["sources"][layer["source"]]
        path = downloads / source["folder"] / layer["file"]
        rate, channels = probe(path)
        inputs += ["-i", str(path)]
        filters.append(layer_filter(index, layer, channels, rate, length))
    mix = "".join(f"[l{i}]" for i in range(len(layers)))
    tail = min(0.02, length / 4)
    filters.append(f"{mix}amix=inputs={len(layers)}:normalize=0:duration=longest,atrim=duration={length},"
                   f"afade=t=out:st={length - tail:.4f}:d={tail}[mix]")
    raw = Path(workdir) / "mix.wav"
    subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-loglevel", "error", "-y", *inputs, "-filter_complex", ";".join(filters),
                    "-map", "[mix]", "-ac", "1", "-ar", "48000", "-c:a", "pcm_f32le", str(raw)], check=True)
    return raw


def build(args):
    recipe = json.loads(RECIPE.read_text())
    cues = recipe["cue_order"]
    problems = validate_recipe(recipe, cues)
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
    out_dir = RECIPE.parent / "cues"
    out_dir.mkdir(exist_ok=True)
    ceiling = recipe["true_peak_ceiling"]
    report = {}
    for cue in cues:
        if args.only and cue not in args.only:
            continue
        plan = recipe["cues"][cue]
        category = recipe["categories"][plan["category"]]
        target = category["lufs"]
        for index, variation in enumerate(plan["variations"]):
            out_path = out_dir / output_name(cue, index)
            with tempfile.TemporaryDirectory() as workdir:
                raw = render(variation, recipe, downloads, out_path, workdir)
                limiter = limiter_filter(measure(raw)[1], category.get("limit_db", 0))
                if limiter:
                    limited = Path(workdir) / "limited.wav"
                    subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-loglevel", "error", "-y", "-i", str(raw), "-af", limiter,
                                    "-c:a", "pcm_f32le", str(limited)], check=True)
                    raw = limited
                integrated, peak = measure(raw)
                gain = normalising_gain(integrated, peak, target, ceiling)
                # Dither once, on the single conversion to 16-bit.
                subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-loglevel", "error", "-y", "-i", str(raw), "-af",
                                f"volume={gain:.3f}dB,aresample=osf=s16:dither_method=triangular", "-ac", "1", "-ar", "48000",
                                "-c:a", "pcm_s16le", "-map_metadata", "-1", "-fflags", "+bitexact", "-flags:a", "+bitexact", str(out_path)],
                               check=True)
            final_i, final_tp = measure(out_path)
            report[out_path.name] = {"target": target, "gain": round(gain, 2), "lufs": round(final_i, 1), "true_peak": round(final_tp, 1)}
            print(f"{out_path.name:20s} target {target:6.1f}  gain {gain:+6.1f} dB  ->  {final_i:6.1f} LUFS  {final_tp:5.1f} dBTP")
    stale = sorted(p.name for p in out_dir.glob("*.wav")
                   if not any(p.name == output_name(c, i) for c in cues for i in range(len(recipe["cues"][c]["variations"]))))
    if stale:
        print("Not in the recipe (remove deliberately): " + ", ".join(stale))
    return report


def measure_command(args):
    recipe = json.loads(RECIPE.read_text())
    ceiling = recipe["true_peak_ceiling"]
    targets = {cue: recipe["categories"][plan["category"]]["lufs"] for cue, plan in recipe["cues"].items()}
    print("| File | Cue | Target LUFS | Integrated LUFS | True peak dBTP | Sample peak dBFS | Length s |")
    print("| --- | --- | ---: | ---: | ---: | ---: | ---: |")
    failures = 0
    for name in args.paths:
        path = Path(name)
        integrated, peak = measure(path)
        cue = path.stem.rsplit("_", 1)[0]
        target = targets.get(cue)
        over = peak > ceiling + 0.05
        failures += over
        print(f"| {path.name} | {cue} | {target if target is not None else '?'} | {integrated:.1f} | {peak:.1f}{' OVER' if over else ''} | "
              f"{sample_peak(path):.1f} | {duration(path):.2f} |")
    if failures:
        raise SystemExit(f"{failures} file(s) above the {ceiling} dBTP ceiling")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)
    b = sub.add_parser("build", help="Render every cue variation from the recipe")
    b.add_argument("--downloads", required=True, help="Folder holding one subfolder per recipe source")
    b.add_argument("--only", nargs="*", help="Render only these cues")
    m = sub.add_parser("measure", help="Print the loudness and peak table of WAV files")
    m.add_argument("paths", nargs="+")
    args = parser.parse_args(argv)
    for tool in ("ffmpeg", "ffprobe"):
        if not shutil.which(tool):
            raise SystemExit(f"{tool} is required on PATH")
    build(args) if args.command == "build" else measure_command(args)


if __name__ == "__main__":
    main()
