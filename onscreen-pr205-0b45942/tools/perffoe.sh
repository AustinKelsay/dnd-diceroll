#!/usr/bin/env bash
# perffoe.sh NAME FIXTURE LOOKS : locked foe-fixture perf session (CSV profiler), 1920x1080, hold then play. Run under flock.
source /home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-641aa874-26d2aed68b01116a/.local/onscreen/lib.sh
n=$1; fx=$2; looks=$3
D0=$P/$n; rm -rf $D0; mkdir -p $D0/saves $D0/runtime
cp $R/Saved/CaptureFixtures/$fx/current-v1.json $D0/saves/
hyprctl keyword monitor HEADLESS-1,1920x1080@60,0x0,1 >/dev/null; sleep 1
for try in 1 2 3; do
  T0=$(date +%s.%N)
  setsid "$BIN" -windowed -ForceRes -ResX=1920 -ResY=1080 -unattended -nosplash -corelimit=2 -novsync -csvCaptureFrames=60000 -csvGpuStats -UserDir=$D0/runtime -ExpeditionSaveRoot=$D0/saves -ExpeditionFoeLooks=$looks -abslog=$D0/game.log -stdout -FullStdOutLogOutput > $D0/stdout.log 2>&1 &
  sleep 20; pid=$(pgrep -f "abslog=$D0/game.log" | tail -1)
  if [ -n "$pid" ] && ps -p $pid >/dev/null; then break; fi
  echo "startup race try $try"; cp $D0/game.log $D0/game-crash-try$try.log 2>/dev/null; pid=""
done
[ -z "$pid" ] && { echo FAILED-LAUNCH; exit 1; }
echo $pid > $D0/pid; echo "T0=$T0" > $D0/times.txt
wtype -s 300 o; echo "RESUME=$(date +%s.%N)" >> $D0/times.txt; sleep 10
echo "HOLD_START=$(date +%s.%N)" >> $D0/times.txt; sleep 45; echo "HOLD_END=$(date +%s.%N)" >> $D0/times.txt
echo "PLAY_START=$(date +%s.%N)" >> $D0/times.txt
for i in $(seq 1 25); do wtype -k space; sleep 3; done
echo "PLAY_END=$(date +%s.%N)" >> $D0/times.txt
grep -E "VmHWM|VmRSS" /proc/$pid/status > $D0/rss.txt
a=$(hyprctl clients -j | python3 -c "import json,sys;p=int('$pid');print(([c['address'] for c in json.load(sys.stdin) if c['pid']==p] or [''])[0])")
hyprctl dispatch closewindow address:$a >/dev/null; sleep 10; ps -p $pid >/dev/null && echo STILL-RUNNING || echo closed
echo "errors: $(grep -c 'Error:' $D0/game.log)"
find $D0/runtime -name '*.csv' | head
