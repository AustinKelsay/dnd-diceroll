#!/bin/bash
# usage: unreal_sheets.sh OUT.png SHOT "label=gallery_dir" ... ; builds a labelled side-by-side sheet of one shot (1920x1080 gallery runs).
out=$1; shot=$2; shift 2
files=()
for pair in "$@"; do
  label=${pair%%=*}; dir=${pair#*=}
  files+=(-label "$label" "$dir/board-18x12/1920x1080/$shot.png")
done
montage "${files[@]}" -tile 2x -geometry 960x540+6+6 -pointsize 22 -background '#101014' -fill white "$out"
