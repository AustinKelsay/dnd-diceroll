#!/usr/bin/env bash
# rburst PREFIX COUNT GEOM : COUNT region grabs (grim -g GEOM) as fast as possible, logging ms offsets
t0=$(date +%s%N); for i in $(seq 1 $2); do grim -g "$3" $1$i.png; echo "$i $(( ($(date +%s%N)-t0)/1000000 ))" >> $1times.txt; done
