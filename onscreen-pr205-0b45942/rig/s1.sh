#!/usr/bin/env bash
source /home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-641aa874-26d2aed68b01116a/.local/onscreen/lib2.sh
for fx in foes-vigil-a foes-vigil-b foes-root-a foes-root-b foes-root-c foes-vault-a foes-vault-finale; do
  L=$(looksof $fx)
  fxscene s1-$fx $R/Saved/CaptureFixtures/$fx/current-v1.json 1920 1080 $L || { echo "FAIL $fx"; continue; }
  snap s1-$fx-default
  pw -3; sleep 1.5; snap s1-$fx-zoom1
  pw -3; sleep 1.5; snap s1-$fx-zoom2
  closegame s1-$fx
done
L=$(looksof foes-vigil-b)
fxscene s1-p720 $R/Saved/CaptureFixtures/foes-vigil-b/current-v1.json 1280 720 $L && { snap s1-p720-default; pw -3; sleep 1.5; snap s1-p720-zoom1; closegame s1-p720; }
echo S1-DONE
