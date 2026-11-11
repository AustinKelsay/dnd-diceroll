#!/usr/bin/env bash
# runs under flock; stays alive while $1 exists
[ -e "$1" ] || touch "$1"
while [ -e "$1" ]; do sleep 2; done
