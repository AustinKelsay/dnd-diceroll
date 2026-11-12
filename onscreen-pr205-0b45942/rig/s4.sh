#!/usr/bin/env bash
source /home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-641aa874-26d2aed68b01116a/.local/onscreen/lib2.sh
two() { # NAME SRC LOOKS : default + zoom shots
  fxscene $1 $2 1920 1080 $3 | cut -c1-60; snap $1-default; pw -3; sleep 1.5; snap $1-zoom1; closegame $1; }
two d-vigil-b $O/fixtures/D-vigil-b/current-v1.json Brute,Warden,Archer
two d-vigil-a $O/fixtures/D-vigil-a/current-v1.json Scout,Skirmisher,Archer
two d-vault-finale $O/fixtures/D-vault-finale/current-v1.json Brute,Warden,Archer
two d-root-b $O/fixtures/D-root-b/current-v1.json Brute,Archer
two iso-vigil-b $R/Saved/CaptureFixtures/foes-vigil-b/current-v1.json -
two iso-vault-finale $R/Saved/CaptureFixtures/foes-vault-finale/current-v1.json -
two iso-root-b $R/Saved/CaptureFixtures/foes-root-b/current-v1.json -
two reg-default $R/Saved/CaptureFixtures/default-seed-battle/current-v1.json -
two reg-vault-raiders $R/Saved/CaptureFixtures/board-vault-raiders/current-v1.json -
# brute swing / enemy turn
fxscene swing $R/Saved/CaptureFixtures/foes-vigil-b/current-v1.json 1920 1080 Brute,Warden,Archer | cut -c1-60
mkdir -p $P/bursts/swing; wtype -s 250 -k Escape; wtype -s 250 -k space; sleep 0.5
for i in $(seq -w 1 420); do grim -g "500,100 1100x700" -t jpeg -q 82 $P/bursts/swing/f$i.jpg; done
snap swing-end; closegame swing
echo S4-DONE
