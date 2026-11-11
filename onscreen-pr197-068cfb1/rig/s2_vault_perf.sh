#!/usr/bin/env bash
source $(dirname $0)/lib.sh
N=${1:-D}
mkdir -p $P/$N/saves; cp $O/fixtures/board-vault/current-v1.json $P/$N/saves/
SAVEROOT=$P/$N/saves game $N 1920 1080 -novsync -csvCaptureFrames=60000 -csvGpuStats '-ExecCmds=stat unit,stat fps' || exit 1
pid=$(cat $P/$N/pid)
( while kill -0 $pid 2>/dev/null; do echo "$(date +%s) $(grep -E 'VmRSS|VmHWM' /proc/$pid/status | tr -s ' ' | tr '\n' ' ')"; sleep 1; done ) > $P/$N/rss.log 2>&1 &
sleep 3; key -k o; sleep 8
sleep 1
echo "MARK first-wheel-out $(date +%s)" >> $P/$N/marks.log
pw 30; shot $N-wheelout-first 1.5
echo "MARK default-start $(date +%s)" >> $P/$N/marks.log
sleep 3; shot $N-default-1 0.2; sleep 14; shot $N-default-2 0.2; sleep 3
echo "MARK default-end $(date +%s)" >> $P/$N/marks.log
pw -4; sleep 1; pw 60; sleep 2
echo "MARK far-start $(date +%s)" >> $P/$N/marks.log
shot $N-far-1 0.2; sleep 14; shot $N-far-2 0.2; sleep 3
echo "MARK far-end $(date +%s)" >> $P/$N/marks.log
pw -80; sleep 2
echo "MARK near-start $(date +%s)" >> $P/$N/marks.log
shot $N-near-1 0.2; sleep 14; shot $N-near-2 0.2; sleep 3
echo "MARK near-end $(date +%s)" >> $P/$N/marks.log
for i in $(seq 1 60); do grep -a -q "Capture Ended" $P/$N/game.log && break; sleep 5; done
closegame $N
