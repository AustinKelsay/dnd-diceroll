#!/usr/bin/env bash
# crop.sh IN.png OUT.png : crop around the party's green health bars (native pixels, +/- margin)
IN=$1; OUT=$2
BB=$(magick "$IN" -crop 1590x880+330+90 +repage -fuzz 2% -fill red -opaque "srgb(160,237,169)" -fill black +opaque red -trim -format '%@' info:)
python3 - "$BB" "$IN" "$OUT" <<'PY'
import sys,subprocess,re
w,h,x,y=map(int,re.match(r'(\d+)x(\d+)\+(-?\d+)\+(-?\d+)',sys.argv[1]).groups()); x+=330; y+=90
X0=max(x-40,0); Y0=max(y-30,0); X1=min(x+w+60,1919); Y1=min(y+h+150,1079)
subprocess.run(['magick',sys.argv[2],'-crop',f'{X1-X0}x{Y1-Y0}+{X0}+{Y0}','+repage',sys.argv[3]])
print('crop',sys.argv[3],X0,Y0,X1-X0,Y1-Y0)
PY
