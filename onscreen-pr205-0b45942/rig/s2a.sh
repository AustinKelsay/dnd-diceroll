#!/usr/bin/env bash
source /home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-641aa874-26d2aed68b01116a/.local/onscreen/lib2.sh
fxscene s2a $R/Saved/CaptureFixtures/foes-root-a/current-v1.json 1920 1080 - || exit 1
snap s2a-start
wtype -k space; sleep 1.2; wtype -k space; sleep 1.2
mkdir -p $P/s2a/burst
( sleep 0.4; wtype -k space ) >/dev/null 2>&1 &
for i in $(seq -w 1 110); do grim -o HEADLESS-1 -t jpeg -q 85 $P/s2a/burst/f$i.jpg; done; wait
python3 $O/drv.py $SAVE state | cut -c1-250
closegame s2a
echo S2A-DONE
