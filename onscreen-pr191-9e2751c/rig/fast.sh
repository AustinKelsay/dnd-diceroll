#!/usr/bin/env bash
i=0; while [ ! -e "$1/stop" ]; do i=$((i+1)); grim -t jpeg -q 65 -s 0.67 "$1/g$(printf %05d $i).jpg"; done
