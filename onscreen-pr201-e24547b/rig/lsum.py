#!/usr/bin/env python3
import sys,csv
rows=list(csv.DictReader(open(sys.argv[1])))
f=lambda k:[float(r[k]) for r in rows]
w=f('white_pct');m=f('mean');p=f('peak')
i=w.index(max(w)); print(f"{sys.argv[1].split('/')[-1]}: n={len(rows)} mean {min(m):.1f}-{max(m):.1f} | peak max {max(p):.1f} | white max {max(w):.2f}% at {rows[i]['frame']} | frames>1% white: {sum(1 for x in w if x>1)} | frames>=254 clip max {max(f('clip_pct')):.2f}%")
