#!/usr/bin/env bash
source $(dirname $0)/lib.sh
run() { # name fixture W H
  local n=$1 fx=$2 w=$3 h=$4
  rm -rf $P/$n; mkdir -p $P/$n/saves; cp $O/fixtures/$fx/current-v1.json $P/$n/saves/
  SAVEROOT=$P/$n/saves game $n $w $h || return 1
  sleep 3; key -k o; sleep 8
  pw 6; pw -6; sleep 1   # wiggle so wheel registers
  shot $n-def-0 0.5
  for y in 1 2 3; do key -k e; key -k e; sleep 2.5; shot $n-def-yaw$y 0.2; done
  key -k e; key -k e; sleep 2.5          # back to yaw 0
  pw 60; sleep 3; shot $n-far-0 0.2
  key -k e; key -k e; sleep 2.5; shot $n-far-yaw1 0.2
  key -k e; key -k e; sleep 2.5; shot $n-far-yaw2 0.2
  key -k e; key -k e; sleep 2.5
  pw -80; sleep 3; shot $n-near-0 0.2
  key -k e; key -k e; sleep 2.5; shot $n-near-yaw1 0.2
  closegame $n
}
run V720 board-vault 1280 720
run VIGIL board-vigil-walls 1920 1080
run ROOT board-root-r002 1920 1080
