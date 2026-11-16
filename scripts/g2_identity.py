"""G2 (#241): every CSV of each before/after pair byte-identical. usage: python3 -I g2_identity.py CHECKOUT before:after ..."""
import filecmp, sys
from pathlib import Path
root = Path(sys.argv[1]) / ".local" / "balance"
bad = 0
for pair in sys.argv[2:]:
    a, b = pair.split(":")
    groups = sorted(p.name for p in (root / a).iterdir() if p.is_dir())
    n = 0
    for g in groups:
        for f in ("runs.csv", "encounters.csv", "heroes.csv"):
            same = filecmp.cmp(root / a / g / f, root / b / g / f, shallow=False); n += 1
            if not same: bad += 1; print("DIFF", a, b, g, f)
    print(f"{a} vs {b}: {n} files compared, {'all identical' if not bad else 'differences'}")
sys.exit(1 if bad else 0)
