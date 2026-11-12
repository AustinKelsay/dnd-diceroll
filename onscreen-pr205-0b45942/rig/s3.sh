#!/usr/bin/env bash
# s3.sh NAME SCRIPT : a real autoplayed run (no override) with a jpeg frame loop on the real display
source /home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-641aa874-26d2aed68b01116a/.local/onscreen/lib.sh
n=$1; S=$2
D0=$P/$n; rm -rf $D0; mkdir -p $D0/saves $D0/runtime $D0/frames
hyprctl keyword monitor HEADLESS-1,1920x1080@60,0x0,1 >/dev/null; sleep 1
for try in 1 2 3; do
  setsid "$BIN" -windowed -ForceRes -ResX=1920 -ResY=1080 -unattended -nosplash -corelimit=2 -novsync -UserDir=$D0/runtime -ExpeditionSaveRoot=$D0/saves -ExpeditionAutoplay=$S -abslog=$D0/game.log -stdout -FullStdOutLogOutput > $D0/stdout.log 2>&1 &
  sleep 12; pid=$(pgrep -f "abslog=$D0/game.log" | tail -1)
  if [ -n "$pid" ] && ps -p $pid >/dev/null; then break; fi
  echo "startup race try $try"; pid=""
done
[ -z "$pid" ] && { echo FAILED-LAUNCH; exit 1; }
i=0; t0=$(date +%s)
while ps -p $pid >/dev/null && [ $(( $(date +%s) - t0 )) -lt 200 ]; do
  i=$((i+1)); grim -o HEADLESS-1 -t jpeg -q 72 $D0/frames/f$(printf %05d $i).jpg
done
ps -p $pid >/dev/null && { a=$(hyprctl clients -j | python3 -c "import json,sys;p=int('$pid');print(([c['address'] for c in json.load(sys.stdin) if c['pid']==p] or [''])[0])"); hyprctl dispatch closewindow address:$a >/dev/null; sleep 8; }
ps -p $pid >/dev/null && echo STILL-RUNNING || echo exited; echo frames $i
grep -h 'AUTOPLAY_DONE\|AUTOPLAY_PLAN\|AUTOPLAY_FAIL\|AUTOPLAY_BEAT' $D0/game.log | cut -c1-160 | head -30; echo "errors: $(grep -c 'Error:' $D0/game.log)"
