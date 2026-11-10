#!/usr/bin/env bash
# grab a 960x540 jpg every ~1.2 s until the stop file appears
i=0; while [ ! -e "$1/stop" ]; do i=$((i+1)); grim -t jpeg -q 70 -s 0.5 "$1/f$(printf %04d $i).jpg"; sleep 0.9; done
